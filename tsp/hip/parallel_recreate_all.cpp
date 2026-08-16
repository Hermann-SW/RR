// NOLINT(legal/copyright)
#include <hip/hip_runtime.h>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <algorithm>
#include <random>
#include <cstdint>
#include <climits>
#include <cmath>

#include "../loader.h"

double C[100000][2];
std::vector<coord_t> Coords;

#define HIP_CHECK(command) \
    do { \
        hipError_t status = command; \
        if (status != hipSuccess) { \
            std::cerr << "HIP error: " << hipGetErrorString(status) \
                      << " at line " << __LINE__ << std::endl; \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

constexpr uint32_t NUM_CITIES = 100000;
constexpr int NUM_RUNS = 25;  // 100;

__device__ inline double gpu_sqrt(double val) {
    return val * rsqrt(val);
//    return __builtin_amdgcn_sqrt(val);
}

// 1. Kernel: Parallel search for the best insertion position
__global__ void find_best_insertion_kernel(
    const uint32_t* __restrict__ d_tour,
    uint32_t k,
    uint32_t u,
    const double (* __restrict__ d_C)[2],
    uint64_t* __restrict__ d_best) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = gridDim.x * blockDim.x;

    double ux = d_C[u][0];
    double uy = d_C[u][1];

    uint64_t min_cost = ULLONG_MAX;
    uint32_t best_i = 0;

    // Grid-stride loop evaluating candidate edges across all threads
    for (uint32_t i = idx; i < k; i += stride) {
        uint32_t p = d_tour[i];
        uint32_t v = (i + 1 == k) ? d_tour[0] : d_tour[i + 1];

        double px = d_C[p][0];
        double py = d_C[p][1];

        double vx = d_C[v][0];
        double vy = d_C[v][1];

        double d_pu = gpu_sqrt((px - ux)*(px - ux) + (py - uy)*(py - uy));
        double d_uv = gpu_sqrt((ux - vx)*(ux - vx) + (uy - vy)*(uy - vy));
        double d_pv = gpu_sqrt((px - vx)*(px - vx) + (py - vy)*(py - vy));

        int64_t extra_cost = static_cast<int64_t>(
            static_cast<uint16_t>(0.5 + d_pu) +
            static_cast<uint16_t>(0.5 + d_uv) -
            static_cast<uint16_t>(0.5 + d_pv));

        // Offset cost so it is positive for 64-bit atomic packing
        uint64_t biased_cost = static_cast<uint64_t>(extra_cost + 2000000000LL);

        if (biased_cost < min_cost) {
            min_cost = biased_cost;
            best_i = i;
        }
    }

    // Pack biased_cost (high 32 bits) and index best_i (low 32 bits)
    uint64_t val = (min_cost << 32) | static_cast<uint64_t>(best_i);

    // Block-level shared memory reduction
    __shared__ uint64_t s_min[256];
    s_min[threadIdx.x] = val;
    __syncthreads();

    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) {
            if (s_min[threadIdx.x + s] < s_min[threadIdx.x]) {
                s_min[threadIdx.x] = s_min[threadIdx.x + s];
            }
        }
        __syncthreads();
    }

    // Global atomic update
    if (threadIdx.x == 0) {
#if defined(__HIP_PLATFORM_NVIDIA__)
        atomicAdd((unsigned long long*) d_best,    // NOLINT
                  (unsigned long long) s_min[0]);  // NOLINT
#else
        atomicMin(d_best, s_min[0]);
#endif
    }
}

// 2. Kernel: Parallel double-buffered insertion into tour
__global__ void insert_and_copy_kernel(
    const uint32_t* __restrict__ d_tour_in,
    uint32_t* __restrict__ d_tour_out,
    uint32_t k,
    uint32_t u,
    const uint64_t* __restrict__ d_best) {
    uint64_t best_val = *d_best;
    uint32_t best_i = static_cast<uint32_t>(best_val & 0xFFFFFFFFULL);

    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx <= k) {
        if (idx <= best_i) {
            d_tour_out[idx] = d_tour_in[idx];
        } else if (idx == best_i + 1) {
            d_tour_out[idx] = u;
        } else {
            d_tour_out[idx] = d_tour_in[idx - 1];
        }
    }
}

