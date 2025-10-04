// g++ collatz.cpp -lgmp -lgmpxx -O3 -fopenmp -Wall -pedantic -Wextra -o collatz
// cpplint --filter=-legal/copyright,-runtime/references collatz.cpp
// cppcheck --enable=all --suppress=missingIncludeSystem collatz.cpp
//
#include <gmpxx.h>

#include <cassert>
#include <iostream>

void collatz(mpz_class& r, mpz_class& l, int s, mpz_class mx) {
  mpz_class a = s;
  l = 1;
  while (a != 1) {
    l += 1;
    if (mpz_fdiv_ui(a.get_mpz_t(), 2)) {
      a = 3 * a + 1;
      if (a > mx)  mx = a;
    } else {
      a >>= 1;
    }
  }
  r = mx;
}

int main(int argc, char *argv[]) {
  assert(argc == 2);
  int N = atoi(argv[1]);
  mpz_class mx = 1;
  mpz_class lmx = 1;

  #pragma omp parallel
  {
    mpz_class mx_local = mx;
    mpz_class lmx_local = lmx;

    #pragma omp for
    for (int i = 1; i <= N; ++i) {
      collatz(mx_local, lmx_local, i, mx);

      if (mx_local > mx) {
        if (lmx_local > lmx) {
          #pragma omp critical
          {
            mx = mx_local;
            lmx = lmx_local;
          }
        } else {
          #pragma omp critical
          {
            mx = mx_local;
          }
        }
      } else if (lmx_local > lmx) {
        #pragma omp critical
        {
          lmx = lmx_local;
        }
      }
    }
  }
  std::cout << "maximal=" << mx << "\n";
  std::cout << "longest=" << lmx << "\n";

  return 0;
}
