// NOLINT(legal/copyright)
#include <hip/hip_runtime.h>
#include <hip/hip_cooperative_groups.h>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <algorithm>
#include <random>
#include <cstdint>
#include <climits>
#include <cmath>

#include "../loader.h"

namespace cg = cooperative_groups;

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
}

// Single persistent kernel eliminating launch latency across loop iterations
__global__ void persistent_recreate_all_kernel(
    const uint32_t* __restrict__ d_city_order,
    uint32_t* __restrict__ d_tour_A,
    uint32_t* __restrict__ d_tour_B,
    const double (* __restrict__ d_C)[2],
    uint64_t* __restrict__ d_best,
    uint64_t* __restrict__ d_tour_length) {

    cg::grid_group grid = cg::this_grid();
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = gridDim.x * blockDim.x;

    // Phase 1: Initialize first 3 cities and initial atomic state
    if (idx == 0) {
        d_tour_A[0] = d_city_order[0];
        d_tour_A[1] = d_city_order[1];
        d_tour_A[2] = d_city_order[2];
        *d_best = ULLONG_MAX;
        *d_tour_length = 0;
    }
    grid.sync();

    // Phase 2: Progressively insert remaining cities
    for (uint32_t k = 3; k < NUM_CITIES; ++k) {
        uint32_t u = d_city_order[k];

        // Ping-pong between tour buffers without host intervention
        const uint32_t* d_curr = (k % 2 == 1) ? d_tour_A : d_tour_B;
        uint32_t* d_next = (k % 2 == 1) ? d_tour_B : d_tour_A;

        double ux = d_C[u][0];
        double uy = d_C[u][1];

        uint64_t min_cost = ULLONG_MAX;
        uint32_t best_i = 0;

        // Grid-stride search for the best insertion position
        for (uint32_t i = idx; i < k; i += stride) {
            uint32_t p = d_curr[i];
            uint32_t v = (i + 1 == k) ? d_curr[0] : d_curr[i + 1];

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

            uint64_t biased_cost = static_cast<uint64_t>(extra_cost + 2000000000LL);

            if (biased_cost < min_cost) {
                min_cost = biased_cost;
                best_i = i;
            }
        }

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

        // Global atomic reduction
        if (threadIdx.x == 0) {
#if defined(__HIP_PLATFORM_NVIDIA__)
            atomicMin((unsigned long long*) d_best, (unsigned long long) s_min[0]);
#else
            atomicMin(d_best, s_min[0]);
#endif
        }

        grid.sync();

        // Insertion step into double buffer
        uint64_t best_val = *d_best;
        uint32_t best_index = static_cast<uint32_t>(best_val & 0xFFFFFFFFULL);

        for (uint32_t i = idx; i <= k; i += stride) {
            if (i <= best_index) {
                d_next[i] = d_curr[i];
            } else if (i == best_index + 1) {
                d_next[i] = u;
            } else {
                d_next[i] = d_curr[i - 1];
            }
        }

        grid.sync();

        // Reset atomic tracker for next loop iteration
        if (idx == 0) {
            *d_best = ULLONG_MAX;
        }

        grid.sync();
    }

    // Phase 3: Final tour length calculation
    const uint32_t* d_final_tour = ((NUM_CITIES - 1) % 2 == 1) ? d_tour_B : d_tour_A;

    for (uint32_t i = idx; i < NUM_CITIES; i += stride) {
        uint32_t u_city = d_final_tour[i];
        uint32_t v_city = (i + 1 == NUM_CITIES) ? d_final_tour[0] : d_final_tour[i + 1];

        double x1 = d_C[u_city][0];
        double y1 = d_C[u_city][1];
        double x2 = d_C[v_city][0];
        double y2 = d_C[v_city][1];

        double xd = x1 - x2;
        double yd = y1 - y2;

        uint16_t dist = static_cast<uint16_t>(0.5 + gpu_sqrt(xd * xd + yd * yd));
#if defined(__HIP_PLATFORM_NVIDIA__)
        atomicAdd((unsigned long long*) d_tour_length,
                  (unsigned long long) static_cast<uint64_t>(dist));
#else
        atomicAdd(d_tour_length, static_cast<uint64_t>(dist));
#endif
    }
}

