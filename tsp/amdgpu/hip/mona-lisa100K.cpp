#include <iostream>
#include <vector>
#include <cmath>
#include <hip/hip_runtime.h>

// --- GPU Device Kernel ---
__global__ void euclidean_distance_kernel(const short2* __restrict__ xy_even, 
                                           const short2* __restrict__ xy_odd, 
                                           int* __restrict__ global_sum, 
                                           int N) 
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int local_acc = 0;

    // 1. Thread-Coalesced Compute Loop
    if (idx < N) {
        short2 a = xy_even[idx];
        short2 b = xy_odd[idx];

        int dx = (int)a.x - (int)b.x;
        int dy = (int)a.y - (int)b.y;

        double dist_sq = (double)(dx * dx + dy * dy);
        
        // Native MI50 v_sqrt_f64 instruction (quarter-rate)
//        double sqrt_res = __builtin_amdgcn_sqrtf64(dist_sq);
        double sqrt_res = __builtin_amdgcn_sqrt(dist_sq);

        // Truncate to round to nearest integer matching (+0.5)
        local_acc = (int)(sqrt_res + 0.5);
    }

    // 2. Intra-Wavefront Reduction (64 threads wide on Vega20/MI50)
    for (int offset = 64 / 2; offset > 0; offset /= 2) {
        local_acc += __shfl_down(local_acc, offset, 64);
    }

    // 3. Shared Memory Reduction for the entire Thread Block
    __shared__ int shared_block_sums[16]; // Fits max 1024 threads (16 wave fronts)
    int lane = threadIdx.x % 64;
    int wid = threadIdx.x / 64;

    if (lane == 0) {
        shared_block_sums[wid] = local_acc;
    }
    __syncthreads();

    // The first wavefront accumulates all warp totals for this block
    if (wid == 0) {
        int block_acc = (threadIdx.x < (blockDim.x / 64)) ? shared_block_sums[lane] : 0;
        for (int offset = 64 / 2; offset > 0; offset /= 2) {
            block_acc += __shfl_down(block_acc, offset, 64);
        }
        
        // 4. Single atomic update per thread block back to global storage
        if (lane == 0) {
            atomicAdd(global_sum, block_acc);
        }
    }
}

// --- Host Orchestration ---
int main() {
    const int N = 100000;
    const size_t array_size_bytes = N * sizeof(short2); // ~400 KB per array
    
    // Explicitly target the first MI50 device (change index for multi-GPU scaling)
    int device_id = 0;
    hipSetDevice(device_id);

    // 1. Allocate Host (CPU) memory and initialize demo data
    std::vector<short2> h_xy_even(N);
    std::vector<short2> h_xy_odd(N);
    
    for (int i = 0; i < N; ++i) {
        h_xy_even[i] = make_short2(100 + (i % 10), 200 + (i % 5));
        h_xy_odd[i]  = make_short2(90 + (i % 10),  190 + (i % 5));
    }

    // 2. Allocate Device (GPU) memory
    short2* d_xy_even = nullptr;
    short2* d_xy_odd  = nullptr;
    int* d_global_sum = nullptr;

    hipMalloc(&d_xy_even, array_size_bytes);
    hipMalloc(&d_xy_odd, array_size_bytes);
    hipMalloc(&d_global_sum, sizeof(int));

    // Create an asynchronous HIP stream
    hipStream_t stream;
    hipStreamCreate(&stream);

    // 3. Push data to the GPU and clear the remote scalar tracker
    int h_zero = 0;
    hipMemcpyAsync(d_xy_even, h_xy_even.data(), array_size_bytes, hipMemcpyHostToDevice, stream);
    hipMemcpyAsync(d_xy_odd, h_xy_odd.data(), array_size_bytes, hipMemcpyHostToDevice, stream);
    hipMemcpyAsync(d_global_sum, &h_zero, sizeof(int), hipMemcpyHostToDevice, stream);

    // 4. Configure Grid execution dimensions
    // 256 threads per block balances VGPR register allocation and local scheduling
    int threads_per_block = 256;
    int blocks_per_grid = (N + threads_per_block - 1) / threads_per_block;

    // 5. Fire execution kernel asynchronously inside the execution stream
    hipLaunchKernelGGL(
        euclidean_distance_kernel,
        dim3(blocks_per_grid),
        dim3(threads_per_block),
        0, // Dynamic shared memory size in bytes
        stream,
        d_xy_even,
        d_xy_odd,
        d_global_sum,
        N
    );

    // 6. Read back the lone calculated final scalar
    int h_final_sum = 0;
    hipMemcpyAsync(&h_final_sum, d_global_sum, sizeof(int), hipMemcpyDeviceToHost, stream);

    // Synchronize host execution threads with the asynchronous hardware pipeline
    hipStreamSynchronize(stream);

    // Print out the output result verification
    std::cout << "Successfully calculated Euclidean distance profile across " << N << " items." << std::endl;
    std::cout << "Aggregated Rounding Integer Sum Result: " << h_final_sum << std::endl;

    // 7. Cleanup Resources
    hipStreamDestroy(stream);
    hipFree(d_xy_even);
    hipFree(d_xy_odd);
    hipFree(d_global_sum);

    return 0;
}
