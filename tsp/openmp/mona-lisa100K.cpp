/*
f=mona-lisa100K
g++ -O3 -fopenmp -mavx512f -mavx512vnni -mavx512bw -Wall -Wextra -pedantic $f.cpp -o $f
cpplint --filter=-legal/copyright $f.cpp
cppcheck --enable=all --suppress=missingIncludeSystem $f.cpp --check-config

echo off | sudo tee /sys/devices/system/cpu/smt/control
echo 0 | sudo tee /proc/sys/kernel/perf_event_paranoid
perf stat -a -e fp_ops_retired_by_width.pack_512_uops_retired,cycles,instructions,task-clock ./$f

- now single omp thread slower than 8
- single thread faster than autovectorized sequential
- warning on #pragma inside (REPEAT) macro ignored for now

hermann@8840hs:~/RR/tsp/openmp$ OMP_NUM_THREADS=1 ./$f
Starting benchmark(s) using 1 threads...
... completed
Execution Time: 1.33151 seconds
Sum of two sqs: 190899739
Execution Time: 3.24091 seconds
sequential    : 190899739
Execution Time: 6.86801 seconds
sequent. noavx: 190899739
hermann@8840hs:~/RR/tsp/openmp$ OMP_NUM_THREADS=8 ./$f
Starting benchmark(s) using 8 threads...
... completed
Execution Time: 0.252588 seconds
Sum of two sqs: 190899739
Execution Time: 3.24941 seconds
sequential    : 190899739
Execution Time: 6.8669 seconds
sequent. noavx: 190899739
hermann@8840hs:~/RR/tsp/openmp$
*/
#include <omp.h>
#include <inttypes.h>
#include <immintrin.h>
#include <iostream>
#include <chrono>   // NOLINT [build/c++11]
#include "../../data/tsp/extra/mona-lisa100K.opt.h"

const int N = 100000;  // mona-list100K.tsp, divisible by 32!

#define REPEAT(block) for (int i = 0; i < 500000; ++i) { block }

alignas(64) int16_t xy_even[N] = {0};
alignas(64) int16_t xy_odd[N] = {0};

#pragma omp declare reduction(v512_add : __m512i : \
    omp_out = _mm512_add_epi32(omp_out, omp_in)) \
    initializer(omp_priv = _mm512_setzero_si512())

void bench1() {
  auto start_time = std::chrono::high_resolution_clock::now();

  int32_t sum = 0;
  __m512i acc = _mm512_setzero_si512();

REPEAT(
  acc = _mm512_setzero_si512();

  #pragma omp parallel for reduction(v512_add:acc)
  for (int i = 0; i < N; i+=32) {
    __m512i a = _mm512_load_si512((const __m512i*)(xy_even+i));
    __m512i b = _mm512_load_si512((const __m512i*)(xy_odd+i));

    __m512i dxy = _mm512_sub_epi16(a, b);

    acc = _mm512_dpwssd_epi32(acc, dxy, dxy);
  }

  sum = _mm512_reduce_add_epi32(acc);
)

  std::cout << "... completed\n";

  std::chrono::duration<double> duration =
    std::chrono::high_resolution_clock::now() - start_time;

  std::cout << "Execution Time: " << duration.count() << " seconds\n";
  std::cout << "Sum of two sqs: " << sum << "\n";
}

#define SEQUENTIAL                                                                                                         \
  auto start_time = std::chrono::high_resolution_clock::now();         \
  int64_t sum = 0;                                                     \
                                                                       \
REPEAT(                                                                \
  sum = 0;                                                             \
  for (int i = 0; i < N; i+=32) {                                      \
    for (int j = 0; j < 32; j+=2) {                                    \
      const int16_t dx = xy_even[i+j] - xy_odd[i+j];                   \
      const int16_t dy = xy_even[i+j+1] - xy_odd[i+j+1];               \
      sum += (dx*dx + dy*dy);                                          \
    }                                                                  \
  }                                                                    \
)                                                                      \
                                                                       \
  std::chrono::duration<double> duration =                             \
    std::chrono::high_resolution_clock::now() - start_time;            \
                                                                       \
  std::cout << "Execution Time: " << duration.count() << " seconds\n";

__attribute__((target("no-avx")))
void sequential_noavx() {
  SEQUENTIAL
  std::cout << "sequent. noavx: " << sum << "\n";
}

void sequential() {
  SEQUENTIAL
  std::cout << "sequential    : " << sum << "\n";
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
  sequential_noavx();

  return 0;
}
