#include <omp.h>
#include <sys/time.h>
#include <inttypes.h>

#include <iostream>
#include <string>
#include <sstream>

auto _sum = 0;
struct timeval _tv0;
#define _tim gettimeofday(&_tv0, NULL)
#define _start (_tim, _sum -= (1000000*_tv0.tv_sec + _tv0.tv_usec));
#define _stop  (_tim, _sum += (1000000*_tv0.tv_sec + _tv0.tv_usec));

std::string i2s(int x) { std::stringstream s2; s2 << x; return s2.str(); }


#define N 250000000000

static unsigned char arr[N];

void init(unsigned char c) {
  _sum = 0;
_start {
    _Pragma("omp parallel for")
    for (int64_t i = 0; i < N; ++i)  arr[i] = c;
  }
_stop
  std::cout << "         init(" << i2s(c) << ") in " << _sum*1e-6 << "s\n";
}

// https://www.geeksforgeeks.org/cpp/introduction-to-parallel-programming-with-openmp-in-cpp/
//
void sum() {
  uint64_t s = 0;
  _sum = 0;
_start {
    _Pragma("omp parallel for reduction(+ : s)")
    for (int64_t i = 0; i < N; ++i)  s+=arr[i];
  }
_stop
  std::cout << "sum " << s << " in " << _sum*1e-6 << "s\n";
}

int main() {
  int ncpu = omp_get_max_threads();
  omp_set_num_threads(ncpu);

  std::cout << " #B " << sizeof(arr) << "\n";

  init(2);
  sum();

  init(3);
  sum();

  return 0;
}
