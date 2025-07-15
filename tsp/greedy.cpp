/*
   TSP Ruin and Recreate greedy implementation with random+sequential+radial ruins:
   https://www.semanticscholar.org/paper/Record-Breaking-Optimization-Results-Using-the-Ruin-Schrimpf-Schneider/4f80e70e51e368858c3df0787f05c3aa2b9650b4

   c++ -O3 -std=c++17 -Wall -Wextra -pedantic pcb442.cpp -o pcb442 -lstdc++ -lm
   (tested with g++ and clang++)

   for tour display
   - append compiler flags "-Dezxdisp -lezx -lX11"
   - after "make install" of ezxdisp repo first: 
   https://github.com/Hermann-SW/ezxdisp?tab=readme-ov-file#support-for-c--use-in-ide
   (left mouse click continues to next accepted mutation and updates display; repeat)

   cpplint --filter=-legal/copyright,-runtime/references pcb442.cpp
   cppcheck --enable=all --suppress=missingIncludeSystem pcb442.cpp
*/
#include <sys/time.h>
auto _sum = 0;
struct timeval _tv0;
#define _tim gettimeofday(&_tv0, NULL)
#define _start (_tim, _sum -= (1000000*_tv0.tv_sec + _tv0.tv_usec));
#define _stop  (_tim, _sum += (1000000*_tv0.tv_sec + _tv0.tv_usec));

#ifdef ezxdisp
#include <unistd.h>
#include <ezxdisp.h>
#endif

#include <sstream>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <cassert>
#include <vector>
#include <list>

std::string i2s(int x) { std::stringstream s2; s2 << x; return s2.str(); }

