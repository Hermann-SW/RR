/*
f=mona-lisa100K
g++ -O3 -fopenmp -mavx512f -mavx512vnni -mavx512bw -mavx512dq -Wall -Wextra -pedantic $f.cpp -o $f
cpplint --filter=-legal/copyright $f.cpp
cppcheck --enable=all --suppress=missingIncludeSystem $f.cpp --check-config

echo off | sudo tee /sys/devices/system/cpu/smt/control
echo 0 | sudo tee /proc/sys/kernel/perf_event_paranoid
perf stat -a -e fp_ops_retired_by_width.pack_512_uops_retired,cycles,instructions,task-clock ./$f

- warning on #pragma inside (REPEAT) macro ignored for now
- power mode performance
- SMT turned off
- 500,000× determining current (optimal) 100,000 cities TSP tour length:
- sequential / AVX512 single threaded = 4.299
- AVX512 1 thread / 16 threads = 14.609
- total speedup 62.810
- check_sqrt_pd() did prove that AVX512 and C sqrt computations have identical results

hermann@7950x:~/RR/tsp/openmp$ nproc
16
hermann@7950x:~/RR/tsp/openmp$ OMP_NUM_THREADS=16 ./$f
Starting benchmark(s) using 16 threads...
... completed
Execution Time: 1.26175 seconds
euc_2d sum    : 5757191
hermann@7950x:~/RR/tsp/openmp$ OMP_NUM_THREADS=1 ./$f
Starting benchmark(s) using 1 threads...
... completed
Execution Time: 18.4331 seconds
euc_2d sum    : 5757191
Execution Time: 79.2512 seconds
sequential    : 5757191
Execution Time: 80.6615 seconds
sequent. noavx: 5757191
hermann@7950x:~/RR/tsp/openmp$
*/
#include <math.h>
#include <omp.h>
#include <inttypes.h>
#include <immintrin.h>
#include <iostream>
#include <chrono>   // NOLINT [build/c++11]
#include "../../data/tsp/extra/mona-lisa100K.opt.h"

const int N = 100000;  // mona-list100K.tsp, divisible by 32!

#define REPEAT(block) for (int i = 0; i < 500000; ++i) { block }

alignas(64) int16_t xy_even[2*N] = {0};
alignas(64) int16_t xy_odd[2*N] = {0};

#pragma omp declare reduction(v512_add : __m512i : \
    omp_out = _mm512_add_epi32(omp_out, omp_in)) \
    initializer(omp_priv = _mm512_setzero_si512())

// forced to use sqrt_pd and adding 0.5 for rounding (by C nint() and euc_2d):
// http://comopt.ifi.uni-heidelberg.de/software/TSPLIB95/tsp95.pdf#page=6
void check_sqrt_pd() {
  auto start_time = std::chrono::high_resolution_clock::now();

  alignas(64) int32_t tgt[16] = {0};
  alignas(64) float tgtf[16] = {0};

  alignas(64) int16_t xy_even2[2*N] = {0};

  for (int k = 0; k < N; ++k) {
    xy_even2[2*k] = opt[k][0]; xy_even2[2*k+1] = opt[k][1];
  }

  int64_t cnt = 0;
  std::cout.setf(std::ios_base::fixed, std::ios_base::floatfield);
  std::cout.precision(30);
  __m512d half_pd = _mm512_set1_pd(0.5);

  for (int i = 0; i < N; ++i) {
    for (int k = 0; k < 32; k += 2) {
      xy_odd[k] = opt[i][0]; xy_odd[k+1] = opt[i][1];
    }
    __m512i b = _mm512_load_si512((const __m512i*)(xy_odd));

    for (int j = 0; j < 2*N; j += 32) {
      __m512i a = _mm512_load_si512((const __m512i*)(xy_even2+j));

      __m512i dxy = _mm512_sub_epi16(a, b);

      __m512i acc = _mm512_setzero_si512();

      acc = _mm512_dpwssd_epi32(acc, dxy, dxy);

      __m512d low_doubles  = _mm512_cvtepi32_pd(_mm512_castsi512_si256(acc));
      __m256i high_lanes   = _mm512_extracti64x4_epi64(acc, 1);
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

      __m512i result = _mm512_inserti64x4(_mm512_castsi256_si512(int_low),
                                          int_high, 1);

      _mm512_store_si512(tgt, result);

      __m256 low_floats = _mm512_cvtpd_ps(sqrt_low);

      __m256 high_floats = _mm512_cvtpd_ps(sqrt_high);

      __m512 result2 = _mm512_insertf32x8(_mm512_castps256_ps512(low_floats),
                                          high_floats, 1);

      _mm512_store_ps(tgtf, result2);

      for (int k = 0; k < 32; k+=2) {
        const int16_t dx = opt[(j+k)/2][0] - opt[i][0];
        const int16_t dy = opt[(j+k)/2][1] - opt[i][1];
        const double sqs = sqrt(dx*dx + dy*dy);
        const int16_t euc2d = static_cast<int>(sqs+0.5);
        if (tgt[k/2] != euc2d) {
          std::cout << (j+k)/2 << "," << i << ") " << sqs << " " << tgtf[k/2]
                    << " " << euc2d << " " << tgt[k/2] << "\n";
          ++cnt;
        }
      }
    }
  }

  std::chrono::duration<double> duration =
    std::chrono::high_resolution_clock::now() - start_time;

  std::cout << "Execution Time: " << duration.count() << " seconds\n";
  std::cout << "cnt: " << cnt << " \n";
}

