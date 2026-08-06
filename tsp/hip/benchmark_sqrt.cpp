/*
  f=benchmark_sqrt
  hipcc -O3 --offload-arch=gfx906 $f.cpp -o $f  # AMD Instinct MI50
  hipcc -O3 -arch=sm_60 -x cu $f.cpp -o $f      # NVIDIA Tesla P100
  cpplint --filter=-legal/copyright $f.cpp
  cppcheck --enable=all --suppress=missingIncludeSystem $f.cpp --checkers-report=out

hermann@7600x:~$ ./benchmark_sqrt 3
Device ID 3 (AMD Instinct MI50/MI60) UUID: GPU-13c24061732c730c
Number of CUs/SMs: 60
Allocating 6103 MB of VRAM...
Launching Kernel across 1666667 thread blocks...
--------------------------------------------------------
Execution Completed Successfully.
Execution Time: 0.00916043 seconds
Total Sqrt Operations: 4e+09
Verification Check (Last Element): 3.51286
436.661 double Gsqrt/s
hermann@7600x:~$
*/
#include <hip/hip_runtime.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>  // NOLINT [build/c++11]
#include <cmath>

#if defined(__HIP_PLATFORM_NVIDIA__)
    #include <cuda_runtime.h>
    #define SQRT __dsqrt_rn
#else
    #define SQRT __builtin_amdgcn_sqrt
#endif

__global__ void genuine_sqrt_kernel(const double* __restrict__ d_in,
                                    double* __restrict__ d_out,
                                    int N, int loops) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx < N) {
        double val = d_in[idx];

      for(int i=0; i<loops; ++i) {
        val = SQRT(val);
        val = SQRT(val + 1.0);
        val = SQRT(val + 2.0);
        val = SQRT(val + 3.0);
        val = SQRT(val + 4.0);
        val = SQRT(val + 5.0);
        val = SQRT(val + 6.0);
        val = SQRT(val + 7.0);
        val = SQRT(val + 8.0);
        val = SQRT(val + 9.0);
      }
        d_out[idx] = val;
    }
}

int main(int argc, const char *argv[]) {
    const int N = 400'000'000;
    int loops = (argc == 2) ? 1 : atoi(argv[2]);
    const int SQRTS_PER_THREAD = 10*loops;
    size_t bytes = N * sizeof(double);

    int deviceId = (argc == 1) ? 0 : atoi(argv[1]);
    int cusmCount = 0;

    hipDeviceProp_t props;
    assert(hipSuccess == hipGetDeviceProperties(&props, deviceId));

    std::cout << "Device ID " << deviceId << " (" << props.name << ") UUID: ";

    #if defined(__HIP_PLATFORM_AMD__)
    // AMD handles UUID as a literal 16-character ASCII string inside props
    std::cout << "GPU-";
    std::cout.write(reinterpret_cast<const char*>(props.uuid.bytes), 16);

    #elif defined(__HIP_PLATFORM_NVIDIA__)
    cudaDeviceProp cudaProps;
    if (cudaGetDeviceProperties(&cudaProps, deviceId) == cudaSuccess) {
        std::cout << "GPU-";
        std::hex(std::cout);
        std::cout << std::setfill('0');

        std::cout << std::setw(16)
                  << *reinterpret_cast<uint64_t*>(cudaProps.uuid.bytes)
                  << std::setw(16)
                  << *reinterpret_cast<uint64_t*>(cudaProps.uuid.bytes+8);
        std::dec(std::cout);
    } else {
        std::cout << "UNKNOWN_CUDA_ERROR";
    }
    #else
        std::cout << "UNSUPPORTED_PLATFORM";
    #endif

    std::cout << "\n";

    assert(hipSuccess == hipDeviceGetAttribute(&cusmCount,
                             hipDeviceAttributeMultiprocessorCount, deviceId));
    std::cout << "Number of CUs/SMs: " << cusmCount << std::endl;

    int THREADS_PER_BLOCK = 4 * cusmCount;

    std::cout << "Allocating " << (bytes * 2) / (1024 * 1024)
              << " MB of VRAM..." << std::endl;

    std::vector<double> h_in(N);
    std::vector<double> h_out(N);

    for (int i = 0; i < N; ++i) {
        h_in[i] = static_cast<double>(i) + 0.5;
    }

    double *d_in, *d_out;
    (void) hipMalloc(&d_in, bytes);
    (void) hipMalloc(&d_out, bytes);

    (void) hipMemcpy(d_in, h_in.data(), bytes, hipMemcpyHostToDevice);

    int blocks = (N + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;

    std::cout << "Launching Kernel across " << blocks << " thread blocks..."
              << std::endl;
    std::cout << "--------------------------------------------------------"
              << std::endl;

    hipEvent_t start, stop;
    (void) hipEventCreate(&start);
    (void) hipEventCreate(&stop);

    (void) hipEventRecord(start, nullptr);

    hipLaunchKernelGGL(genuine_sqrt_kernel, dim3(blocks),
                       dim3(THREADS_PER_BLOCK), 0, nullptr, d_in, d_out, N, loops);

    (void) hipEventRecord(stop, nullptr);
    (void) hipEventSynchronize(stop);

    float milliseconds = 0;
    (void) hipEventElapsedTime(&milliseconds, start, stop);
    double seconds = milliseconds / 1000.0;

    // Fetch output data back to host to guarantee the operations are completed
    (void) hipMemcpy(h_out.data(), d_out, bytes, hipMemcpyDeviceToHost);

    double total_sqrts = static_cast<double>(N) * SQRTS_PER_THREAD;
    double gflops = (total_sqrts / seconds) / 1e9;

    std::cout << "Execution Completed Successfully." << std::endl;
    std::cout << "Execution Time: " << seconds << " seconds" << std::endl;
    std::cout << "Total Sqrt Operations: " << total_sqrts << std::endl;
    std::cout << "Verification Check (Last Element): " << h_out[N-1]
              << std::endl;
    std::cout << gflops << " double Gsqrt/s " << std::endl;

    (void) hipEventDestroy(start);
    (void) hipEventDestroy(stop);
    (void) hipFree(d_in);
    (void) hipFree(d_out);

    return 0;
}
