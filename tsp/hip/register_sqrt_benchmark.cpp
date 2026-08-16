// NOLINT(legal/copyright)
#include <hip/hip_runtime.h>
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <cmath>

#include "../loader.h"

double C[100000][2];

#define HIP_CHECK(command) \
    do { \
        hipError_t status = command; \
        if (status != hipSuccess) { \
            std::cerr << "HIP error: " << hipGetErrorString(status) \
                      << " at line " << __LINE__ << std::endl; \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

std::vector<coord_t> Coords;

constexpr uint64_t NUM_CITIES = 100000;
constexpr uint64_t TOTAL_ENTRIES = NUM_CITIES * NUM_CITIES;

__device__ inline double gpu_sqrt(double val) {
    return val * rsqrt(val);
    // return rsqrt(1.0/val);
}

// 1. Pure Register Benchmark: IEEE-754 sqrt()
__global__ void benchmark_sqrt_registers(
    const double (* __restrict__ d_C)[2],
    uint64_t* __restrict__ d_checksum) {

    uint64_t i = static_cast<uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (i >= NUM_CITIES) return;

    double x1 = d_C[i][0];
    double y1 = d_C[i][1];

    uint64_t local_checksum = 0;

    // Process all 100,000 columns directly in registers (Zero VRAM writes)
    for (uint64_t j = 0; j < NUM_CITIES; ++j) {
        double x2 = d_C[j][0];
        double y2 = d_C[j][1];

        double xd = x1 - x2;
        double yd = y1 - y2;

        double dist = sqrt(xd * xd + yd * yd);
        uint16_t d_gpu = static_cast<uint16_t>(0.5 + dist);

        // Accumulate into register to prevent compiler dead-code elimination
        local_checksum += d_gpu;
    }

    // Single atomic update per thread at the end
#if defined(__HIP_PLATFORM_NVIDIA__)
    atomicAdd((unsigned long long*)d_checksum, (unsigned long long) local_checksum);
#else
     atomicAdd(d_checksum, local_checksum);
#endif
}

// 2. Pure Register Benchmark: gpu_sqrt()
__global__ void benchmark_builtin_registers(
    const double (* __restrict__ d_C)[2],
    uint64_t* __restrict__ d_checksum) {

    uint64_t i = static_cast<uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (i >= NUM_CITIES) return;

    double x1 = d_C[i][0];
    double y1 = d_C[i][1];

    uint64_t local_checksum = 0;

    for (uint64_t j = 0; j < NUM_CITIES; ++j) {
        double x2 = d_C[j][0];
        double y2 = d_C[j][1];

        double xd = x1 - x2;
        double yd = y1 - y2;

        // double dist = __builtin_amdgcn_sqrt(xd * xd + yd * yd);
        double dist = gpu_sqrt(xd * xd + yd * yd);
        uint16_t d_gpu = static_cast<uint16_t>(0.5 + dist);

        local_checksum += d_gpu;
    }

#if defined(__HIP_PLATFORM_NVIDIA__)
    atomicAdd((unsigned long long*)d_checksum, (unsigned long long) local_checksum);
#else
     atomicAdd(d_checksum, local_checksum);
#endif
}

// 3. Verification Kernel: Compare both on-the-fly in registers
__global__ void verify_registers(
    const double (* __restrict__ d_C)[2],
    uint64_t* __restrict__ d_matches) {

    uint64_t i = static_cast<uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (i >= NUM_CITIES) return;

    double x1 = d_C[i][0];
    double y1 = d_C[i][1];

    uint64_t local_matches = 0;

    for (uint64_t j = 0; j < NUM_CITIES; ++j) {
        double x2 = d_C[j][0];
        double y2 = d_C[j][1];

        double xd = x1 - x2;
        double yd = y1 - y2;

        double d_sqrt = sqrt(xd * xd + yd * yd);
        // double d_builtin = __builtin_amdgcn_sqrt(xd * xd + yd * yd);
        double d_builtin = gpu_sqrt(xd * xd + yd * yd);

        uint16_t val_sqrt = static_cast<uint16_t>(0.5 + d_sqrt);
        uint16_t val_builtin = static_cast<uint16_t>(0.5 + d_builtin);

        if (val_sqrt == val_builtin) {
            local_matches++;
        }
    }

#if defined(__HIP_PLATFORM_NVIDIA__)
    atomicAdd((unsigned long long*)d_matches, (unsigned long long) local_matches);
#else
     atomicAdd(d_matches, local_matches);
#endif
}

int main() {
    load<coord_t>("../../data/tsp/extra/mona-lisa100K.tsp", Coords);
    assert(100000 == Coords.size());

    for (int i = 0; i < Coords.size(); ++i) {
        C[i][0] = Coords[i].first;
        C[i][1] = Coords[i].second;
    }

    std::cout << "=== Zero-VRAM 100,000 x 100,000 TSP Register Benchmark ==="
              << std::endl;
    std::cout << "Total Pairs to Evaluate: " << TOTAL_ENTRIES
              << " (10 Billion)" << std::endl;

    // Allocate GPU VRAM (Only 400 KB for coordinates + 8 bytes for counters)
    double (*d_C)[2] = nullptr;
    uint64_t* d_checksum_sqrt = nullptr;
    uint64_t* d_checksum_builtin = nullptr;
    uint64_t* d_matches = nullptr;

    HIP_CHECK(hipMalloc(&d_C, NUM_CITIES * 2 * sizeof(double)));
    HIP_CHECK(hipMalloc(&d_checksum_sqrt, sizeof(uint64_t)));
    HIP_CHECK(hipMalloc(&d_checksum_builtin, sizeof(uint64_t)));
    HIP_CHECK(hipMalloc(&d_matches, sizeof(uint64_t)));

    HIP_CHECK(hipMemset(d_checksum_sqrt, 0, sizeof(uint64_t)));
    HIP_CHECK(hipMemset(d_checksum_builtin, 0, sizeof(uint64_t)));
    HIP_CHECK(hipMemset(d_matches, 0, sizeof(uint64_t)));

    // Copy Coordinates to VRAM
    HIP_CHECK(hipMemcpy(d_C, C, NUM_CITIES * 2 * sizeof(double),
                        hipMemcpyHostToDevice));

    int threadsPerBlock = 256;
    int blocksPerGrid = (NUM_CITIES + threadsPerBlock - 1) / threadsPerBlock;

    hipEvent_t start, stop;
    HIP_CHECK(hipEventCreate(&start));
    HIP_CHECK(hipEventCreate(&stop));

    // 1. Time sqrt()
    std::cout << "\nRunning sqrt() register benchmark..." << std::endl;
    HIP_CHECK(hipEventRecord(start));
    benchmark_sqrt_registers<<<blocksPerGrid, threadsPerBlock>>>
        (d_C, d_checksum_sqrt);
    HIP_CHECK(hipEventRecord(stop));
    HIP_CHECK(hipEventSynchronize(stop));

    float sqrt_ms = 0.0f;
    HIP_CHECK(hipEventElapsedTime(&sqrt_ms, start, stop));

    // 2. Time __builtin_amdgcn_sqrt()
    std::cout << "Running gpu_sqrt() register benchmark..." << std::endl;
    HIP_CHECK(hipEventRecord(start));
    benchmark_builtin_registers<<<blocksPerGrid, threadsPerBlock>>>
        (d_C, d_checksum_builtin);
    HIP_CHECK(hipEventRecord(stop));
    HIP_CHECK(hipEventSynchronize(stop));

    float builtin_ms = 0.0f;
    HIP_CHECK(hipEventElapsedTime(&builtin_ms, start, stop));

    // 3. Count Identical Matches
    std::cout << "Running verification kernel..." << std::endl;
    verify_registers<<<blocksPerGrid, threadsPerBlock>>>(d_C, d_matches);
    HIP_CHECK(hipDeviceSynchronize());

    // Fetch Results
    uint64_t total_matches = 0;
    HIP_CHECK(hipMemcpy(&total_matches, d_matches, sizeof(uint64_t),
              hipMemcpyDeviceToHost));

    // Print Results
    std::cout << "\n=================== RESULTS ==================="
              << std::endl;
    std::cout << "Total Pairs Tested           : " << TOTAL_ENTRIES
              << std::endl;
    std::cout << "Identical Values Count       : " << total_matches
              << std::endl;
    std::cout << "Match Percentage             : " << std::fixed
              << std::setprecision(8)
              << (static_cast<double>(total_matches) / TOTAL_ENTRIES) * 100.0
              << " %" << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;
    std::cout << "Total sqrt() Runtime         : " << sqrt_ms << " ms ("
              << sqrt_ms / 1000.0f << " s)" << std::endl;
    std::cout << "Total gpu_sqrt Runtime       : " << builtin_ms << " ms ("
              << builtin_ms / 1000.0f << " s)" << std::endl;
    std::cout << "Throughput sqrt()            : " << std::setprecision(2)
              << (TOTAL_ENTRIES / (sqrt_ms / 1000.0f)) / 1e9 << " Gsqrt/s"
              << std::endl;
    std::cout << "Throughput gpu_sqrt()        : " << std::setprecision(2)
              << (TOTAL_ENTRIES / (builtin_ms / 1000.0f)) / 1e9 << " Gsqrt/s"
              << std::endl;
    std::cout << "Speedup Factor               : " << std::setprecision(2)
              << (sqrt_ms / builtin_ms) << "x" << std::endl;

    // Cleanup
    HIP_CHECK(hipFree(d_C));
    HIP_CHECK(hipFree(d_checksum_sqrt));
    HIP_CHECK(hipFree(d_checksum_builtin));
    HIP_CHECK(hipFree(d_matches));
    HIP_CHECK(hipEventDestroy(start));
    HIP_CHECK(hipEventDestroy(stop));

    return 0;
}
