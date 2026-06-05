#include <hip/hip_runtime.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>

// Hard architectural parameters for gfx906 (Radeon VII / MI50)
#define THREADS_PER_BLOCK 256

// --- The Un-Optimizable Hardware Smasher Kernel ---
__global__ void genuine_sqrt_kernel(const double* __restrict__ d_in, 
                                    double* __restrict__ d_out, 
                                    int N) 
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (idx < N) {
        double val = d_in[idx];
        
        // A deep, dependent instruction pipeline chain.
        // Every single step depends strictly on the result of the previous step.
        // The compiler has absolutely no choice but to issue 10 sequential v_sqrt_f64 commands.
        val = __builtin_amdgcn_sqrt(val);
        val = __builtin_amdgcn_sqrt(val + 1.0);
        val = __builtin_amdgcn_sqrt(val + 2.0);
        val = __builtin_amdgcn_sqrt(val + 3.0);
        val = __builtin_amdgcn_sqrt(val + 4.0);
        val = __builtin_amdgcn_sqrt(val + 5.0);
        val = __builtin_amdgcn_sqrt(val + 6.0);
        val = __builtin_amdgcn_sqrt(val + 7.0);
        val = __builtin_amdgcn_sqrt(val + 8.0);
        val = __builtin_amdgcn_sqrt(val + 9.0);
        
        // Forcing a global memory write back means the loop can never be categorized as "Dead Code"
        d_out[idx] = val;
    }
}

int main() {
    // 100 Million Elements = 1,000,000,000 (1 Billion) total double square roots executed!
    const int N = 100000000; 
    const int SQRTS_PER_THREAD = 10;
    size_t bytes = N * sizeof(double);

    std::cout << "Allocating " << (bytes * 2) / (1024 * 1024) << " MB of VRAM..." << std::endl;

    // Host memory pointers
    std::vector<double> h_in(N);
    std::vector<double> h_out(N);

    // Initialize inputs with dynamic data so the compiler cannot treat inputs as zero/constants
    for (int i = 0; i < N; ++i) {
        h_in[i] = static_cast<double>(i) + 0.5;
    }

    // Device memory pointers
    double *d_in, *d_out;
    hipMalloc(&d_in, bytes);
    hipMalloc(&d_out, bytes);

    // Copy input data to Radeon VII
    hipMemcpy(d_in, h_in.data(), bytes, hipMemcpyHostToDevice);

    // Grid sizing parameters
    int blocks = (N + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;

    std::cout << "Launching Kernel across " << blocks << " thread blocks..." << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;

    // Create GPU timers for pristine hardware monitoring
    hipEvent_t start, stop;
    hipEventCreate(&start);
    hipEventCreate(&stop);

    // Record hardware start timeline
    hipEventRecord(start, nullptr);

    // Execute the benchmark kernel
    hipLaunchKernelGGL(genuine_sqrt_kernel, dim3(blocks), dim3(THREADS_PER_BLOCK), 0, nullptr, d_in, d_out, N);

    // Record hardware stop timeline
    hipEventRecord(stop, nullptr);
    hipEventSynchronize(stop);

    // Calculate execution runtime from GPU clocks
    float milliseconds = 0;
    hipEventElapsedTime(&milliseconds, start, stop);
    double seconds = milliseconds / 1000.0;

    // Fetch output data back to host to guarantee the operations are completed
    hipMemcpy(h_out.data(), d_out, bytes, hipMemcpyDeviceToHost);

    // Calculate actual operational metrics
    double total_sqrts = static_cast<double>(N) * SQRTS_PER_THREAD;
    double gflops = (total_sqrts / seconds) / 1e9;

    std::cout << "Execution Completed Successfully." << std::endl;
    std::cout << "Execution Time: " << seconds << " seconds" << std::endl;
    std::cout << "Total Sqrt Operations: " << total_sqrts << std::endl;
    std::cout << "Verification Check (Last Element): " << h_out[N-1] << std::endl;
    std::cout << "\033[1;32m" << gflops << " Genuine double sqrt GFLOPS\033[0m" << std::endl;

    // Clean up resources
    hipEventDestroy(start);
    hipEventDestroy(stop);
    hipFree(d_in);
    hipFree(d_out);

    return 0;
}