template <typename C>
[[maybe_unused]] void print(const C& L) {
  std::for_each(L.begin(), L.end(), [](const typename C::value_type i) {
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

template <typename val, int N>
class random_access_list {
  std::list<val> L;

 public:
  typedef typename std::list<val>::iterator iterator;
  iterator A[N];

  void init() {
    L.clear();
    for (int i = 0; i < N; ++i)  A[i] = L.end();
  }

  iterator& operator[](std::size_t i)  { return A[i]; }

  void sort()  { L.sort(); }
  void push_back(val& v )  { L.push_back(v); A[v] = --L.end(); }
  iterator insert(iterator it, val& v )  { return A[v] = L.insert(it, v); }
  iterator erase(iterator it)  { A[*it] = L.end(); return L.erase(it); }
  iterator erase(int i)  {
    iterator it = A[i]; A[i] = L.end(); return L.erase(it);
  }
  iterator begin()  { return L.begin(); }
  iterator end()  { return L.end(); }
  bool empty()  { return L.empty(); }
  val& back()  { return L.back(); }
  size_t size()  { return L.size(); }
};

template <typename config, typename urn>
class pcb442 {
 public:
  static const int N = 442;  // config::N;
  const double siz, ran, seq, rad;
  std::string last;

  pcb442(double _siz, double _ran, double _seq, double _rad) :
    siz(_siz), ran(_ran), seq(_seq), rad(_rad) {
    assert(siz >= 0.0 && siz <= 1.0);
    assert(ran+seq+rad == 1.0);
    assert(ran >= 0.0 && seq >= 0.0 && rad >= 0.0);

    init_dist();
  }


  int cost(config& C) {
    int cost = 0;
    int prev = C.empty() ? -1 : C.back();
    std::for_each(C.begin(), C.end(), [this, &cost, &prev](const int c) {
                                        cost += D[prev][c]; prev = c;
                                      });
    return cost;
  }


  void init(config &C, std::pair<urn, urn> &Us) {
    C.init();
    Us.first.clear();
    Us.second.clear();
    for (int i = 0; i < N; ++i)  Us.first.push_back(i);
  }

  void RR_all(config &C, std::pair<urn, urn> &Us) {
    init(C, Us);
    recreate(C, Us);
  }

  int draw_rad(config& C, int size, std::pair<urn, urn>& Us) {
    auto center = random() % C.size();
    last = "rad(" + i2s(center) + "," + i2s(size) + ")";
    Us.first.clear();
    std::for_each_n(rad_nxt[center].begin(), size, [&C, &Us](auto& c) {
      C.erase(c);
      Us.first.push_back(c);
    });
    return center;
  }

  int draw_seq(config& C, int size, std::pair<urn, urn>& Us) {
    auto start = random() % C.size();
    last = "seq(" + i2s(start) + "," + i2s(size) + ")";
    typename config::iterator it = C[start];
    int ret = *it;
    while (size-- > 0 && it != C.end()) {
      int c = *it;
      it = C.erase(it);
      Us.first.push_back(c);
    }
    it = C.begin();
    while (size-- > 0) {
      int c = *it;
      it = C.erase(it);
      Us.first.push_back(c);
    }
    return -1-ret;
  }

  int draw_ran(config& C, int size,
               std::pair<urn, urn>& Us) {
    assert(Us.first.size() == 0);
    assert(Us.second.size() == N);
    last = "ran(" + i2s(size) + ")";
    for (; size > 0; --size) {
      int r = edraw(Us.second);
      Us.first.push_back(r);
      C.erase(r);
    }
    std::for_each(Us.first.begin(), Us.first.end(), [&Us](int c)  {
      Us.second.push_back(c);
    });
    return std::numeric_limits<int>::max();
  }

  int draw(config& C, int size,
           std::pair<urn, urn>& Us) {
    double d = drand48();

    if      (d < ran)      return draw_ran(C, size, Us);
    else if (d < ran+seq)  return draw_seq(C, size, Us);
    else                   return draw_rad(C, size, Us);
  }

/*
  returns
  -  std::numeric_limits<int>::max() for ran
  -  center city for rad
  -  -(1+start) city for seq
*/
  int ruin(config& C, std::pair<urn, urn>& Us) {
    return draw(C, ceil(drand48() * (siz * N)), Us);
  }


  void recreate(config& C, std::pair<urn, urn>& Us) {
    while (!Us.first.empty()) {
      int c = edraw(Us.first);
      typename config::iterator itend = C.end(); assert(C[c] == itend);
_start
      int mincost = std::numeric_limits<int>::max();
      int prev = C.empty() ? -1 : C.back();
      typename config::iterator best = C.end();
      for (typename config::iterator it = C.begin(); it != C.end(); ++it) {
        int cur = *it;
        int ncost = D[prev][c] + D[c][cur] - D[prev][cur];
        if (ncost < mincost) {
          best = it;
          mincost = ncost;
        }
        prev = cur;
      }
_stop
      C.insert(best, c);
    }
  }


  int D[N][N];           // distance matrix

  struct {
    int* vi;
    bool  operator()(int a, int b)  {
      return vi[a] < vi[b];
    }
  } Dless;

  std::vector<int> rad_nxt[N];     // radial next

  void init_dist() {
    for (int from = 0; from < N; ++from) {
      rad_nxt[from].clear();
      for (int to = 0; to < N; ++to) {
        D[from][to] = dist(from, to);
        rad_nxt[from].push_back(to);
      }
    }

    for (int from = 0; from < N; ++from) {
      Dless.vi = D[from];
      std::sort(rad_nxt[from].begin(), rad_nxt[from].end(), Dless);
    }
  }


  inline int nint(double d) { return static_cast<int>(0.5 + d); }
  // http://comopt.ifi.uni-heidelberg.de/software/TSPLIB95/tsp95.pdf#page=6
  // $ grep EDGE_WEIGHT_TYPE pcb442.tsp
  // EDGE_WEIGHT_TYPE : EUC_2D
  // $
  int dist(int from, int to) {
    double xd = C[from][0] - C[to][0];
    double yd = C[from][1] - C[to][1];
    return nint(sqrt(xd*xd + yd*yd));
  }

  // http://comopt.ifi.uni-heidelberg.de/software/TSPLIB95/tsp/pcb442.tsp.gz
  const double C[N][2]={
    {2.00000e+02, 4.00000e+02},
    {2.00000e+02, 5.00000e+02},
    {2.00000e+02, 6.00000e+02},
    {2.00000e+02, 7.00000e+02},
    {2.00000e+02, 8.00000e+02},
    {2.00000e+02, 9.00000e+02},
    {2.00000e+02, 1.00000e+03},
    {2.00000e+02, 1.10000e+03},
    {2.00000e+02, 1.20000e+03},
    {2.00000e+02, 1.30000e+03},
    {2.00000e+02, 1.40000e+03},
    {2.00000e+02, 1.50000e+03},
    {2.00000e+02, 1.60000e+03},
    {2.00000e+02, 1.70000e+03},
    {2.00000e+02, 1.80000e+03},
    {2.00000e+02, 1.90000e+03},
    {2.00000e+02, 2.00000e+03},
    {2.00000e+02, 2.10000e+03},
    {2.00000e+02, 2.20000e+03},
    {2.00000e+02, 2.30000e+03},
    {2.00000e+02, 2.40000e+03},
    {2.00000e+02, 2.50000e+03},
    {2.00000e+02, 2.60000e+03},
    {2.00000e+02, 2.70000e+03},
    {2.00000e+02, 2.80000e+03},
    {2.00000e+02, 2.90000e+03},
    {2.00000e+02, 3.00000e+03},
    {2.00000e+02, 3.10000e+03},
    {2.00000e+02, 3.20000e+03},
    {2.00000e+02, 3.30000e+03},
    {2.00000e+02, 3.40000e+03},
    {2.00000e+02, 3.50000e+03},
    {2.00000e+02, 3.60000e+03},
    {3.00000e+02, 4.00000e+02},
    {3.00000e+02, 5.00000e+02},
    {3.00000e+02, 6.00000e+02},
    {3.00000e+02, 7.00000e+02},
    {3.00000e+02, 8.00000e+02},
    {3.00000e+02, 9.00000e+02},
    {3.00000e+02, 1.00000e+03},
    {3.00000e+02, 1.10000e+03},
    {3.00000e+02, 1.20000e+03},
    {3.00000e+02, 1.30000e+03},
    {3.00000e+02, 1.40000e+03},
    {3.00000e+02, 1.50000e+03},
    {3.00000e+02, 1.60000e+03},
    {3.00000e+02, 1.70000e+03},
    {3.00000e+02, 1.80000e+03},
    {3.00000e+02, 1.90000e+03},
    {3.00000e+02, 2.00000e+03},
    {3.00000e+02, 2.10000e+03},
    {3.00000e+02, 2.20000e+03},
    {3.00000e+02, 2.30000e+03},
    {3.00000e+02, 2.40000e+03},
    {3.00000e+02, 2.50000e+03},
    {3.00000e+02, 2.60000e+03},
    {3.00000e+02, 2.70000e+03},
    {3.00000e+02, 2.80000e+03},
    {3.00000e+02, 2.90000e+03},
    {3.00000e+02, 3.00000e+03},
    {3.00000e+02, 3.10000e+03},
    {3.00000e+02, 3.20000e+03},
    {3.00000e+02, 3.30000e+03},
    {3.00000e+02, 3.40000e+03},
    {3.00000e+02, 3.50000e+03},
    {4.00000e+02, 4.00000e+02},
    {4.00000e+02, 5.00000e+02},
    {4.00000e+02, 6.00000e+02},
    {4.00000e+02, 7.00000e+02},
    {4.00000e+02, 8.00000e+02},
    {4.00000e+02, 9.00000e+02},
    {4.00000e+02, 1.00000e+03},
    {4.00000e+02, 1.10000e+03},
    {4.00000e+02, 1.20000e+03},
    {4.00000e+02, 1.30000e+03},
    {4.00000e+02, 1.40000e+03},
    {4.00000e+02, 1.50000e+03},
    {4.00000e+02, 1.60000e+03},
    {4.00000e+02, 1.70000e+03},
    {4.00000e+02, 1.80000e+03},
    {4.00000e+02, 1.90000e+03},
    {4.00000e+02, 2.00000e+03},
    {4.00000e+02, 2.10000e+03},
    {4.00000e+02, 2.20000e+03},
    {4.00000e+02, 2.30000e+03},
    {4.00000e+02, 2.40000e+03},
    {4.00000e+02, 2.50000e+03},
    {4.00000e+02, 2.60000e+03},
    {4.00000e+02, 2.70000e+03},
    {4.00000e+02, 2.80000e+03},
    {4.00000e+02, 2.90000e+03},
    {4.00000e+02, 3.00000e+03},
    {4.00000e+02, 3.10000e+03},
    {4.00000e+02, 3.20000e+03},
    {4.00000e+02, 3.30000e+03},
    {4.00000e+02, 3.40000e+03},
    {4.00000e+02, 3.50000e+03},
    {4.00000e+02, 3.60000e+03},
    {5.00000e+02, 1.50000e+03},
    {5.00000e+02, 1.82900e+03},
    {5.00000e+02, 3.10000e+03},
    {6.00000e+02, 4.00000e+02},
    {7.00000e+02, 3.00000e+02},
    {7.00000e+02, 6.00000e+02},
    {7.00000e+02, 1.50000e+03},
    {7.00000e+02, 1.60000e+03},
    {7.00000e+02, 1.80000e+03},
    {7.00000e+02, 2.10000e+03},
    {7.00000e+02, 2.40000e+03},
    {7.00000e+02, 2.70000e+03},
    {7.00000e+02, 3.00000e+03},
    {7.00000e+02, 3.30000e+03},
    {7.00000e+02, 3.60000e+03},
    {8.00000e+02, 3.00000e+02},
    {8.00000e+02, 6.00000e+02},
    {8.00000e+02, 1.03000e+03},
    {8.00000e+02, 1.50000e+03},
    {8.00000e+02, 1.80000e+03},
    {8.00000e+02, 2.10000e+03},
    {8.00000e+02, 2.40000e+03},
    {8.00000e+02, 2.60000e+03},
    {8.00000e+02, 2.70000e+03},
    {8.00000e+02, 3.00000e+03},
    {8.00000e+02, 3.30000e+03},
    {8.00000e+02, 3.60000e+03},
    {9.00000e+02, 3.00000e+02},
    {9.00000e+02, 6.00000e+02},
    {9.00000e+02, 1.50000e+03},
    {9.00000e+02, 1.80000e+03},
    {9.00000e+02, 2.10000e+03},
    {9.00000e+02, 2.40000e+03},
    {9.00000e+02, 2.70000e+03},
    {9.00000e+02, 3.00000e+03},
    {9.00000e+02, 3.30000e+03},
    {9.00000e+02, 3.60000e+03},
    {1.00000e+03, 3.00000e+02},
    {1.00000e+03, 6.00000e+02},
    {1.00000e+03, 1.10000e+03},
    {1.00000e+03, 1.50000e+03},
    {1.00000e+03, 1.62900e+03},
    {1.00000e+03, 1.80000e+03},
    {1.00000e+03, 2.10000e+03},
    {1.00000e+03, 2.40000e+03},
    {1.00000e+03, 2.60000e+03},
    {1.00000e+03, 2.70000e+03},
    {1.00000e+03, 3.00000e+03},
    {1.00000e+03, 3.30000e+03},
    {1.00000e+03, 3.60000e+03},
    {1.10000e+03, 3.00000e+02},
    {1.10000e+03, 6.00000e+02},
    {1.10000e+03, 7.00000e+02},
    {1.10000e+03, 9.00000e+02},
    {1.10000e+03, 1.50000e+03},
    {1.10000e+03, 1.80000e+03},
    {1.10000e+03, 2.10000e+03},
    {1.10000e+03, 2.40000e+03},
    {1.10000e+03, 2.70000e+03},
    {1.10000e+03, 3.00000e+03},
    {1.10000e+03, 3.30000e+03},
    {1.10000e+03, 3.60000e+03},
    {1.20000e+03, 3.00000e+02},
    {1.20000e+03, 6.00000e+02},
    {1.20000e+03, 1.50000e+03},
    {1.20000e+03, 1.70000e+03},
    {1.20000e+03, 1.80000e+03},
    {1.20000e+03, 2.10000e+03},
    {1.20000e+03, 2.40000e+03},
    {1.20000e+03, 2.70000e+03},
    {1.20000e+03, 3.00000e+03},
    {1.20000e+03, 3.30000e+03},
    {1.20000e+03, 3.60000e+03},
    {1.30000e+03, 3.00000e+02},
    {1.30000e+03, 6.00000e+02},
    {1.30000e+03, 7.00000e+02},
    {1.30000e+03, 1.13000e+03},
    {1.30000e+03, 1.50000e+03},
    {1.30000e+03, 1.80000e+03},
    {1.30000e+03, 2.10000e+03},
    {1.30000e+03, 2.20000e+03},
    {1.30000e+03, 2.40000e+03},
    {1.30000e+03, 2.70000e+03},
    {1.30000e+03, 3.00000e+03},
    {1.30000e+03, 3.30000e+03},
    {1.30000e+03, 3.60000e+03},
    {1.40000e+03, 3.00000e+02},
    {1.40000e+03, 6.00000e+02},
    {1.40000e+03, 9.30000e+02},
    {1.40000e+03, 1.50000e+03},
    {1.40000e+03, 1.80000e+03},
    {1.40000e+03, 2.00000e+03},
    {1.40000e+03, 2.10000e+03},
    {1.40000e+03, 2.40000e+03},
    {1.40000e+03, 2.50000e+03},
    {1.40000e+03, 2.70000e+03},
    {1.40000e+03, 2.82000e+03},
    {1.40000e+03, 2.90000e+03},
    {1.40000e+03, 3.00000e+03},
    {1.40000e+03, 3.30000e+03},
    {1.40000e+03, 3.60000e+03},
    {1.50000e+03, 1.50000e+03},
    {1.50000e+03, 1.80000e+03},
    {1.50000e+03, 1.90000e+03},
    {1.50000e+03, 2.10000e+03},
    {1.50000e+03, 2.40000e+03},
    {1.50000e+03, 2.70000e+03},
    {1.50000e+03, 2.80000e+03},
    {1.50000e+03, 2.86000e+03},
    {1.50000e+03, 3.00000e+03},
    {1.50000e+03, 3.30000e+03},
    {1.50000e+03, 3.60000e+03},
    {1.60000e+03, 1.10000e+03},
    {1.60000e+03, 1.30000e+03},
    {1.60000e+03, 1.50000e+03},
    {1.60000e+03, 1.80000e+03},
    {1.60000e+03, 2.10000e+03},
    {1.60000e+03, 2.40000e+03},
    {1.60000e+03, 2.70000e+03},
    {1.60000e+03, 3.00000e+03},
    {1.60000e+03, 3.30000e+03},
    {1.60000e+03, 3.60000e+03},
    {1.70000e+03, 1.20000e+03},
    {1.70000e+03, 1.50000e+03},
    {1.70000e+03, 1.80000e+03},
    {1.70000e+03, 2.10000e+03},
    {1.70000e+03, 2.40000e+03},
    {1.70000e+03, 3.60000e+03},
    {1.80000e+03, 3.00000e+02},
    {1.80000e+03, 6.00000e+02},
    {1.80000e+03, 1.23000e+03},
    {1.80000e+03, 1.50000e+03},
    {1.80000e+03, 1.80000e+03},
    {1.80000e+03, 2.10000e+03},
    {1.80000e+03, 2.40000e+03},
    {1.90000e+03, 3.00000e+02},
    {1.90000e+03, 6.00000e+02},
    {1.90000e+03, 3.00000e+03},
    {1.90000e+03, 3.52000e+03},
    {2.00000e+03, 3.00000e+02},
    {2.00000e+03, 3.70000e+02},
    {2.00000e+03, 6.00000e+02},
    {2.00000e+03, 8.00000e+02},
    {2.00000e+03, 9.00000e+02},
    {2.00000e+03, 1.00000e+03},
    {2.00000e+03, 1.10000e+03},
    {2.00000e+03, 1.20000e+03},
    {2.00000e+03, 1.30000e+03},
    {2.00000e+03, 1.40000e+03},
    {2.00000e+03, 1.50000e+03},
    {2.00000e+03, 1.60000e+03},
    {2.00000e+03, 1.70000e+03},
    {2.00000e+03, 1.80000e+03},
    {2.00000e+03, 1.90000e+03},
    {2.00000e+03, 2.00000e+03},
    {2.00000e+03, 2.10000e+03},
    {2.00000e+03, 2.20000e+03},
    {2.00000e+03, 2.30000e+03},
    {2.00000e+03, 2.40000e+03},
    {2.00000e+03, 2.50000e+03},
    {2.00000e+03, 2.60000e+03},
    {2.00000e+03, 2.70000e+03},
    {2.00000e+03, 2.80000e+03},
    {2.00000e+03, 2.90000e+03},
    {2.00000e+03, 3.00000e+03},
    {2.00000e+03, 3.10000e+03},
    {2.00000e+03, 3.50000e+03},
    {2.10000e+03, 3.00000e+02},
    {2.10000e+03, 6.00000e+02},
    {2.10000e+03, 3.20000e+03},
    {2.20000e+03, 3.00000e+02},
    {2.20000e+03, 4.69000e+02},
    {2.20000e+03, 6.00000e+02},
    {2.20000e+03, 3.20000e+03},
    {2.30000e+03, 3.00000e+02},
    {2.30000e+03, 6.00000e+02},
    {2.30000e+03, 3.40000e+03},
    {2.40000e+03, 3.00000e+02},
    {2.40000e+03, 6.00000e+02},
    {2.40000e+03, 2.10000e+03},
    {2.50000e+03, 3.00000e+02},
    {2.50000e+03, 8.00000e+02},
    {2.60000e+03, 4.00000e+02},
    {2.60000e+03, 5.00000e+02},
    {2.60000e+03, 8.00000e+02},
    {2.60000e+03, 9.00000e+02},
    {2.60000e+03, 1.00000e+03},
    {2.60000e+03, 1.10000e+03},
    {2.60000e+03, 1.20000e+03},
    {2.60000e+03, 1.30000e+03},
    {2.60000e+03, 1.40000e+03},
    {2.60000e+03, 1.50000e+03},
    {2.60000e+03, 1.60000e+03},
    {2.60000e+03, 1.70000e+03},
    {2.60000e+03, 1.80000e+03},
    {2.60000e+03, 1.90000e+03},
    {2.60000e+03, 2.00000e+03},
    {2.60000e+03, 2.10000e+03},
    {2.60000e+03, 2.20000e+03},
    {2.60000e+03, 2.30000e+03},
    {2.60000e+03, 2.40000e+03},
    {2.60000e+03, 2.50000e+03},
    {2.60000e+03, 2.60000e+03},
    {2.60000e+03, 2.70000e+03},
    {2.60000e+03, 2.80000e+03},
    {2.60000e+03, 2.90000e+03},
    {2.60000e+03, 3.00000e+03},
    {2.60000e+03, 3.10000e+03},
    {2.60000e+03, 3.40000e+03},
    {2.70000e+03, 7.00000e+02},
    {2.70000e+03, 8.00000e+02},
    {2.70000e+03, 9.00000e+02},
    {2.70000e+03, 1.00000e+03},
    {2.70000e+03, 1.10000e+03},
    {2.70000e+03, 1.20000e+03},
    {2.70000e+03, 1.30000e+03},
    {2.70000e+03, 1.40000e+03},
    {2.70000e+03, 1.50000e+03},
    {2.70000e+03, 1.60000e+03},
    {2.70000e+03, 1.70000e+03},
    {2.70000e+03, 1.80000e+03},
    {2.70000e+03, 1.90000e+03},
    {2.70000e+03, 2.00000e+03},
    {2.70000e+03, 2.10000e+03},
    {2.70000e+03, 2.20000e+03},
    {2.70000e+03, 2.30000e+03},
    {2.70000e+03, 2.50000e+03},
    {2.70000e+03, 2.60000e+03},
    {2.70000e+03, 2.70000e+03},
    {2.70000e+03, 2.80000e+03},
    {2.70000e+03, 2.90000e+03},
    {2.70000e+03, 3.00000e+03},
    {2.70000e+03, 3.10000e+03},
    {2.70000e+03, 3.20000e+03},
    {2.70000e+03, 3.30000e+03},
    {2.70000e+03, 3.40000e+03},
    {2.70000e+03, 3.50000e+03},
    {2.70000e+03, 3.60000e+03},
    {2.70000e+03, 3.70000e+03},
    {2.70000e+03, 3.80000e+03},
    {2.80000e+03, 9.00000e+02},
    {2.80000e+03, 1.13000e+03},
    {2.90000e+03, 4.00000e+02},
    {2.90000e+03, 5.00000e+02},
    {2.90000e+03, 1.40000e+03},
    {2.90000e+03, 2.40000e+03},
    {2.90000e+03, 3.00000e+03},
    {3.00000e+03, 7.00000e+02},
    {3.00000e+03, 8.00000e+02},
    {3.00000e+03, 9.00000e+02},
    {3.00000e+03, 1.00000e+03},
    {3.00000e+03, 1.10000e+03},
    {3.00000e+03, 1.20000e+03},
    {3.00000e+03, 1.30000e+03},
    {3.00000e+03, 1.50000e+03},
    {3.00000e+03, 1.60000e+03},
    {3.00000e+03, 1.70000e+03},
    {3.00000e+03, 1.80000e+03},
    {3.00000e+03, 1.90000e+03},
    {3.00000e+03, 2.00000e+03},
    {3.00000e+03, 2.10000e+03},
    {3.00000e+03, 2.20000e+03},
    {3.00000e+03, 2.30000e+03},
    {3.00000e+03, 2.50000e+03},
    {3.00000e+03, 2.60000e+03},
    {3.00000e+03, 2.70000e+03},
    {3.00000e+03, 2.80000e+03},
    {3.00000e+03, 2.90000e+03},
    {3.00000e+03, 3.00000e+03},
    {3.00000e+03, 3.10000e+03},
    {3.00000e+03, 3.20000e+03},
    {3.00000e+03, 3.30000e+03},
    {3.00000e+03, 3.40000e+03},
    {3.00000e+03, 3.50000e+03},
    {3.00000e+03, 3.60000e+03},
    {3.00000e+03, 3.70000e+03},
    {3.00000e+03, 3.80000e+03},
    {1.50000e+02, 3.50000e+03},
    {1.50000e+02, 3.55000e+03},
    {4.69000e+02, 2.55000e+03},
    {4.69000e+02, 3.35000e+03},
    {4.69000e+02, 3.45000e+03},
    {5.40000e+02, 2.33000e+03},
    {5.40000e+02, 2.43000e+03},
    {6.20000e+02, 3.65000e+03},
    {6.20000e+02, 3.70900e+03},
    {7.50000e+02, 2.55000e+03},
    {8.50000e+02, 5.20000e+02},
    {8.50000e+02, 7.00000e+02},
    {8.50000e+02, 2.28000e+03},
    {9.39000e+02, 7.40000e+02},
    {9.50000e+02, 2.22000e+03},
    {9.10000e+02, 2.60000e+03},
    {1.05000e+03, 1.05000e+03},
    {1.15000e+03, 1.35000e+03},
    {1.17000e+03, 2.28000e+03},
    {1.22000e+03, 2.21000e+03},
    {1.35000e+03, 7.50000e+02},
    {1.35000e+03, 1.70000e+03},
    {1.35000e+03, 2.14000e+03},
    {1.45000e+03, 7.70000e+02},
    {1.55000e+03, 3.00000e+02},
    {1.55000e+03, 5.00000e+02},
    {1.55000e+03, 1.85000e+03},
    {1.65000e+03, 1.05000e+03},
    {1.69000e+03, 2.68000e+03},
    {1.71000e+03, 3.10000e+02},
    {1.71000e+03, 5.10000e+02},
    {1.75000e+03, 7.50000e+02},
    {1.79000e+03, 2.58000e+03},
    {1.72000e+03, 2.61000e+03},
    {1.79000e+03, 3.33000e+03},
    {1.72000e+03, 3.40900e+03},
    {1.82900e+03, 2.70000e+03},
    {1.82900e+03, 2.80000e+03},
    {1.82900e+03, 3.45000e+03},
    {2.06000e+03, 1.65000e+03},
    {2.05000e+03, 3.15000e+03},
    {2.17000e+03, 1.90000e+03},
    {2.11000e+03, 2.00000e+03},
    {2.12000e+03, 2.75000e+03},
    {2.15000e+03, 3.25000e+03},
    {2.29000e+03, 1.40000e+03},
    {2.22000e+03, 2.82000e+03},
    {2.28000e+03, 3.25000e+03},
    {2.39000e+03, 1.30000e+03},
    {2.32000e+03, 1.50000e+03},
    {2.45000e+03, 7.10000e+02},
    {2.62000e+03, 3.65000e+03},
    {2.75000e+03, 5.20000e+02},
    {2.76000e+03, 2.36000e+03},
    {2.85000e+03, 2.20000e+03},
    {2.85000e+03, 2.70000e+03},
    {2.85000e+03, 3.35000e+03},
    {2.93000e+03, 9.50000e+02},
    {2.95000e+03, 1.75000e+03},
    {2.95000e+03, 2.05000e+03},
    {5.20000e+02, 3.20000e+03},
    {2.30000e+03, 3.50000e+03},
    {2.32000e+03, 3.15000e+03},
    {5.30000e+02, 2.10000e+03},
    {2.55000e+03, 7.10000e+02},
    {7.50000e+02, 4.90000e+02},
    {0.00000e+00, 0.00000e+00}
  };

  // http://comopt.ifi.uni-heidelberg.de/software/TSPLIB95/tsp/pcb442.opt.tour.gz
  // (optimal tour, 1-based)
  const int Opt[N]={
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 53,
    52, 51, 83, 84, 85, 381, 382, 86, 54, 21, 22, 55, 87, 378, 88, 56, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 376, 377, 33, 65, 64, 63, 62, 61, 60, 59,
    58, 57, 89, 90, 91, 92, 93, 101, 111, 123, 133, 146, 158, 169, 182, 197,
    196, 195, 194, 181, 168, 157, 145, 144, 391, 132, 122, 110, 121, 385, 109,
    120, 388, 131, 143, 156, 167, 180, 193, 192, 204, 216, 225, 233, 408, 409,
    412, 413, 404, 217, 205, 206, 207, 208, 218, 219, 209, 198, 183, 170, 159,
    147, 134, 124, 112, 436, 94, 95, 379, 96, 380, 97, 98, 384, 383, 113, 125,
    135, 148, 160, 171, 184, 199, 210, 220, 226, 411, 410, 414, 237, 265, 437,
    275, 423, 438, 272, 420, 268, 416, 264, 236, 263, 262, 261, 422, 419, 260,
    259, 258, 257, 256, 255, 254, 253, 418, 417, 252, 251, 250, 415, 249, 248,
    247, 246, 245, 244, 243, 242, 241, 407, 228, 235, 240, 267, 271, 270, 274,
    277, 426, 280, 440, 308, 309, 283, 284, 310, 339, 311, 285, 286, 312, 340,
    313, 287, 288, 314, 315, 316, 290, 289, 424, 421, 425, 291, 317, 318, 292,
    293, 319, 320, 294, 295, 321, 322, 296, 278, 297, 323, 430, 429, 324, 298,
    299, 300, 325, 326, 301, 302, 327, 328, 303, 304, 329, 330, 305, 306, 331,
    332, 333, 432, 334, 307, 335, 336, 427, 337, 338, 375, 374, 373, 372, 371,
    370, 369, 368, 345, 367, 366, 365, 431, 364, 363, 362, 344, 361, 360, 359,
    435, 358, 357, 356, 434, 355, 354, 353, 343, 352, 351, 350, 349, 433, 348,
    347, 346, 342, 341, 428, 282, 281, 279, 276, 273, 269, 266, 239, 238, 234,
    227, 405, 406, 401, 400, 185, 172, 161, 149, 136, 126, 114, 103, 102, 441,
    104, 115, 386, 127, 387, 389, 116, 138, 392, 152, 151, 137, 150, 162, 173,
    186, 174, 396, 399, 187, 175, 211, 403, 221, 229, 212, 230, 222, 213, 200,
    188, 176, 163, 393, 153, 139, 140, 128, 117, 105, 106, 107, 118, 129, 141,
    154, 165, 164, 397, 177, 189, 201, 202, 402, 214, 223, 231, 232, 224, 215,
    203, 190, 191, 398, 178, 179, 395, 394, 166, 155, 142, 390, 130, 119, 108,
    439, 82, 50, 49, 81, 100, 80, 48, 47, 79, 78, 46, 45, 77, 99, 76, 44, 43,
    75, 74, 42, 41, 73, 72, 40, 39, 71, 70, 38, 37, 69, 68, 36, 35, 67, 66, 34,
    442
  };
};  // class pcb442


void errlog(int i, int v, const std::string& trailer = "") {
  if (i >= 0)  std::cerr << i << ": ";
  std::cerr << v << "           " << trailer << "\r";
  if (i < 0)  std::cerr << "\n";
}

#ifdef ezxdisp
const int mar = 8;
const int wid = 3000;
const int hei = 3800;
const int Div = 5;

void mp(int x, int y, int s, std::pair<int, int>& a) {
  a = std::pair<int, int>(mar+x/Div+s*(2*mar+wid/Div), mar+(hei-y)/Div);
}

void city(const std::pair<int, int>& c, ezx_t *e) {
  ezx_fillrect_2d(e, c.first-2, c.second-2, c.first+2, c.second+2, &ezx_black);
}

void city2(const std::pair<int, int>& c, ezx_t *e) {
  ezx_fillrect_2d(e, c.first-4, c.second-4, c.first+4, c.second+4, &ezx_orange);
}

void city2a(const std::pair<int, int>& c, ezx_t *e) {
  ezx_fillrect_2d(e, c.first-6, c.second-6, c.first+6, c.second+6, &ezx_orange);
}

template <typename config, typename urn>
void ezx_tours(pcb442<config, urn>& P, config& T, config& R, urn& U, config& N,
               int ret, int mut, ezx_t*& e) {
  bool initial = (ret == std::numeric_limits<int>::min());
  ezx_wipe(e);

  ezx_line_2d(e, wid/Div+2*mar, hei/Div+2*mar, wid/Div+2*mar, 0, &ezx_black, 1);
  ezx_line_2d(e, 2*(wid/Div+2*mar), hei/Div+2*mar, 2*(wid/Div+2*mar), 0,
              &ezx_black, 1);

  ezx_str_2d(e, 5, 10, const_cast<char *>(reinterpret_cast<const char*>
                         (initial ? (mut == 0 ? "RR_all" : "minimum found")
                                  : "previous")), &ezx_black);
  std::stringstream s2;
  s2 << P.cost(T);
  ezx_str_2d(e, 50, hei/Div+mar,
             const_cast<char *>(reinterpret_cast<const char*>
              (s2.str().c_str())), &ezx_black);
  if (ret != std::numeric_limits<int>::min()) {
    s2 = std::stringstream();
    s2 << mut << ": ";
    if (ret == std::numeric_limits<int>::max())
      s2 << "ran";
    else
      s2 << (ret >= 0 ? "rad" : "seq");
    s2 << "(" << U.size() << ") ";
    s2 << P.cost(R);
    ezx_str_2d(e, 50+wid/Div+2*mar, hei/Div+mar,
               const_cast<char*>(reinterpret_cast<const char *>
                 (s2.str().c_str())), &ezx_black);
    s2 = std::stringstream();
    s2 << P.cost(N) << " (" << P.cost(N) - P.cost(T) << ")";
    ezx_str_2d(e, 50+2*(wid/Div+2*mar), hei/Div+mar,
               const_cast<char*>(reinterpret_cast<const char *>
                 (s2.str().c_str())), &ezx_black);

    ezx_str_2d(e, 5+wid/Div+2*mar, 10,
               const_cast<char*>(reinterpret_cast<const char*>("ruined")),
               &ezx_black);
    ezx_str_2d(e, 5+2*(wid/Div+2*mar), 10,
               const_cast<char*>(reinterpret_cast<const char*>("recreated")),
               &ezx_black);
  } else if (mut != 0) {
    ezx_str_2d(e, 5+wid/Div+2*mar, 10,
               const_cast<char*>(reinterpret_cast<const char*>
                 ("global minimum")), &ezx_black);

    s2 = std::stringstream();
    s2 << P.cost(R);
    ezx_str_2d(e, 50+wid/Div+2*mar, hei/Div+mar,
               const_cast<char*>(reinterpret_cast<const char *>
                 (s2.str().c_str())), &ezx_black);
  }

  if (ret != std::numeric_limits<int>::max() && ret >= 0) {
    std::pair<int, int> c;
    mp(P.C[ret][0], P.C[ret][1], 0, c);
    int r = P.D[ret][P.rad_nxt[ret][U.size()-1]];
    ezx_circle_2d(e, c.first, c.second, r/Div, &ezx_orange, 2);
    ezx_circle_2d(e, c.first+2*(wid/Div+2*mar), c.second, r/Div,
                  &ezx_orange, 2);
  }

  int prev = T.back();
  std::pair<int, int> p;
  mp(P.C[prev][0], P.C[prev][1], 0, p);

  std::for_each(T.begin(), T.end(), [&p, &e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i][0], P.C[i][1], 0, c);
    ezx_line_2d(e, p.first, p.second, c.first, c.second, &ezx_blue, 1);
    p = c;
  });

  int hig = (ret < 0) ? -(1 + ret)
                      : (ret != std::numeric_limits<int>::max()) ? ret : -1;
  std::for_each(U.begin(), U.end(), [hig, &e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i][0], P.C[i][1], 1, c);
    if (i == hig) { city2a(c, e); } else { city2(c, e); }
  });

  prev = R.back();
  mp(P.C[prev][0], P.C[prev][1], 1,  p);

  std::for_each(R.begin(), R.end(), [&p, &e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i][0], P.C[i][1], 1, c);
    ezx_line_2d(e, p.first, p.second, c.first, c.second, &ezx_blue, 1);
    p = c;
  });

  prev = N.back();
  mp(P.C[prev][0], P.C[prev][1], 2,  p);

  std::for_each(N.begin(), N.end(), [&p, &e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i][0], P.C[i][1], 2, c);
    ezx_line_2d(e, p.first, p.second, c.first, c.second, &ezx_blue, 1);
    p = c;
  });

  std::for_each(T.begin(), T.end(), [ret, &e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i][0], P.C[i][1], 0, c);
    city(c, e);
    if (ret != std::numeric_limits<int>::min()) {
      mp(P.C[i][0], P.C[i][1], 2, c);
      city(c, e);
    }
  });

  std::for_each(R.begin(), R.end(), [&e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i][0], P.C[i][1], 1, c);
    city(c, e);
  });

  // ezx_window_name(e, "57123 -> 53207 -> 51219");
  ezx_redraw(e);
  usleep(10000);
}
#endif

