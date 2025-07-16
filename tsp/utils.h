#ifndef TSP_UTILS_H_
#define TSP_UTILS_H_

#include <sys/time.h>
#include <string>

#include "./tsp_tour.h"

auto _sum = 0;
struct timeval _tv0;
#define _tim gettimeofday(&_tv0, NULL)
#define _start (_tim, _sum -= (1000000*_tv0.tv_sec + _tv0.tv_usec));
#define _stop  (_tim, _sum += (1000000*_tv0.tv_sec + _tv0.tv_usec));

std::string i2s(int x) { std::stringstream s2; s2 << x; return s2.str(); }

template <typename C>
void print(C& L) {
  std::for_each(L.begin(), L.end(), [](typename C::value_type i) {
                                      std::cout << i << " ";
                                    });
  std::cout << '\n';
}

template <typename urn>
typename urn::value_type edraw(urn& U) {
  auto r = random() % U.size();
  typename urn::value_type ret = U[r];
  U[r] = U.back();
  U.pop_back();
  return ret;
}

void errlog(int i, int v, const std::string& trailer = "") {
  if (i >= 0)  std::cerr << i << ": ";
  std::cerr << v << "           " << trailer << "\r";
  if (i < 0)  std::cerr << "\n";
}

#endif  // TSP_UTILS_H_
