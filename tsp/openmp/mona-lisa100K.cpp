/*
f=mona-lisa100K
g++ -O3 -fopenmp -mavx512f -mavx512vnni -mavx512bw -Wall -Wextra -pedantic $f.cpp -o $f
cpplint --filter=-legal/copyright $f.cpp
cppcheck --enable=all --suppress=missingIncludeSystem $f.cpp --check-config

echo off | sudo tee /sys/devices/system/cpu/smt/control
echo 0 | sudo tee /proc/sys/kernel/perf_event_paranoid
perf stat -a -e fp_ops_retired_by_width.pack_512_uops_retired,cycles,instructions,task-clock ./$f

- single omp thread faster than 8
- sequential without avx512 slower than single omp thread only after
  repeating the computations 500,000 times in new REPEAT(block)
- maybe needed addition of sqrt for TSP best insert will make the difference

hermann@8840hs:~/RR/tsp/openmp$ OMP_NUM_THREADS=1 ./$f
Starting benchmark(s) using 1 threads...
... completed
Execution Time: 1.30092 seconds
Sum of two sqs: 190899739
Execution Time: 3.16036 seconds
sequential: 190899739
hermann@8840hs:~/RR/tsp/openmp$ OMP_NUM_THREADS=8 ./$f
Starting benchmark(s) using 8 threads...
... completed
Execution Time: 2.00539 seconds
Sum of two sqs: 190899739
Execution Time: 3.18885 seconds
sequential: 190899739
hermann@8840hs:~/RR/tsp/openmp$
*/
#include <omp.h>
#include <inttypes.h>
#include <immintrin.h>
#include <iostream>
#include <chrono>   // NOLINT [build/c++11]
#include "../../data/tsp/extra/mona-lisa100K.opt.h"

const int N = 100000;  // mona-list100K.tsp, divisible by 32!

#define REPEAT(block) for(int i = 0; i < 500000; ++i) { block }

alignas(64) int16_t xy_even[N] = {0};
alignas(64) int16_t xy_odd[N] = {0};

int32_t horizontal_add_s32(__m512i v) {
  return _mm512_reduce_add_epi32(v);
}

void bench1() {
  auto start_time = std::chrono::high_resolution_clock::now();

  int32_t sum[2*16] = {0};  // 16C/32T AMD 7950X
  int total_threads = omp_get_num_threads();

  #pragma omp parallel
  {
    int thread_id = omp_get_thread_num();
    __m512i acc = _mm512_setzero_si512();

REPEAT(
    acc = _mm512_setzero_si512();

    for (int i = 0; i < N; i+=32) {
      __m512i a = _mm512_load_si512((const __m512i*)(xy_even+i));
      __m512i b = _mm512_load_si512((const __m512i*)(xy_odd+i));

      __m512i dxy = _mm512_sub_epi16(a, b);

      // Multiply pairs of s16, widen to s32, and add to accumulator
      // Resolves 32 pairs of s16 down to 16 lanes of s32
      acc = _mm512_dpwssd_epi32(acc, dxy, dxy);
    }
)

    sum[thread_id] = _mm512_reduce_add_epi32(acc);
  }  // omp parallel 

  for (int i = 1; i < total_threads; ++i) {
    sum[0] += sum[i];
  }

  std::cout << "... completed\n";

  std::chrono::duration<double> duration =
    std::chrono::high_resolution_clock::now() - start_time;

  std::cout << "Execution Time: " << duration.count() << " seconds\n";
  std::cout << "Sum of two sqs: " << sum[0] << "\n";
}

void sequential() {
  auto start_time = std::chrono::high_resolution_clock::now();
  int64_t sum = 0;

REPEAT(
  sum = 0;
  for (int i = 0; i < N; i+=32) {
    for (int j = 0; j < 32; j+=2) {
      const int16_t dx = xy_even[i+j] - xy_odd[i+j];
      const int16_t dy = xy_even[i+j+1] - xy_odd[i+j+1];
      sum += (dx*dx + dy*dy);
    }
  }
)

  std::chrono::duration<double> duration =
    std::chrono::high_resolution_clock::now() - start_time;

  std::cout << "Execution Time: " << duration.count() << " seconds\n";
  std::cout << "sequential: " << sum << "\n";
}

int main() {
  for (int i = 0; i < N; i+=2) {
    xy_even[i+0] = opt[i+0][0];  xy_even[i+1] = opt[i+0][1];
     xy_odd[i+0] = opt[i+1][0];   xy_odd[i+1] = opt[i+1][1];
  }

  std::cout << "Starting benchmark(s) using "
            << omp_get_max_threads() << " threads...\n";

  bench1();

  sequential();

  return 0;
}
