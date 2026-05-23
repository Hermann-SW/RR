/*
f=mona-lisa100K
g++ -O3 -fopenmp -mavx512f -mavx512vnni -Wall -Wextra -pedantic $f.cpp -o $f
cpplint --filter=-legal/copyright $f.cpp
cppcheck --enable=all --suppress=missingIncludeSystem $f.cpp --check-config

echo off | sudo tee /sys/devices/system/cpu/smt/control
echo 0 | sudo tee /proc/sys/kernel/perf_event_paranoid
perf stat -a -e fp_ops_retired_by_width.pack_512_uops_retired,cycles,instructions,task-clock ./$f

1st version, sequential verification fails:

hermann@8840hs:~/RR/tsp/openmp$ ./$f
Starting benchmark(s) using 8 threads...
... completed
Execution Time: 0.000188086 seconds
Sum of two sqs: 118156476
sequential: 116953125
hermann@8840hs:~/RR/tsp/openmp$
*/
#include <omp.h>
#include <inttypes.h>
#include <immintrin.h>
#include <iostream>
#include <chrono>   // NOLINT [build/c++11]
#include "../../data/tsp/extra/mona-lisa100K.opt.h"

const int N = 100000;  // mona-list100K.tsp, divisible by 32!
alignas(64) int16_t x[N] = {0};
alignas(64) int16_t y[N] = {0};

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

    for (int i = 0; i < N; i+=32) {
      __m512i a = _mm512_load_si512((const __m512i*)(x+i));
      __m512i b = _mm512_load_si512((const __m512i*)(y+i));
      // Multiply pairs of s16, widen to s32, and add to accumulator
      // Resolves 32 pairs of s16 down to 16 lanes of s32
      acc = _mm512_dpwssd_epi32(acc, a, b);
    }

    sum[thread_id] = _mm512_reduce_add_epi32(acc);
  }

  for (int i = 1; i < total_threads; ++i) {
    sum[0] += sum[i];
  }

  std::cout << "... completed\n";

  std::chrono::duration<double> duration =
    std::chrono::high_resolution_clock::now() - start_time;

  std::cout << "Execution Time: " << duration.count() << " seconds\n";
  std::cout << "Sum of two sqs: " << sum[0] << "\n";
}

int main() {
  for (int i = 0; i < N; ++i) {
    x[i] = opt[i][0];
    y[i] = opt[i][1];
  }

  std::cout << "Starting benchmark(s) using "
            << omp_get_max_threads() << " threads...\n";

  bench1();

  int32_t sum = 0;
  for (int i = 0; i < N; i+=32) {
    for (int j = 0; j < 32; j+=2) {
      const int16_t dx = x[j] - x[j+1];
      const int16_t dy = y[j] - y[j+1];
      sum += (dx*dx + dy*dy);
    }
  }
  std::cout << "sequential: " << sum << "\n";

  return 0;
}
