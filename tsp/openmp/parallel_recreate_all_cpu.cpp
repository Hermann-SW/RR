/* NOLINT(legal/copyright)
f=parallel_recreate_all_cpu
g++-14 -O3 -march=znver5 -fopenmp -Wall -Wextra -pedantic $f.cpp -o $f
*/
#include <omp.h>
#include <immintrin.h>
#include <string.h>

#include <iostream>
#include <iomanip>
#include <vector>
#include <numeric>
#include <algorithm>
#include <random>
#include <cstdint>
#include <climits>
#include <cmath>
#include <chrono>  // NOLINT [build/c++11]

#include "../loader.h"

std::vector<coord_t> Coords;

constexpr uint32_t NUM_CITIES = 100000;
constexpr int NUM_RUNS = 25;  // 100;

// AVX-512 SIMD helper to calculate 8 Euclidean distance values at once
inline void calc_dist_8x_avx512(
    const double ux, const double uy,
    const double* __restrict px, const double* __restrict py,
    double* __restrict out_dist) {
    __m512d v_ux = _mm512_set1_pd(ux);
    __m512d v_uy = _mm512_set1_pd(uy);

    __m512d v_px = _mm512_loadu_pd(px);
    __m512d v_py = _mm512_loadu_pd(py);

    __m512d dx = _mm512_sub_pd(v_px, v_ux);
    __m512d dy = _mm512_sub_pd(v_py, v_uy);

    __m512d dist_sq = _mm512_fmadd_pd(dx, dx, _mm512_mul_pd(dy, dy));
    __m512d dist = _mm512_sqrt_pd(dist_sq);

    _mm512_storeu_pd(out_dist, dist);
}

// Single tour evaluation optimized with OpenMP SIMD / AVX-512
uint64_t run_single_ruin_recreate(
    const double (* __restrict C_coords)[2],
    const std::vector<uint32_t>& city_order,
    uint32_t* __restrict tour_A,
    uint32_t* __restrict tour_B) {
    // Initialize first 3 cities
    tour_A[0] = city_order[0];
    tour_A[1] = city_order[1];
    tour_A[2] = city_order[2];

    uint32_t* d_curr = tour_A;
    uint32_t* d_next = tour_B;

    // Progressively insert remaining cities
    for (uint32_t k = 3; k < NUM_CITIES; ++k) {
        uint32_t u = city_order[k];
        double ux = C_coords[u][0];
        double uy = C_coords[u][1];

        int64_t min_cost = LLONG_MAX;
        uint32_t best_i = 0;

        // Auto-vectorized SIMD loop searching for minimum extra insertion cost
        #pragma omp simd reduction(min:min_cost)
        for (uint32_t i = 0; i < k; ++i) {
            uint32_t p = d_curr[i];
            uint32_t v = (i + 1 == k) ? d_curr[0] : d_curr[i + 1];

            double px = C_coords[p][0];
            double py = C_coords[p][1];

            double vx = C_coords[v][0];
            double vy = C_coords[v][1];

            double d_pu = std::sqrt((px - ux)*(px - ux) + (py - uy)*(py - uy));
            double d_uv = std::sqrt((ux - vx)*(ux - vx) + (uy - vy)*(uy - vy));
            double d_pv = std::sqrt((px - vx)*(px - vx) + (py - vy)*(py - vy));

            int64_t extra_cost = static_cast<int64_t>(
                static_cast<uint16_t>(0.5 + d_pu) +
                static_cast<uint16_t>(0.5 + d_uv) -
                static_cast<uint16_t>(0.5 + d_pv));

            if (extra_cost < min_cost) {
                min_cost = extra_cost;
                best_i = i;
            }
        }

        // Insert new city into tour buffer
        for (uint32_t i = 0; i <= best_i; ++i) {
            d_next[i] = d_curr[i];
        }
        d_next[best_i + 1] = u;
        for (uint32_t i = best_i + 1; i < k; ++i) {
            d_next[i + 1] = d_curr[i];
        }

        std::swap(d_curr, d_next);
    }

    // Compute final rounded tour length
    uint64_t tour_length = 0;
    #pragma omp simd reduction(+:tour_length)
    for (uint32_t i = 0; i < NUM_CITIES; ++i) {
        uint32_t u = d_curr[i];
        uint32_t v = (i + 1 == NUM_CITIES) ? d_curr[0] : d_curr[i + 1];

        double x1 = C_coords[u][0];
        double y1 = C_coords[u][1];
        double x2 = C_coords[v][0];
        double y2 = C_coords[v][1];

        double xd = x1 - x2;
        double yd = y1 - y2;

        uint16_t dist = static_cast<uint16_t>(0.5 + std::sqrt(xd*xd + yd*yd));
        tour_length += static_cast<uint64_t>(dist);
    }

    return tour_length;
}