int main() {
    load<coord_t>("../../data/tsp/extra/mona-lisa100K.tsp", Coords);
    assert(100000 == Coords.size());

    for (size_t i = 0; i < Coords.size(); ++i) {
        C[i][0] = Coords[i].first;
        C[i][1] = Coords[i].second;
    }

    std::cout << "=== Persistent GPU Kernel Parallel RecreateALL Benchmark ===" << std::endl;
    std::cout << "Total Cities        : " << NUM_CITIES << std::endl;
    std::cout << "Total Runs Requested: " << NUM_RUNS << std::endl;

    // Check device cooperative launch support
    int device = 0;
    HIP_CHECK(hipGetDevice(&device));
    hipDeviceProp_t prop;
    HIP_CHECK(hipGetDeviceProperties(&prop, device));

    if (!prop.cooperativeLaunch) {
        std::cerr << "Device does not support cooperative kernel launches!" << std::endl;
        return EXIT_FAILURE;
    }

    // Calculate maximum active blocks per CU for cooperative grid synchronization
    int block_size = 256;
    int num_blocks_per_cu = 0;
    HIP_CHECK(hipOccupancyMaxActiveBlocksPerMultiprocessor(
        &num_blocks_per_cu,
        (void*)persistent_recreate_all_kernel,
        block_size,
        0));

    int num_blocks = prop.multiProcessorCount * num_blocks_per_cu;
    std::cout << "Cooperative Grid    : " << num_blocks << " blocks x " 
              << block_size << " threads (" << prop.multiProcessorCount 
              << " CUs)" << std::endl;

    // Allocate VRAM
    double (*d_C)[2] = nullptr;
    uint32_t *d_city_order = nullptr;
    uint32_t *d_tour_A = nullptr;
    uint32_t *d_tour_B = nullptr;
    uint64_t *d_best = nullptr;
    uint64_t *d_tour_length = nullptr;

    HIP_CHECK(hipMalloc(&d_C, NUM_CITIES * 2 * sizeof(double)));
    HIP_CHECK(hipMalloc(&d_city_order, NUM_CITIES * sizeof(uint32_t)));
    HIP_CHECK(hipMalloc(&d_tour_A, NUM_CITIES * sizeof(uint32_t)));
    HIP_CHECK(hipMalloc(&d_tour_B, NUM_CITIES * sizeof(uint32_t)));
    HIP_CHECK(hipMalloc(&d_best, sizeof(uint64_t)));
    HIP_CHECK(hipMalloc(&d_tour_length, sizeof(uint64_t)));

    HIP_CHECK(hipMemcpy(d_C, C, NUM_CITIES * 2 * sizeof(double), hipMemcpyHostToDevice));

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

    std::cout << "\nExecuting " << NUM_RUNS << " persistent GPU runs..." << std::endl;
    HIP_CHECK(hipEventRecord(start));

    for (int run = 0; run < NUM_RUNS; ++run) {
        std::shuffle(city_order.begin(), city_order.end(), rng);
        HIP_CHECK(hipMemcpy(d_city_order, city_order.data(),
                            NUM_CITIES * sizeof(uint32_t), hipMemcpyHostToDevice));

        // Launch single persistent kernel for the run
        void* args[] = {
            (void*)&d_city_order,
            (void*)&d_tour_A,
            (void*)&d_tour_B,
            (void*)&d_C,
            (void*)&d_best,
            (void*)&d_tour_length
        };

        HIP_CHECK(hipLaunchCooperativeKernel(
            (void*)persistent_recreate_all_kernel,
            dim3(num_blocks),
            dim3(block_size),
            args,
            0,
            0));

        // Retrieve tour length (synchronizes stream implicit to memcpy)
        uint64_t h_len = 0;
        HIP_CHECK(hipMemcpy(&h_len, d_tour_length, sizeof(uint64_t), hipMemcpyDeviceToHost));

        for (uint32_t k = 3; k < NUM_CITIES; ++k) {
            total_sqrts += (3 * k);
        }
        total_sqrts += NUM_CITIES;

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

    std::cout << "\n=================== RESULTS ===================" << std::endl;
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

    HIP_CHECK(hipFree(d_C));
    HIP_CHECK(hipFree(d_city_order));
    HIP_CHECK(hipFree(d_tour_A));
    HIP_CHECK(hipFree(d_tour_B));
    HIP_CHECK(hipFree(d_best));
    HIP_CHECK(hipFree(d_tour_length));
    HIP_CHECK(hipEventDestroy(start));
    HIP_CHECK(hipEventDestroy(stop));

    return 0;
}
