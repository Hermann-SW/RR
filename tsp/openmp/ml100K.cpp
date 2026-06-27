/*
f=ml100K
g++ -O3 -march=native -fopenmp -Wall -Wextra -pedantic $f.cpp -o $f
cpplint --filter=-legal/copyright $f.cpp
cppcheck --enable=all --suppress=missingIncludeSystem $f.cpp --check-config

echo off | sudo tee /sys/devices/system/cpu/smt/control
echo 0 | sudo tee /proc/sys/kernel/perf_event_paranoid
perf stat -a -e fp_ops_retired_by_width.pack_512_uops_retired,cycles,instructions,task-clock ./$f

- power mode performance
- SMT turned off
- 500,000× determining current (optimal) 100,000 cities TSP tour length / core
- asserting tour length sums as repeats*opt_len enforces correct computations
- real world application with >92% of synthetic 43 Gsqrt/s(!) peak performance
  (https://gist.github.com/Hermann-SW/c4e40e823d274d03094d5e6d5071017d)

$ nproc
16
$ ./ml100K
... completed
#threads      : 16
Execution Time: 20.1636 seconds
double sqrt   : 39.6755 Gsqrt/S
euc_2d sum    : 5757191
$
*/
#include <math.h>
#include <omp.h>
#include <inttypes.h>
#include <immintrin.h>
#include <iostream>
#include <cassert>
#include <chrono>   // NOLINT [build/c++11]
#include "../../data/tsp/extra/mona-lisa100K.opt.h"

const int N = 100000;  // mona-list100K.tsp, divisible by 32!

const int repeats = 500000;

alignas(64) int16_t xy_even_[2*N] = {0};
alignas(64) int16_t xy_odd_[2*N] = {0};
alignas(64) int16_t xy_even[2*N] = {0};
alignas(64) int16_t xy_odd[2*N] = {0};

#if defined(__AVX512VNNI__)
    #define DOT_PRODUCT_ACC(acc, a, b) _mm512_dpwssd_epi32(acc, a, b)
#elif defined(__AVX512BW__)
    #define DOT_PRODUCT_ACC(acc, a, b) \
        _mm512_add_epi32(acc, _mm512_madd_epi16(a, b))
#else
    #error "This architecture is not supported. Requires at least AVX512BW."
#endif

void bench2() {
  auto start_time = std::chrono::high_resolution_clock::now();

  int64_t sum = 0;
  __m512d half_pd = _mm512_set1_pd(0.5);

for (int i = 0; i < repeats; ++i) {
  __m512i acc = _mm512_setzero_si512();

  for (int i = 0; i < 2*N; i+=32) {
    __m512i a = _mm512_load_si512((const __m512i*)(xy_even+i));
    __m512i b = _mm512_load_si512((const __m512i*)(xy_odd+i));

    __m512i dxy = _mm512_sub_epi16(a, b);

    __m512i aux = DOT_PRODUCT_ACC(_mm512_setzero_si512(), dxy, dxy);

    __m512d low_doubles  = _mm512_cvtepi32_pd(_mm512_castsi512_si256(aux));
    __m256i high_lanes   = _mm512_extracti64x4_epi64(aux, 1);
    __m512d high_doubles = _mm512_cvtepi32_pd(high_lanes);

    __m512d sqrt_low  = _mm512_sqrt_pd(low_doubles);
    __m512d sqrt_high = _mm512_sqrt_pd(high_doubles);

    __m512d res_low  = _mm512_add_pd(sqrt_low, half_pd);
    __m512d res_high = _mm512_add_pd(sqrt_high, half_pd);

    __m256i int_low  = _mm512_mask_cvtt_roundpd_epi32
                         (_mm256_undefined_si256(), 0xFF, res_low,
                             (_MM_FROUND_NO_EXC));
    __m256i int_high = _mm512_mask_cvtt_roundpd_epi32
                         (_mm256_undefined_si256(), 0xFF, res_high,
                           (_MM_FROUND_NO_EXC));

    __m512i euc_2d = _mm512_inserti64x4(_mm512_castsi256_si512(int_low),
                                        int_high, 1);

    acc = _mm512_add_epi32(acc, euc_2d);
  }

  sum += _mm512_reduce_add_epi32(acc);
}

  std::chrono::duration<double> duration =
    std::chrono::high_resolution_clock::now() - start_time;

  assert(sum == repeats * 5757191L);

  if (omp_get_thread_num() == 0) {
    std::cout << "... completed\n";

    std::cout << "#threads      : " << omp_get_num_threads() << "\n";
    std::cout << "Execution Time: " << duration.count() << " seconds\n";
    std::cout << "double sqrt   : "
              << repeats/1e9*N*omp_get_num_threads()/duration.count()
              << " Gsqrt/S\n";
    std::cout << "euc_2d sum    : " << sum/repeats << "\n";
  }
}

int main() {
  for (int i = 0; i < N; ++i) {
    xy_even_[2*i+0] = opt[i+0][0];      xy_even_[2*i+1] = opt[i+0][1];
     xy_odd_[2*i+0] = opt[(i+1)%N][0];   xy_odd_[2*i+1] = opt[(i+1)%N][1];
  }
  for (int64_t i = 0; i < 2 * N; i += 32) {
    for (int j = 0; j < 32; ++j) {
      if (i + j < 2 * N) {
        xy_even[i + j] = xy_even_[i + j];
        xy_odd[i + j]  = xy_odd_[i + j];
      }
    }
  }

  // do same computations on each thread, to determine real GHz drop perf
  #pragma omp parallel
  bench2();

  return 0;
}
