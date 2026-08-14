// NOLINT(legal/copyright)
#include <hip/hip_runtime.h>

#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <cmath>

#include "../loader.h"

typedef int city_t;

std::vector<coord_t> Coords;
std::vector<city_t> opt;

constexpr uint64_t NUM_EVALS = 10000000;

#define HIP_CHECK(command) \
    do { \
        hipError_t status = command; \
        if (status != hipSuccess) { \
            std::cerr << "HIP error: " << hipGetErrorString(status) \
                      << " at line " << __LINE__ << std::endl; \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

// GPU Kernel: Each thread computes the complete TSP tour length
__global__ void compute_tour_length_kernel(
    uint64_t num_cities,
    const double (* __restrict__ d_opt)[2],
    uint64_t* __restrict__ d_total_sum) {

    uint64_t tid = static_cast<uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (tid >= NUM_EVALS) return;

    uint64_t tour_length = 0;

    // 1. Sum distances between consecutive cities in the tour
    for (uint64_t i = 0; i < num_cities - 1; ++i) {
        double x1 = d_opt[i][0];
        double y1 = d_opt[i][1];

        double x2 = d_opt[i + 1][0];
        double y2 = d_opt[i + 1][1];

        double xd = x1 - x2;
        double yd = y1 - y2;

        double dist = sqrt(xd * xd + yd * yd);
        tour_length += static_cast<uint32_t>(0.5 + dist);
    }

    // 2. Add closing edge (from last city back to first city)
    double x_last  = d_opt[num_cities - 1][0];
    double y_last  = d_opt[num_cities - 1][1];
    double x_first = d_opt[0][0];
    double y_first = d_opt[0][1];

    double xd = x_last - x_first;
    double yd = y_last - y_first;

    double closing_dist = sqrt(xd * xd + yd * yd);
    tour_length += static_cast<uint32_t>(0.5 + closing_dist);

    // 3. Accumulate thread's computed tour length into global total
#if defined(__HIP_PLATFORM_NVIDIA__)
    atomicAdd((unsigned long long*)d_total_sum, (unsigned long long)tour_length);
#else
    atomicAdd(d_total_sum, tour_length);
#endif
}

int main(int argc, char *argv[]) {
    assert(argc == 2);
    std::string fname(argv[1]);

    load<coord_t>(fname + ".tsp", Coords);
    uint64_t NUM_CITIES = Coords.size();

    load<city_t>(fname + ".opt.tour", opt);

    auto dopt = new double[NUM_CITIES][2];
    for (int i = 0; i < Coords.size(); ++i) {
        dopt[i][0] = Coords[opt[i]-1].first;
        dopt[i][1] = Coords[opt[i]-1].second;
    }

    std::cout << "=== Optimal TSP Tour Length Benchmark ===" << std::endl;
    std::cout << "Total Tour Length Computations: " << NUM_EVALS << std::endl;
    std::cout << "Total Cities per Tour         : " << NUM_CITIES << std::endl;
    std::cout << "Total Distance Calculations   : " << NUM_EVALS * NUM_CITIES
              << std::endl;

    // Allocate GPU VRAM
    double (*d_opt)[2] = nullptr;
    uint64_t* d_total_sum = nullptr;

    HIP_CHECK(hipMalloc(&d_opt, NUM_CITIES * 2 * sizeof(double)));
    HIP_CHECK(hipMalloc(&d_total_sum, sizeof(uint64_t)));

    HIP_CHECK(hipMemset(d_total_sum, 0, sizeof(uint64_t)));

    // Copy coordinates to VRAM
    HIP_CHECK(hipMemcpy(d_opt, dopt, NUM_CITIES * 2 * sizeof(double),
                        hipMemcpyHostToDevice));

    // Execution Configuration: Launch 10,000 threads
    int threadsPerBlock = 256;
    int blocksPerGrid = (NUM_EVALS + threadsPerBlock - 1) / threadsPerBlock;

    hipEvent_t start, stop;
    HIP_CHECK(hipEventCreate(&start));
    HIP_CHECK(hipEventCreate(&stop));

    std::cout << "\nLaunching Kernel across " << NUM_EVALS << " threads..."
              << std::endl;

    HIP_CHECK(hipEventRecord(start));
    compute_tour_length_kernel<<<blocksPerGrid, threadsPerBlock>>>
        (NUM_CITIES, d_opt, d_total_sum);
    HIP_CHECK(hipEventRecord(stop));
    HIP_CHECK(hipEventSynchronize(stop));

    float kernel_ms = 0.0f;
    HIP_CHECK(hipEventElapsedTime(&kernel_ms, start, stop));

    // Retrieve global sum
    uint64_t total_sum = 0;
    HIP_CHECK(hipMemcpy(&total_sum, d_total_sum, sizeof(uint64_t),
                        hipMemcpyDeviceToHost));

    uint64_t single_tour_length = total_sum / NUM_EVALS;

    // Report Results
    std::cout << "\n=================== RESULTS ==================="
              << std::endl;
    std::cout << "Single Tour Length           : " << single_tour_length
              << std::endl;
    std::cout << "Total Sum (" << NUM_EVALS << " Computations): " << total_sum
              << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;
    std::cout << "Total GPU Kernel Runtime     : " << kernel_ms << " ms ("
              << kernel_ms / 1000.0f << " s)" << std::endl;
    std::cout << "Throughput                   : " << std::fixed
              << std::setprecision(2)
              << ((NUM_EVALS * 1.0 * NUM_CITIES) / (kernel_ms / 1000.0f)) / 1e9
              << " Gsqrt/s" << std::endl;

    assert(single_tour_length == opt_length);

    // Cleanup
    HIP_CHECK(hipFree(d_opt));
    HIP_CHECK(hipFree(d_total_sum));
    HIP_CHECK(hipEventDestroy(start));
    HIP_CHECK(hipEventDestroy(stop));

    delete[] dopt;

    return 0;
}
