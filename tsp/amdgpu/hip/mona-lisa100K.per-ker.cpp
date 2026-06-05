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

// Persistent Grid Kernel utilizing all CUs
__global__ void persistentGridKernel(volatile int* hostFlag, volatile int* devBroadcastFlag, 
                                     volatile int* devBlockCounter,
                                     const short2* __restrict__ xy_even,
                                     const short2* __restrict__ xy_odd,
                                     int* __restrict__ global_sum,
                                     int N, int totalBlocks) {
    
    bool running = true;
    while (running) {
        
        // Stage 1: Master thread polls the Host
        if (blockIdx.x == 0 && threadIdx.x == 0) {
            int currentHostFlag = *hostFlag;
            if (currentHostFlag != *devBroadcastFlag) {
                // Broadcast the new command state to all internal GPU blocks
                *devBroadcastFlag = currentHostFlag;
            }
            if (currentHostFlag == 0) {
                __builtin_amdgcn_s_sleep(10); // Throttle master thread when idle
            }
        }

        // Synchronize internally via L2 cache read
        // __threadfence() equivalent or volatile tracking reads current value
        int command = *devBroadcastFlag;

        if (command == 1) {
            // 1. Thread Block 0 resets variables
            if (blockIdx.x == 0 && threadIdx.x == 0) {
                *global_sum = 0;
                *devBlockCounter = 0; 
                __threadfence(); 
                *devBroadcastFlag = 2; // Signal 'Gate Open / Computation Ready'
            }

            // 2. All blocks wait until Block 0 has cleared the sum and opened the gate
            while (*devBroadcastFlag == 1) {
                __builtin_amdgcn_s_sleep(2);
            }

            int idx = blockIdx.x * blockDim.x + threadIdx.x;
            int local_acc = 0;

            if (idx < N) {
                short2 a = xy_even[idx];
                short2 b = xy_odd[idx];
                int dx = (int)a.x - (int)b.x;
                int dy = (int)a.y - (int)b.y;
                double dist_sq = (double)(dx * dx + dy * dy);
                double sqrt_res = __builtin_amdgcn_sqrt(dist_sq);
                local_acc = (int)(sqrt_res + 0.5);
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

            // Global Fence to ensure global atomics are visible to all CUs
            __threadfence(); 
            __syncthreads(); 

            // EVERY block reports that it has checked in
            __shared__ bool isLastBlock;
            if (threadIdx.x == 0) {
                int ticket = atomicAdd((int*)devBlockCounter, 1);
                isLastBlock = (ticket == (totalBlocks - 1));
            }
            __syncthreads();

            // 3. Handshake back to host (Only handled safely by the absolute last block out the door)
            if (isLastBlock && threadIdx.x == 0) {
                *devBroadcastFlag = 0; // Reset internal broadcast back to idle
                __threadfence_system();
                *hostFlag = 0;         // Signal Host execution is done
            }
            
            // Wait for the broadcast flag to drop out of mode 2 before looping back
            while(*devBroadcastFlag == 2) {
                __builtin_amdgcn_s_sleep(5);
            }
        } 
        else if (command == -1) {
            running = false;
        } 
        else {
            // Idle state throttling
            __builtin_amdgcn_s_sleep(100); 
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

    int threads_per_block = 256;
    int blocks_per_grid = (N + threads_per_block - 1) / threads_per_block;

    std::cout << "GPU Detected: " << props.name << " with " << props.multiProcessorCount << " CUs." << std::endl;
    std::cout << "Launching persistent grid with " << blocks_per_grid << " blocks." << std::endl;

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
    
    // Launch kernel ONCE
    hipLaunchKernelGGL(persistentGridKernel, dim3(blocks_per_grid), dim3(threads_per_block), 0, 0, 
                       hostFlag, devBroadcastFlag, devBlockCounter, d_xy_even, d_xy_odd, d_global_sum, N, blocks_per_grid);
    HIP_CHECK(hipGetLastError());

    // Run 10 iterations safely
    for (int iter = 1; iter <= 10; ++iter) {
        std::cout << "\n--- Batch Loop Iteration " << iter << " ---" << std::endl;

        auto start = std::chrono::high_resolution_clock::now();

        // Wake up kernel execution
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

    // Graceful Termination
    std::cout << "\nTerminating GPU Persistent Grid..." << std::endl;
    *hostFlag = -1;
    HIP_CHECK(hipDeviceSynchronize());

    // Clean up
    HIP_CHECK(hipHostFree(hostFlag));
    HIP_CHECK(hipFree(devBroadcastFlag));
    HIP_CHECK(hipFree(devBlockCounter));
    HIP_CHECK(hipFree(d_xy_even));
    HIP_CHECK(hipFree(d_xy_odd));
    HIP_CHECK(hipFree(d_global_sum));

    std::cout << "System Cleaned Up. Exiting Safely." << std::endl;
    return 0;
}
