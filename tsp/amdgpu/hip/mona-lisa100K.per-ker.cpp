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

// Persistent Grid Kernel using strict VRAM-only signaling
__global__ void persistentGridKernel(int* devSignalFlag, 
                                     int* devBlockCounter,
                                     const short2* __restrict__ xy_even,
                                     const short2* __restrict__ xy_odd,
                                     int* __restrict__ global_sum,
                                     int N, int totalBlocks) {
    
    bool running = true;
    while (running) {
        
        // FORCE L1 BYPASS: All threads read the VRAM signal flag via device-level atomic add of 0
        int command = atomicAdd(devSignalFlag, 0);

        if (command == 1) {
            // 1. Thread Block 0 resets global variables and clears the way
            if (blockIdx.x == 0 && threadIdx.x == 0) {
                atomicExch(global_sum, 0);
                atomicExch(devBlockCounter, 0); 
                __threadfence(); 
                atomicExch(devSignalFlag, 2); // Transition to state 2: "In Progress"
            }

            // 2. All blocks wait until Block 0 shifts the state from 1 to 2
            while (atomicAdd(devSignalFlag, 0) == 1) {
                __builtin_amdgcn_s_sleep(2);
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

            // Ensure atomic additions are flushed out to L2 VRAM
            __threadfence(); 
            __syncthreads(); 

            // Barrier registration via atomic check-in
            __shared__ bool isLastBlock;
            if (threadIdx.x == 0) {
                int ticket = atomicAdd(devBlockCounter, 1);
                isLastBlock = (ticket == (totalBlocks - 1));
            }
            __syncthreads();

            // 3. Handshake back to host
            if (isLastBlock && threadIdx.x == 0) {
                __threadfence();
                atomicExch(devSignalFlag, 0); // Reset VRAM flag back to 0 (Idle)
            }
            
            // Wait for the last block to flip state back to 0 before looping
            while(atomicAdd(devSignalFlag, 0) == 2) {
                __builtin_amdgcn_s_sleep(5);
            }
        } 
        else if (command == -1) {
            running = false;
        } 
        else {
            // Idle state throttling
            __builtin_amdgcn_s_sleep(150); 
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
    int blocks_per_grid = props.multiProcessorCount; // Exactly 60 blocks

    std::cout << "GPU Detected: " << props.name << " with " << props.multiProcessorCount << " CUs." << std::endl;
    std::cout << "Launching safe persistent grid with " << blocks_per_grid << " blocks." << std::endl;

    int* devSignalFlag = nullptr;
    int* devBlockCounter = nullptr;

    // Allocate flags purely inside fast device VRAM
    HIP_CHECK(hipMalloc(&devSignalFlag, sizeof(int)));
    HIP_CHECK(hipMalloc(&devBlockCounter, sizeof(int)));

    int zero = 0;
    int start_cmd = 1;
    int terminate_cmd = -1;

    HIP_CHECK(hipMemcpy(devSignalFlag, &zero, sizeof(int), hipMemcpyHostToDevice));
    HIP_CHECK(hipMemcpy(devBlockCounter, &zero, sizeof(int), hipMemcpyHostToDevice));
    HIP_CHECK(hipMemcpy(d_global_sum, &zero, sizeof(int), hipMemcpyHostToDevice));
    
    // Launch kernel ONCE to stay persistent in background
    hipLaunchKernelGGL(persistentGridKernel, dim3(blocks_per_grid), dim3(threads_per_block), 0, 0, 
                       devSignalFlag, devBlockCounter, d_xy_even, d_xy_odd, d_global_sum, N, blocks_per_grid);
    HIP_CHECK(hipGetLastError());

    // Main control loop
    for (int iter = 1; iter <= 10; ++iter) {
        std::cout << "\n--- Batch Loop Iteration " << iter << " ---" << std::endl;

        auto start = std::chrono::high_resolution_clock::now();

        // Wake up kernel by forcing an explicit high-speed write to VRAM
        HIP_CHECK(hipMemcpy(devSignalFlag, &start_cmd, sizeof(int), hipMemcpyHostToDevice));

        // Host monitoring loop: Poll the GPU VRAM variable until it transitions back to 0
        int check_signal = 1;
        while (check_signal != 0) {
            // Using a fast synchronous copy acts as an iron-clad memory barrier
            HIP_CHECK(hipMemcpy(&check_signal, devSignalFlag, sizeof(int), hipMemcpyDeviceToHost));
            if (check_signal != 0) {
                std::this_thread::yield();
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        std::cout << "All CUs finished execution in: " << duration.count() << " ms" << std::endl;

        int h_final_sum = 0;
        HIP_CHECK(hipMemcpy(&h_final_sum, d_global_sum, sizeof(int), hipMemcpyDeviceToHost));
        std::cout << "Tour Length Global Sum Result: " << h_final_sum << std::endl;
    }

    std::cout << "\nTerminating GPU Persistent Grid..." << std::endl;
    // Tell the grid loop to break execution securely
    HIP_CHECK(hipMemcpy(devSignalFlag, &terminate_cmd, sizeof(int), hipMemcpyHostToDevice));
    HIP_CHECK(hipDeviceSynchronize());

    HIP_CHECK(hipFree(devSignalFlag));
    HIP_CHECK(hipFree(devBlockCounter));
    HIP_CHECK(hipFree(d_xy_even));
    HIP_CHECK(hipFree(d_xy_odd));
    HIP_CHECK(hipFree(d_global_sum));

    std::cout << "System Cleaned Up. Exiting Safely." << std::endl;
    return 0;
}