int main() {
    load<coord_t>("../../data/tsp/extra/mona-lisa100K.tsp", Coords);
    assert(100000 == Coords.size());

    double (*d_C)[2] = new double[NUM_CITIES][2];

    memcpy(d_C, Coords.data(), NUM_CITIES * 2 * sizeof(double));

    int max_threads = omp_get_max_threads();
    std::cout << "== AMD Zen 5 (AVX-512 + OpenMP) CPU RecreateALL Benchmark =="
              << std::endl;
    std::cout << "Total Cities        : " << NUM_CITIES << std::endl;
    std::cout << "Total Runs Requested: " << NUM_RUNS << std::endl;
    std::cout << "OpenMP Threads      : " << max_threads << std::endl;

    std::vector<uint64_t> results(NUM_RUNS, 0);

    auto t_start = std::chrono::high_resolution_clock::now();

    // Parallelize tour runs across 32 threads on the Ryzen 9950X
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        std::mt19937 rng(1337 + tid * 10007);

        std::vector<uint32_t> city_order(NUM_CITIES);
        std::iota(city_order.begin(), city_order.end(), 0);

        // Thread-local tour working memory aligned to 64-byte boundaries
        alignas(64) std::vector<uint32_t> tour_A(NUM_CITIES);
        alignas(64) std::vector<uint32_t> tour_B(NUM_CITIES);

        #pragma omp for schedule(dynamic, 1)
        for (int run = 0; run < NUM_RUNS; ++run) {
            std::shuffle(city_order.begin(), city_order.end(), rng);

            uint64_t len = run_single_ruin_recreate(d_C, city_order,
                               tour_A.data(), tour_B.data());
            results[run] = len;

            #pragma omp critical
            {
                if ((run + 1) % 10 == 0 || run == NUM_RUNS - 1) {
                    std::cout << "Completed Run " << (run+1) << "/" << NUM_RUNS
                              << " | Latest Tour Length: " << len << std::endl;
                }
            }
        }
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    double total_cpu_ms =
        std::chrono::duration<double, std::milli>(t_end - t_start).count();

    // Stats
    uint64_t min_len = *std::min_element(results.begin(), results.end());
    uint64_t max_len = *std::max_element(results.begin(), results.end());
    double sum_len = std::accumulate(results.begin(), results.end(), 0.0);
    double mean_len = sum_len / NUM_RUNS;

    // Report Results
    std::cout << "\n================== RESULTS ==================" << std::endl;
    std::cout << "Total Runs Executed        : " << NUM_RUNS << std::endl;
    std::cout << "Minimum Tour Length (Best) : " << min_len << std::endl;
    std::cout << "Mean Tour Length           : " << std::fixed
              << std::setprecision(2) << mean_len << std::endl;
    std::cout << "Maximum Tour Length (Worst): " << max_len << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;
    std::cout << "Total CPU Runtime          : " << total_cpu_ms << " ms ("
              << total_cpu_ms / 1000.0 << " s)" << std::endl;
    std::cout << "Average Time per Tour Run  : " << (total_cpu_ms / NUM_RUNS)
              << " ms" << std::endl;

    return 0;
}
