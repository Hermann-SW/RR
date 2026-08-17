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

__device__ inline double gpu_euc2d(double2 p_coord, double2 u_coord) {
    return  gpu_sqrt((p_coord.x - u_coord.x) * (p_coord.x - u_coord.x) +
                     (p_coord.y - u_coord.y) * (p_coord.y - u_coord.y));
}

constexpr int BLOCK_SIZE = 512;

// Optimized persistent kernel with LDS Tiling & Vectorized 128-Bit Loads
__global__ void persistent_recreate_all_kernel(
    const uint32_t* __restrict__ d_city_order,
    uint32_t* __restrict__ d_tour_A,
    uint32_t* __restrict__ d_tour_B,
    const double2* __restrict__ d_C,  // double2 cast for 128-bit vector loads
    uint64_t* __restrict__ d_best,
    uint64_t* __restrict__ d_tour_length) {

    cg::grid_group grid = cg::this_grid();
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = gridDim.x * blockDim.x;

    // LDS union (recycle 4 KB shared memory between tiling and block reduction)
    __shared__ union {
        uint32_t s_curr_tour[BLOCK_SIZE + 1];
        uint64_t s_min[BLOCK_SIZE];
    } smem;

    // Phase 1: Initialize first 3 cities
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

        const uint32_t* d_curr = (k % 2 == 1) ? d_tour_A : d_tour_B;
        uint32_t* d_next = (k % 2 == 1) ? d_tour_B : d_tour_A;

        // to insert city coordinates into VGPR registers (128-bit vector load)
        double2 u_coord = d_C[u];

        uint64_t min_cost = ULLONG_MAX;
        uint32_t best_i = 0;

        // Grid-stride loop over LDS tiles
        for (uint32_t tile_start = blockIdx.x * BLOCK_SIZE; tile_start < k;
             tile_start += stride) {
            uint32_t global_i = tile_start + threadIdx.x;

            // 1. Coalesced load of tour indices into LDS
            if (global_i < k) {
                smem.s_curr_tour[threadIdx.x] = d_curr[global_i];
            }

            // 2. Cache boundary element for tour continuity (i+1 wrap-around)
            if (threadIdx.x == 0) {
                if (tile_start + BLOCK_SIZE < k) {
                    smem.s_curr_tour[BLOCK_SIZE]
                        = d_curr[tile_start + BLOCK_SIZE];
                } else if (tile_start < k) {
                    smem.s_curr_tour[k - tile_start] = d_curr[0];
                }
            }
            __syncthreads();

            // 3. Compute cost (LDS cached indices & 128-bit vector coord loads)
            if (global_i < k) {
                uint32_t p = smem.s_curr_tour[threadIdx.x];
                uint32_t v = smem.s_curr_tour[threadIdx.x + 1];

                double2 p_coord = d_C[p];
                double2 v_coord = d_C[v];

                double d_pu = gpu_euc2d(p_coord, u_coord);
                double d_uv = gpu_euc2d(u_coord, v_coord);
                double d_pv = gpu_euc2d(p_coord, v_coord);

                int64_t extra_cost = static_cast<int64_t>(
                    static_cast<uint16_t>(0.5 + d_pu) +
                    static_cast<uint16_t>(0.5 + d_uv) -
                    static_cast<uint16_t>(0.5 + d_pv));

                uint64_t biased_cost
                    = static_cast<uint64_t>(extra_cost + 2000000000LL);

                if (biased_cost < min_cost) {
                    min_cost = biased_cost;
                    best_i = global_i;
                }
            }
            __syncthreads();
        }

        // Pack local min cost and index
        uint64_t val = (min_cost << 32) | static_cast<uint64_t>(best_i);

        // Block-level reduction using recycled LDS
        smem.s_min[threadIdx.x] = val;
        __syncthreads();

        for (int s = BLOCK_SIZE / 2; s > 0; s >>= 1) {
            if (threadIdx.x < s) {
                if (smem.s_min[threadIdx.x + s] < smem.s_min[threadIdx.x]) {
                    smem.s_min[threadIdx.x] = smem.s_min[threadIdx.x + s];
                }
            }
            __syncthreads();
        }

        // Global atomic reduction
        if (threadIdx.x == 0) {
#if defined(__HIP_PLATFORM_NVIDIA__)
            atomicMin((unsigned long long*) d_best,         // NOLINT
                      (unsigned long long) smem.s_min[0]);  // NOLINT
#else
            atomicMin(d_best, smem.s_min[0]);
#endif
        }

        grid.sync();

        // Double-buffered tour insertion
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

        if (idx == 0) {
            *d_best = ULLONG_MAX;
        }

        grid.sync();
    }

    // Phase 3: Compute final tour length
    const uint32_t* d_final_tour = ((NUM_CITIES-1)%2 == 1)?d_tour_B:d_tour_A;

    for (uint32_t i = idx; i < NUM_CITIES; i += stride) {
        uint32_t u_city = d_final_tour[i];
        uint32_t v_city = (i+1 == NUM_CITIES)?d_final_tour[0]:d_final_tour[i+1];

        double2 p1 = d_C[u_city];
        double2 p2 = d_C[v_city];

        uint16_t dist = static_cast<uint16_t>(0.5 + gpu_euc2d(p1, p2));
#if defined(__HIP_PLATFORM_NVIDIA__)
        atomicAdd((unsigned long long*) d_tour_length,                // NOLINT
                  (unsigned long long) static_cast<uint64_t>(dist));  // NOLINT
#else
        atomicAdd(d_tour_length, static_cast<uint64_t>(dist));
#endif
    }
}