void bench2() {
  auto start_time = std::chrono::high_resolution_clock::now();

  int32_t sum = 0;
  __m512i acc = _mm512_setzero_si512();
  __m512d half_pd = _mm512_set1_pd(0.5);

REPEAT(
  acc = _mm512_setzero_si512();

  #pragma omp parallel for reduction(v512_add:acc)
  for (int i = 0; i < 2*N; i+=32) {
    __m512i a = _mm512_load_si512((const __m512i*)(xy_even+i));
    __m512i b = _mm512_load_si512((const __m512i*)(xy_odd+i));

    __m512i dxy = _mm512_sub_epi16(a, b);

    __m512i aux = _mm512_dpwssd_epi32(_mm512_setzero_si512(), dxy, dxy);

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

  sum = _mm512_reduce_add_epi32(acc);
)

  std::cout << "... completed\n";

  std::chrono::duration<double> duration =
    std::chrono::high_resolution_clock::now() - start_time;

  std::cout << "Execution Time: " << duration.count() << " seconds\n";
  std::cout << "euc_2d sum    : " << sum << "\n";
}

/*
   I tried
   - always inline
   - template functions
   - lambdas
   without success, keeping macro as that works.
*/
#define SEQUENTIAL                                                     \
  auto start_time = std::chrono::high_resolution_clock::now();         \
  int64_t sum = 0;                                                     \
                                                                       \
REPEAT(                                                                \
  sum = 0;                                                             \
  for (int i = 0, j = N-1; i < N; j = i, ++i) {                        \
    const int16_t dx = opt[i][0] - opt[j][0];                          \
    const int16_t dy = opt[i][1] - opt[j][1];                          \
    const double sqs = sqrt(dx*dx + dy*dy);                            \
    sum += static_cast<int>(sqs+0.5);                                  \
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
#if 0
  for (int i = 0; i < N; i+=2) {
    xy_even[i+0] = opt[i+0][0];  xy_even[i+1] = opt[i+0][1];
     xy_odd[i+0] = opt[i+1][0];   xy_odd[i+1] = opt[i+1][1];
  }
#endif
  for (int i = 0; i < N; ++i) {
    xy_even[2*i+0] = opt[i+0][0];      xy_even[2*i+1] = opt[i+0][1];
     xy_odd[2*i+0] = opt[(i+1)%N][0];   xy_odd[2*i+1] = opt[(i+1)%N][1];
  }

  std::cout << "Starting benchmark(s) using "
            << omp_get_max_threads() << " threads...\n";
#if 0
  bench1();

  sequential();
  sequential_noavx();
#endif

// check_sqrt_pd();

  bench2();

  if (omp_get_max_threads() == 1) {
    sequential();
    sequential_noavx();
  }

  return 0;
}
