#include "../../../data/tsp/extra/mona-lisa100K.opt.h"

#include <hip/hip_runtime.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>

#define HIP_CHECK(command) { \
    hipError_t status = command; \
    if (status != hipSuccess) { \
        std::cerr << "HIP Error: " << hipGetErrorString(status) << " at line " << __LINE__ << std::endl; \
        exit(EXIT_FAILURE); \
    } \
}

// Persistent Grid Kernel utilizing explicit cache invalidations via HIP intrinsics
__global__ void persistentGridKernel(volatile int* hostFlag, volatile int* devBroadcastFlag, 
                                     volatile int* devBlockCounter,
                                     const short2* __restrict__ xy_even,
                                     const short2* __restrict__ xy_odd,
                                     int* __restrict__ global_sum,
                                     int N, int totalBlocks) {
    
    bool running = true;
    while (running) {
        
        // Stage 1: Block 0, Thread 0 polls the Host Flag via atomics
        if (blockIdx.x == 0 && threadIdx.x == 0) {
            int currentHostFlag = atomicAdd((int*)hostFlag, 0); 
            int currentBroadcast = atomicAdd((int*)devBroadcastFlag, 0);

            if (currentHostFlag != currentBroadcast) {
                atomicExch((int*)devBroadcastFlag, currentHostFlag);
            }
            if (currentHostFlag == 0) {
                __builtin_amdgcn_s_sleep(15); 
            }
        }

        // Force L1 Cache Invalidation and sync visibility across CUs
        __builtin_amdgcn_fence(__ATOMIC_ACQUIRE, "agent");
        
        int command = atomicAdd((int*)devBroadcastFlag, 0);

        if (command == 1) {
            // 1. Thread Block 0 resets global variables
            if (blockIdx.x == 0 && threadIdx.x == 0) {
                *global_sum = 0;
                *devBlockCounter = 0; 
                __builtin_amdgcn_fence(__ATOMIC_RELEASE, "agent");
                atomicExch((int*)devBroadcastFlag, 2); // Open Gate
            }

            // 2. All blocks wait until Gate is opened (command moves from 1 to 2)
            while (atomicAdd((int*)devBroadcastFlag, 0) == 1) {
                __builtin_amdgcn_s_sleep(2);
                __builtin_amdgcn_fence(__ATOMIC_ACQUIRE, "agent");
            }

            int local_acc = 0;
            int gtid = blockIdx.x * blockDim.x + threadIdx.x;
            int stride = totalBlocks * blockDim.x;

            // Grid Stride Loop
            for (int idx = gtid; idx < N; idx += stride) {
                short2 a = xy_even[idx];
                short2 b = xy_odd[idx];
                int dx = (int)a.x - (int)b.x;
                int dy = (int)a.y - (int)b.y;
                double dist_sq = (double)(dx * dx + dy * dy);
                double sqrt_res = __builtin_amdgcn_sqrt(dist_sq);
                local_acc += (int)(sqrt_res + 0.5);
            }

            // Intra-Wavefront Reduction
            for (int offset = 64 / 2; offset > 0; offset /= 2) {
                local_acc += __shfl_down(local_acc, offset, 64);
            }

            // Shared Memory Reduction
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

            // Ensure atomic additions are flushed globally out to L2
            __builtin_amdgcn_fence(__ATOMIC_RELEASE, "agent");
            __syncthreads(); 

            // Barrier registration
            __shared__ bool isLastBlock;
            if (threadIdx.x == 0) {
                int ticket = atomicAdd((int*)devBlockCounter, 1);
                isLastBlock = (ticket == (totalBlocks - 1));
            }
            __syncthreads();

            // 3. Handshake back to host
            if (isLastBlock && threadIdx.x == 0) {
                atomicExch((int*)devBroadcastFlag, 0); 
                __builtin_amdgcn_fence(__ATOMIC_RELEASE, "system");
                atomicExch((int*)hostFlag, 0); // Release host loop         
            }
            
            // Wait for mode reset before continuing loop
            while(atomicAdd((int*)devBroadcastFlag, 0) == 2) {
                __builtin_amdgcn_s_sleep(10);
                __builtin_amdgcn_fence(__ATOMIC_ACQUIRE, "agent");
            }
        } 
        else if (command == -1) {
            running = false;
        } 
        else {
            __builtin_amdgcn_s_sleep(200); 
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

    HIP_CHECK(hipMemcpy(d_xy_even, h_xy_even.data(), array_size_bytes, hipMemcpyHostToDevice));
    HIP_CHECK(hipMemcpy(d_xy_odd, h_xy_odd.data(), array_size_bytes, hipMemcpyHostToDevice));

    // STRICT 1 BLOCK PER CU CEILING
    int threads_per_block = 256;
    int blocks_per_grid = props.multiProcessorCount; // Exactly 60 blocks

    std::cout << "GPU Detected: " << props.name << " with " << props.multiProcessorCount << " CUs." << std::endl;
    std::cout << "Launching 1-Block-per-CU persistent grid with " << blocks_per_grid << " blocks." << std::endl;

    int* hostFlag = nullptr;
    int* devBroadcastFlag = nullptr;
    int* devBlockCounter = nullptr;

    HIP_CHECK(hipHostMalloc(&hostFlag, sizeof(int), hipHostMallocCoherent));
    HIP_CHECK(hipMalloc(&devBroadcastFlag, sizeof(int)));
    HIP_CHECK(hipMalloc(&devBlockCounter, sizeof(int)));

    *hostFlag = 0;
    int zero = 0;
    HIP_CHECK(hipMemcpy(devBroadcastFlag, &zero, sizeof(int), hipMemcpyHostToDevice));
    HIP_CHECK(hipMemcpy(devBlockCounter, &zero, sizeof(int), hipMemcpyHostToDevice));
    HIP_CHECK(hipMemcpy(d_global_sum, &zero, sizeof(int), hipMemcpyHostToDevice));
    
    hipLaunchKernelGGL(persistentGridKernel, dim3(blocks_per_grid), dim3(threads_per_block), 0, 0, 
                       hostFlag, devBroadcastFlag, devBlockCounter, d_xy_even, d_xy_odd, d_global_sum, N, blocks_per_grid);
    HIP_CHECK(hipGetLastError());

    for (int iter = 1; iter <= 10; ++iter) {
        std::cout << "\n--- Batch Loop Iteration " << iter << " ---" << std::endl;

        auto start = std::chrono::high_resolution_clock::now();

        // Wake up kernel safely
        *hostFlag = 1;

        // Host wait loop 
        while (*hostFlag == 1) {
            std::this_thread::yield();
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        std::cout << "All CUs finished execution in: " << duration.count() << " ms" << std::endl;

        int h_final_sum = 0;
        HIP_CHECK(hipMemcpy(&h_final_sum, d_global_sum, sizeof(int), hipMemcpyDeviceToHost));
        std::cout << "Tour Length Global Sum Result: " << h_final_sum << std::endl;
    }

    std::cout << "\nTerminating GPU Persistent Grid..." << std::endl;
    *hostFlag = -1;
    HIP_CHECK(hipDeviceSynchronize());

    HIP_CHECK(hipHostFree(hostFlag));
    HIP_CHECK(hipFree(devBroadcastFlag));
    HIP_CHECK(hipFree(devBlockCounter));
    HIP_CHECK(hipFree(d_xy_even));
    HIP_CHECK(hipFree(d_xy_odd));
    HIP_CHECK(hipFree(d_global_sum));

    std::cout << "System Cleaned Up. Exiting Safely." << std::endl;
    return 0;
}
