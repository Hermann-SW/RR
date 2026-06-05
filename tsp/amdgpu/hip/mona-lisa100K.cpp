/*
First 10 versions of this file from this Google Gemini chat, until working:
https://gemini.google.com/share/5b5d0a983dbf

f=mona-lisa100K
hipcc -O3 --amdgpu-target=gfx906 $f.cpp -o $f
rocm-smi --gpureset -d 0

Does 200,000× determine (optimal) tour length of 100,000 cities TSP.
Does that 18169× per second, with 8.31277 double sqrt GFLOPS !

$ /usr/bin/time ./mona-lisa100K > /dev/shm/out
21.71user 1.32system 0:13.33elapsed 172%CPU (0avgtext+0avgdata 159540maxresident)k
0inputs+12384outputs (1major+11682minor)pagefaults 0swaps
$ tail -11 /dev/shm/out 
--- Batch Loop Iteration 199999 ---
All CUs finished execution in: 0.011951 ms (8.3675 double sqrt GFLOPS)
Tour Length Global Sum Result: 5757191

--- Batch Loop Iteration 200000 ---
All CUs finished execution in: 0.011819 ms (8.46095 double sqrt GFLOPS)
Tour Length Global Sum Result: 5757191

Total time for all iterations: 11007.3 ms / 2405.94ms [18169.7/s / 8.31277 double sqrt GFLOPS]

System Cleaned Up. Exiting Safely.
$ 
*/
#include "../../../data/tsp/extra/mona-lisa100K.opt.h"

#include <hip/hip_runtime.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>

const int iterations = 200'000;

#define HIP_CHECK(command) { \
    hipError_t status = command; \
    if (status != hipSuccess) { \
        std::cerr << "HIP Error: " << hipGetErrorString(status) << " at line " << __LINE__ << std::endl; \
        exit(EXIT_FAILURE); \
    } \
}

// Clean, high-occupancy Grid-Stride Reduction Kernel
__global__ void tspReductionKernel(const short2* __restrict__ xy_even,
                                   const short2* __restrict__ xy_odd,
                                   int* __restrict__ global_sum,
                                   int N) {
    
    int local_acc = 0;
    int gtid = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = gridDim.x * blockDim.x;

    // Grid Stride Loop over the 100K elements
    for (int idx = gtid; idx < N; idx += stride) {
        short2 a = xy_even[idx];
        short2 b = xy_odd[idx];
        int dx = (int)a.x - (int)b.x;
        int dy = (int)a.y - (int)b.y;
        double dist_sq = (double)(dx * dx + dy * dy);
        double sqrt_res = __builtin_amdgcn_sqrt(dist_sq);
        local_acc += (int)(sqrt_res + 0.5);
    }

    // Intra-Wavefront Reduction (64 threads per wave on Radeon VII)
    for (int offset = 64 / 2; offset > 0; offset /= 2) {
        local_acc += __shfl_down(local_acc, offset, 64);
    }

    // Shared Memory Reduction across Wavefronts
    __shared__ int shared_block_sums[16];
    int lane = threadIdx.x % 64;
    int wid = threadIdx.x / 64;

    if (lane == 0) {
        shared_block_sums[wid] = local_acc;
    }
    __syncthreads();

    if (wid == 0) {
        int block_acc = (threadIdx.x < (blockDim.x / 64)) ? shared_block_sums[lane] : 0;
        for (int offset = 64 / 2; offset > 0; offset /= 2) {
            block_acc += __shfl_down(block_acc, offset, 64);
        }

        if (lane == 0) {
            atomicAdd(global_sum, block_acc);
        }
    }
}

int main() {
    const int N = 100000;
    const size_t array_size_bytes = N * sizeof(short2);

    int deviceId = 0;
    hipDeviceProp_t props;
    HIP_CHECK(hipGetDevice(&deviceId));
    HIP_CHECK(hipGetDeviceProperties(&props, deviceId));

    std::vector<short2> h_xy_even(N);
    std::vector<short2> h_xy_odd(N);

    // Initialize using optimization header data
    for (int i = 0; i < N; ++i) {
        h_xy_even[i].x = opt[i+0][0];      h_xy_even[i].y = opt[i+0][1];
        h_xy_odd[i].x = opt[(i+1)%N][0];  h_xy_odd[i].y = opt[(i+1)%N][1];
    }

    short2* d_xy_even = nullptr;
    short2* d_xy_odd  = nullptr;
    int* d_global_sum = nullptr;

    HIP_CHECK(hipMalloc(&d_xy_even, array_size_bytes));
    HIP_CHECK(hipMalloc(&d_xy_odd, array_size_bytes));
    HIP_CHECK(hipMalloc(&d_global_sum, sizeof(int)));

    // Keep data resident in high-speed VRAM
    HIP_CHECK(hipMemcpy(d_xy_even, h_xy_even.data(), array_size_bytes, hipMemcpyHostToDevice));
    HIP_CHECK(hipMemcpy(d_xy_odd, h_xy_odd.data(), array_size_bytes, hipMemcpyHostToDevice));

    // Optimize execution parameters for 60 CUs
    // 4 blocks per CU ensures maximum latency hiding without oversaturating the hardware queues
    int threads_per_block = 256;
    int blocks_per_grid = props.multiProcessorCount * 4; 

    std::cout << "GPU Detected: " << props.name << " with " << props.multiProcessorCount << " CUs." << std::endl;
    std::cout << "Hardware Configuration: Grid Size " << blocks_per_grid << " blocks, Block Size " << threads_per_block << " threads." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    double tot = 0.0;

    // Run 10 iterations safely driven by the host
    for (int iter = 1; iter <= iterations; ++iter) {
        std::cout << "\n--- Batch Loop Iteration " << iter << " ---" << std::endl;

        // Clear global scalar tracker before launching
        int zero = 0;
        HIP_CHECK(hipMemcpy(d_global_sum, &zero, sizeof(int), hipMemcpyHostToDevice));

        auto start = std::chrono::high_resolution_clock::now();

        // Launch the processing configuration directly
        hipLaunchKernelGGL(tspReductionKernel, dim3(blocks_per_grid), dim3(threads_per_block), 0, 0, 
                           d_xy_even, d_xy_odd, d_global_sum, N);
        
        // Wait for execution to complete on the stream
        HIP_CHECK(hipDeviceSynchronize());

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        std::cout << "All CUs finished execution in: " << duration.count() << " ms ("
                  << 1000.0/duration.count()*N/1e9 << " double sqrt GFLOPS)" << std::endl;
        tot += duration.count();

        // Read back the final computed scalar safely
        int h_final_sum = 0;
        HIP_CHECK(hipMemcpy(&h_final_sum, d_global_sum, sizeof(int), hipMemcpyDeviceToHost));
        std::cout << "Tour Length Global Sum Result: " << h_final_sum << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "\nTotal time for all iterations: " << duration.count() << " ms / "
              << tot << "ms [" << 1000.0/duration.count()*iterations << "/s / "
              << 1000.0/tot*N/1e9*iterations << " double sqrt GFLOPS]" << std::endl;

    // Free VRAM resources cleanly
    HIP_CHECK(hipFree(d_xy_even));
    HIP_CHECK(hipFree(d_xy_odd));
    HIP_CHECK(hipFree(d_global_sum));

    std::cout << "\nSystem Cleaned Up. Exiting Safely." << std::endl;
    return 0;
}