int main() {
    load<coord_t>("../../data/tsp/extra/mona-lisa100K.tsp", Coords);
    assert(100000 == Coords.size());

    std::cout << "=== Persistent GPU Kernel Parallel RecreateALL Benchmark ==="
              << std::endl;
    std::cout << "Total Cities        : " << NUM_CITIES << std::endl;
    std::cout << "Total Runs Requested: " << NUM_RUNS << std::endl;

    // Check device cooperative launch support
    int device = 0;
    HIP_CHECK(hipGetDevice(&device));
    hipDeviceProp_t prop;
    HIP_CHECK(hipGetDeviceProperties(&prop, device));

    if (!prop.cooperativeLaunch) {
        std::cerr << "Device does not support cooperative kernel launches!"
                  << std::endl;
        return EXIT_FAILURE;
    }

    // Calculate max active blocks per CU for cooperative grid synchronization
    int block_size = BLOCK_SIZE;
    int num_blocks_per_cu = 0;
    HIP_CHECK(hipOccupancyMaxActiveBlocksPerMultiprocessor(
        &num_blocks_per_cu,
        reinterpret_cast<void*>(persistent_recreate_all_kernel),
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

    HIP_CHECK(hipMemcpy(d_C, Coords.data(), NUM_CITIES * 2 * sizeof(double),
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

    std::cout << "\nExecuting " << NUM_RUNS << " persistent GPU runs..."
              << std::endl;
    HIP_CHECK(hipEventRecord(start));

    for (int run = 0; run < NUM_RUNS; ++run) {
        std::shuffle(city_order.begin(), city_order.end(), rng);
        HIP_CHECK(hipMemcpy(d_city_order, city_order.data(),
                           NUM_CITIES*sizeof(uint32_t), hipMemcpyHostToDevice));

        // Launch single persistent kernel for the run
        void* args[] = {
            reinterpret_cast<void*>(&d_city_order),
            reinterpret_cast<void*>(&d_tour_A),
            reinterpret_cast<void*>(&d_tour_B),
            reinterpret_cast<void*>(&d_C),  // as const double2* inside kernel
            reinterpret_cast<void*>(&d_best),
            reinterpret_cast<void*>(&d_tour_length)
        };

        HIP_CHECK(hipLaunchCooperativeKernel(
            reinterpret_cast<void*>(persistent_recreate_all_kernel),
            dim3(num_blocks),
            dim3(block_size),
            args,
            0,
            0));

        // Retrieve tour length (synchronizes stream implicit to memcpy)
        uint64_t h_len = 0;
        HIP_CHECK(hipMemcpy(&h_len, d_tour_length, sizeof(uint64_t),
                            hipMemcpyDeviceToHost));

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