// 3. Kernel: Reset atomic target for next iteration
__global__ void reset_best_kernel(uint64_t* __restrict__ d_best) {
    *d_best = ULLONG_MAX;
}

// 4. Kernel: Calculate final tour length
__global__ void compute_tour_length_kernel(
    const uint32_t* __restrict__ d_tour,
    const double (* __restrict__ d_C)[2],
    uint64_t* __restrict__ d_tour_length) {
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= NUM_CITIES) return;

    uint32_t u = d_tour[i];
    uint32_t v = (i + 1 == NUM_CITIES) ? d_tour[0] : d_tour[i + 1];

    double x1 = d_C[u][0];
    double y1 = d_C[u][1];
    double x2 = d_C[v][0];
    double y2 = d_C[v][1];

    double xd = x1 - x2;
    double yd = y1 - y2;

    uint16_t dist = static_cast<uint16_t>(0.5 + gpu_sqrt(xd * xd + yd * yd));
#if defined(__HIP_PLATFORM_NVIDIA__)
    atomicAdd((unsigned long long*) d_tour_length,                // NOLINT
              (unsigned long long) static_cast<uint64_t>(dist));  // NOLINT
#else
    atomicAdd(d_tour_length, static_cast<uint64_t>(dist));
#endif
}

int main() {
    load<coord_t>("../../data/tsp/extra/mona-lisa100K.tsp", Coords);
    assert(100000 == Coords.size());

    for (int i = 0; i < Coords.size(); ++i) {
        C[i][0] = Coords[i].first;
        C[i][1] = Coords[i].second;
    }

    std::cout << "=== GPU Multi-Core Parallel RecreateALL Benchmark ==="
              << std::endl;
    std::cout << "Total Cities        : " << NUM_CITIES << std::endl;
    std::cout << "Total Runs Requested: " << NUM_RUNS << std::endl;

    // Allocate minimal VRAM (< 2.4 MB total)
    double (*d_C)[2] = nullptr;
    uint32_t *d_tour_A = nullptr;
    uint32_t *d_tour_B = nullptr;
    uint64_t *d_best = nullptr;
    uint64_t *d_tour_length = nullptr;

    HIP_CHECK(hipMalloc(&d_C, NUM_CITIES * 2 * sizeof(double)));
    HIP_CHECK(hipMalloc(&d_tour_A, NUM_CITIES * sizeof(uint32_t)));
    HIP_CHECK(hipMalloc(&d_tour_B, NUM_CITIES * sizeof(uint32_t)));
    HIP_CHECK(hipMalloc(&d_best, sizeof(uint64_t)));
    HIP_CHECK(hipMalloc(&d_tour_length, sizeof(uint64_t)));

    HIP_CHECK(hipMemcpy(d_C, C, NUM_CITIES * 2 * sizeof(double),
                        hipMemcpyHostToDevice));

    std::vector<uint32_t> city_order(NUM_CITIES);
    std::iota(city_order.begin(), city_order.end(), 0);
    std::mt19937 rng(1337);

    uint64_t min_len = ULLONG_MAX;
    uint64_t max_len = 0;
    uint64_t sum_len = 0;

    uint64_t total_sqrts = 0;

    hipEvent_t start, stop;
    HIP_CHECK(hipEventCreate(&start));
    HIP_CHECK(hipEventCreate(&stop));

    std::cout << "\nExecuting " << NUM_RUNS << " cooperative GPU runs..."
              << std::endl;
    HIP_CHECK(hipEventRecord(start));

    for (int run = 0; run < NUM_RUNS; ++run) {
        // Shuffle city insertion sequence on host
        std::shuffle(city_order.begin(), city_order.end(), rng);

        // Initialize 1st 3 cities on GPU
        uint32_t init_tour[3] = { city_order[0], city_order[1], city_order[2] };
        HIP_CHECK(hipMemcpy(d_tour_A, init_tour, 3 * sizeof(uint32_t),
                            hipMemcpyHostToDevice));

        uint64_t init_best = ULLONG_MAX;
        HIP_CHECK(hipMemcpy(d_best, &init_best, sizeof(uint64_t),
                            hipMemcpyHostToDevice));

        uint32_t* d_curr = d_tour_A;
        uint32_t* d_next = d_tour_B;

        // Progressively insert remaining cities using all GPU CUs
        for (uint32_t k = 3; k < NUM_CITIES; ++k) {
            uint32_t u = city_order[k];

            int search_blocks = std::min(static_cast<int>((k+255) / 256), 256);
            find_best_insertion_kernel<<<search_blocks, 256>>>
                (d_curr, k, u, d_C, d_best);
            total_sqrts += (3*k);

            int copy_blocks = (k + 1 + 255) / 256;
            insert_and_copy_kernel<<<copy_blocks, 256>>>
                (d_curr, d_next, k, u, d_best);

            reset_best_kernel<<<1, 1>>>(d_best);

            std::swap(d_curr, d_next);
        }

        // Calculate tour length
        HIP_CHECK(hipMemset(d_tour_length, 0, sizeof(uint64_t)));
        compute_tour_length_kernel<<<391, 256>>>(d_curr, d_C, d_tour_length);
        total_sqrts += NUM_CITIES;

        uint64_t h_len = 0;
        HIP_CHECK(hipMemcpy(&h_len, d_tour_length, sizeof(uint64_t),
                            hipMemcpyDeviceToHost));

        min_len = std::min(min_len, h_len);
        max_len = std::max(max_len, h_len);
        sum_len += h_len;

        if ((run + 1) % 10 == 0 || run == NUM_RUNS - 1) {
            std::cout << "Completed Run " << (run + 1) << "/" << NUM_RUNS
                      << " | Latest Tour Length: " << h_len << std::endl;
        }
    }

    HIP_CHECK(hipEventRecord(stop));
    HIP_CHECK(hipEventSynchronize(stop));

    float total_gpu_ms = 0.0f;
    HIP_CHECK(hipEventElapsedTime(&total_gpu_ms, start, stop));

    double mean_len = static_cast<double>(sum_len) / NUM_RUNS;

    // Report Results
    std::cout << "\n=================== RESULTS ==================="
              << std::endl;
    std::cout << "Total Runs Executed        : " << NUM_RUNS << std::endl;
    std::cout << "Minimum Tour Length (Best) : " << min_len << std::endl;
    std::cout << "Mean Tour Length           : " << std::fixed
              << std::setprecision(2) << mean_len << std::endl;
    std::cout << "Maximum Tour Length (Worst): " << max_len << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;
    std::cout << "Total GPU Runtime          : " << total_gpu_ms << " ms ("
              << total_gpu_ms / 1000.0f << " s)" << std::endl;
    std::cout << "Average Time per Tour Run  : "
              << (total_gpu_ms / NUM_RUNS) << " ms" << std::endl;

    std::cout << "Throughput                 : " << std::fixed
              << std::setprecision(2)
              << (total_sqrts / (total_gpu_ms / 1000.0f)) / 1e9
              << " Gsqrt/s" << std::endl;

    // Cleanup
    HIP_CHECK(hipFree(d_C));
    HIP_CHECK(hipFree(d_tour_A));
    HIP_CHECK(hipFree(d_tour_B));
    HIP_CHECK(hipFree(d_best));
    HIP_CHECK(hipFree(d_tour_length));
    HIP_CHECK(hipEventDestroy(start));
    HIP_CHECK(hipEventDestroy(stop));

    return 0;
}
