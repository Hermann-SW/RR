#include <iostream>
#include <cmath>
#include <cstdint>
#include <hip/hip_runtime.h>

#define HIP_CHECK(cmd) \
    do { \
        hipError_t err = cmd; \
        if (err != hipSuccess) { \
            std::cerr << "HIP error: " << hipGetErrorString(err) \
                      << " at line " << __LINE__ << std::endl; \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

__device__ inline double gpu_sqrt(double val) {
    return val * rsqrt(val);
}

// Method 1 Kernel: Test arbitrary double precision floats near half-integer boundaries
__global__ void test_fp_kernel(int* found_flag) {
    // Array of k values to test: (k + 0.5)^2 produces exact half-integer square thresholds
    double k_vals[] = {10.0, 100.0, 1000.0, 12345.0, 99999.0};
    
    for (int i = 0; i < 5; ++i) {
        double half_int = k_vals[i] + 0.5;
        double base_d = half_int * half_int; // Exact double (e.g., 100.5^2 = 10100.25)
        
        // Pick the largest representable double strictly smaller than base_d
        double d = nextafter(base_d, 0.0);
        
        double s_exact = sqrt(d);
        double s_gpu   = gpu_sqrt(d);
        
        uint32_t r_exact = static_cast<uint32_t>(0.5 + s_exact);
        uint32_t r_gpu   = static_cast<uint32_t>(0.5 + s_gpu);
        
        if (r_exact != r_gpu) {
            printf("[Method 1: FP d] Found mismatch at d = %.17g\n", d);
            printf("  sqrt(d)     = %.17g -> rounded: %u\n", s_exact, r_exact);
            printf("  gpu_sqrt(d) = %.17g -> rounded: %u\n\n", s_gpu, r_gpu);
            *found_flag = 1;
        }
    }
}

// Method 2 Kernel: Parallel search over integer d = k^2 + k (TSP distance space)
__global__ void search_integer_d(uint64_t start_k, uint64_t count, int* found_flag) {
    uint64_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;
    
    uint64_t k = start_k + idx;
    double d = static_cast<double>(k * k + k);
    
    double s_exact = sqrt(d);
    double s_gpu   = gpu_sqrt(d);
    
    uint32_t r_exact = static_cast<uint32_t>(0.5 + s_exact);
    uint32_t r_gpu   = static_cast<uint32_t>(0.5 + s_gpu);
    
    if (r_exact != r_gpu) {
        printf("[Method 2: Integer d] Found mismatch at d = %.0f (k = %llu)\n", d, (unsigned long long)k);
        printf("  sqrt(d)     = %.17g -> rounded: %u\n", s_exact, r_exact);
        printf("  gpu_sqrt(d) = %.17g -> rounded: %u\n\n", s_gpu, r_gpu);
        atomicExch(found_flag, 1);
    }
}

int main() {
    int* d_found = nullptr;
    int h_found = 0;
    
    HIP_CHECK(hipMalloc(&d_found, sizeof(int)));

    // --- Execute Method 1: Floating-point search ---
    std::cout << "--- Executing Method 1 (Arbitrary Floating-Point d) ---\n";
    HIP_CHECK(hipMemset(d_found, 0, sizeof(int)));
    test_fp_kernel<<<1, 1>>>(d_found);
    HIP_CHECK(hipDeviceSynchronize());

    // --- Execute Method 2: Integer d search ---
    std::cout << "--- Executing Method 2 (Integer d around k ~ 2.37e7) ---\n";
    HIP_CHECK(hipMemset(d_found, 0, sizeof(int)));
    
    uint64_t start_k = 23700000ULL;
    uint64_t search_count = 1771164ULL; // Search 1 million consecutive k values
					//
    int threads_per_block = 256;
    int blocks = (search_count + threads_per_block - 1) / threads_per_block;
    
    search_integer_d<<<blocks, threads_per_block>>>(start_k, search_count, d_found);
    HIP_CHECK(hipDeviceSynchronize());
    
    HIP_CHECK(hipMemcpy(&h_found, d_found, sizeof(int), hipMemcpyDeviceToHost));
    if (!h_found) {
        std::cout << "No integer mismatch found in range. Expand 'search_count' or adjust 'start_k'.\n";
    }

    HIP_CHECK(hipFree(d_found));
    return 0;
}