template <typename config, typename urn>
void RR() {
  std::pair<urn, urn> Us;
  config T;
  pcb442<config, urn> P(0.3,  1.0/3, 1.0/3, 1.0/3);

#ifdef ezxdisp
  ezx_t *e = ezx_init(3*(wid/Div+2*mar), hei/Div+2*mar,
                      const_cast<char*>(reinterpret_cast<const char*>
                      ("TSP greedy Ruin and Recreate")));
#endif

  P.RR_all(T, Us);

  errlog(0, P.cost(T), "RR_all() [" + i2s(_sum) + "us]");
  _sum = 0;

#ifdef ezxdisp
  config RC;
  urn UC;

  std::cerr << "\n";

  ezx_tours(P, T, RC, Us.first, RC, std::numeric_limits<int>::min(), 0, e);

  (void) ezx_pushbutton(e, NULL, NULL);
#endif

  for (int i = 1; i <= 100000; ++i) {
    config R = T;
    std::pair<urn, urn> UsR;
    for (typename config::iterator it = R.begin(); it != R.end(); ++it) {
      R[*it] = it;
      UsR.second.push_back(*it);
    }

#ifdef ezxdisp
    int ret = P.ruin(R, UsR);
    RC = R;
    UC = UsR.first;
#else
    (void) P.ruin(R, UsR);
#endif

    auto oldsum = _sum;
    P.recreate(R, UsR);

    if (P.cost(R) < P.cost(T)) {
      P.last += " (" + i2s(_sum - oldsum) + "us)          ";
      errlog(i, P.cost(R), P.last);

#ifdef ezxdisp
      std::cerr << "\n";
      ezx_tours(P, T, RC, UC, R, ret, i, e);

      while (1 != ezx_pushbutton(e, NULL, NULL))  { usleep(10000); }
#endif

      T = R;
      UsR.second.clear();
      for (typename config::iterator it = T.begin(); it != T.end(); ++it) {
        T[*it] = it;
        UsR.second.push_back(*it);
      }
    }
  }
  errlog(-1, P.cost(T), "local minimum found (after 100,000 greedy mutations)");
  errlog(-1, (_sum+500)/1000, "ms (only recreate)");
  // print(T);
  /*
  std::cout << "[";
  bool first = true;
  std::for_each(T.begin(), T.end(), [&first, P](int i) {
    if (!first) std::cout << ","; else first = false;
    std::cout << "[" << P.C[i][0] << "," << P.C[i][1] << "]";
  });
  std::cout << "]\n";
  */

  config O;  // P.Opt is 1-based
  for (int i = 0; i < P.N; ++i)  { int c = P.Opt[i] - 1; O.push_back(c); }
  errlog(-1, P.cost(O), "global minimum");

  config S = T;
  S.sort();
  urn V(S.begin(), S.end());
  for (int i = 0; i < P.N; ++i)  assert(V[i] == i);

#ifdef ezxdisp
  config dummy;
  ezx_tours(P, T, O, Us.first, dummy, std::numeric_limits<int>::min(), -1, e);

  while (3 != ezx_pushbutton(e, NULL, NULL))  { usleep(10000); }
#endif
}


int main(int argc, char *argv[]) {
  srandom(argc > 1 ? atoi(argv[1]) : time(0));

  RR<random_access_list<int, 442>, std::vector<int>>();

  return 0;
}
