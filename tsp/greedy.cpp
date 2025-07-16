/*
   TSP Ruin and Recreate greedy implementation with random+sequential+radial ruins:
   https://www.semanticscholar.org/paper/Record-Breaking-Optimization-Results-Using-the-Ruin-Schrimpf-Schneider/4f80e70e51e368858c3df0787f05c3aa2b9650b4

   make greedy
   make cpplint2
   make cppcheck2

   for tour display
   - append compiler flags "-Dezxdisp -lezx -lX11"
   - after "make install" of ezxdisp repo first: 
   https://github.com/Hermann-SW/ezxdisp?tab=readme-ov-file#support-for-c--use-in-ide
   (left mouse click continues to next accepted mutation and updates display; repeat)
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

#include "./loader.h"

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

template <typename val>
class random_access_list {
  std::list<val> L;
  int N;

 public:
  typedef typename std::list<val>::iterator iterator;
  std::vector<iterator> A;

  void init(int _N) {
    N = _N;
    L = std::list<val>(N);
    A = std::vector<iterator>(N);
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
  int N;
  const double siz, ran, seq, rad;
  std::string last;

  pcb442(std::string fname, double _siz, double _ran, double _seq, double _rad):
    siz(_siz), ran(_ran), seq(_seq), rad(_rad) {
    assert(siz >= 0.0 && siz <= 1.0);
    assert(ran+seq+rad == 1.0);
    assert(ran >= 0.0 && seq >= 0.0 && rad >= 0.0);

    load<coord_t>(fname + ".tsp", C);
    load<city_t>(fname + ".opt.tour", Opt);

    assert(C.size() == Opt.size());

    N = C.size();

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
    C.init(N);
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
    assert(Us.second.size() == static_cast<unsigned>(N));
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


  int **D;           // distance matrix

  struct {
    int* vi;
    bool  operator()(int a, int b)  {
      return vi[a] < vi[b];
    }
  } Dless;

  std::vector<int> *rad_nxt;     // radial next

  void init_dist() {
    typedef int *pint;
    D = new pint[N];
    rad_nxt = new std::vector<int>[N];

    for (int from = 0; from < N; ++from) {
      D[from] = new int[N];
      rad_nxt[from].clear();
      for (int to = 0; to < N; ++to) {
        D[from][to] = dist(C[from], C[to]);
        rad_nxt[from].push_back(to);
      }
    }

    for (int from = 0; from < N; ++from) {
      Dless.vi = D[from];
      std::sort(rad_nxt[from].begin(), rad_nxt[from].end(), Dless);
    }
  }

  std::vector<coord_t> C;
  std::vector<city_t> Opt;
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
    mp(P.C[ret].first, P.C[ret].second, 0, c);
    int r = P.D[ret][P.rad_nxt[ret][U.size()-1]];
    ezx_circle_2d(e, c.first, c.second, r/Div, &ezx_orange, 2);
    ezx_circle_2d(e, c.first+2*(wid/Div+2*mar), c.second, r/Div,
                  &ezx_orange, 2);
  }

  int prev = T.back();
  std::pair<int, int> p;
  mp(P.C[prev].first, P.C[prev].second, 0, p);

  std::for_each(T.begin(), T.end(), [&p, &e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i].first, P.C[i].second, 0, c);
    ezx_line_2d(e, p.first, p.second, c.first, c.second, &ezx_blue, 1);
    p = c;
  });

  int hig = (ret < 0) ? -(1 + ret)
                      : (ret != std::numeric_limits<int>::max()) ? ret : -1;
  std::for_each(U.begin(), U.end(), [hig, &e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i].first, P.C[i].second, 1, c);
    if (i == hig) { city2a(c, e); } else { city2(c, e); }
  });

  prev = R.back();
  mp(P.C[prev].first, P.C[prev].second, 1,  p);

  std::for_each(R.begin(), R.end(), [&p, &e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i].first, P.C[i].second, 1, c);
    ezx_line_2d(e, p.first, p.second, c.first, c.second, &ezx_blue, 1);
    p = c;
  });

  prev = N.back();
  mp(P.C[prev].first, P.C[prev].second, 2,  p);

  std::for_each(N.begin(), N.end(), [&p, &e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i].first, P.C[i].second, 2, c);
    ezx_line_2d(e, p.first, p.second, c.first, c.second, &ezx_blue, 1);
    p = c;
  });

  std::for_each(T.begin(), T.end(), [ret, &e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i].first, P.C[i].second, 0, c);
    city(c, e);
    if (ret != std::numeric_limits<int>::min()) {
      mp(P.C[i].first, P.C[i].second, 2, c);
      city(c, e);
    }
  });

  std::for_each(R.begin(), R.end(), [&e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i].first, P.C[i].second, 1, c);
    city(c, e);
  });

  // ezx_window_name(e, "57123 -> 53207 -> 51219");
  ezx_redraw(e);
  usleep(10000);
}
#endif

template <typename config, typename urn>
void RR(std::string fname) {
  std::pair<urn, urn> Us;
  config T;
  pcb442<config, urn> P(fname, 0.3,  1.0/3, 1.0/3, 1.0/3);

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
    std::cout << "[" << P.C[i].first << "," << P.C[i].second << "]";
  });
  std::cout << "]\n";
  */

  config O;  // P.Opt is 1-based
  O.init(P.N);
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
  assert(argc == 3);
  srandom(argc > 1 ? atoi(argv[1]) : time(0));

  RR<random_access_list<int>, std::vector<int>>(argv[2]);

  return 0;
}
