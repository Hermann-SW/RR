/*
f=mona-lisa100K
g++ -O3 -fopenmp -mavx512f -mavx512vnni -mavx512bw -mavx512dq -Wall -Wextra -pedantic $f.cpp -o $f
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
#include <math.h>
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

/*
   I tried
   - always inline
   - template functions
   - lambdas
   without success, keeing macro as that works.
*/
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

void check_inverse_sqrt14() {
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

      // fored to use sqrt_pd and adding 0.5 rounding by nint() and euc_2d:
      // http://comopt.ifi.uni-heidelberg.de/software/TSPLIB95/tsp95.pdf#page=6
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

int main() {
  for (int i = 0; i < N; i+=2) {
    xy_even[i+0] = opt[i+0][0];  xy_even[i+1] = opt[i+0][1];
     xy_odd[i+0] = opt[i+1][0];   xy_odd[i+1] = opt[i+1][1];
  }

  std::cout << "Starting benchmark(s) using "
            << omp_get_max_threads() << " threads...\n";
#if 0
  bench1();

  sequential();
  sequential_noavx();
#endif
  check_inverse_sqrt14();

  return 0;
}
