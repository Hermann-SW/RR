/*
   TSP Ruin and Recreate greedy implementation with random+sequential+radial ruins:
   https://www.semanticscholar.org/paper/Record-Breaking-Optimization-Results-Using-the-Ruin-Schrimpf-Schneider/4f80e70e51e368858c3df0787f05c3aa2b9650b4

   make greedy
   make ezxdisp
   make cpplint2
   make cppcheck2

   for tour display
   - make ezxdisp
   - after "make install" of ezxdisp repo first: 
   https://github.com/Hermann-SW/ezxdisp?tab=readme-ov-file#support-for-c--use-in-ide
   (left mouse click continues to next accepted mutation and updates display; repeat)
*/
#include <unistd.h>

#include <sstream>
#include <iostream>

#ifndef TSP_UTILS_H_
#define TSP_UTILS_H_

#include <sys/time.h>
#include <string>
#include <fstream>
#include <random>

// #include "./tsp_tour.h"

std::mt19937 mtgen;
std::uniform_real_distribution<> dis(0.0, 1.0);

auto _sum = 0;
struct timeval _tv0;
#define _tim gettimeofday(&_tv0, NULL)
#define _start (_tim, _sum -= (1000000*_tv0.tv_sec + _tv0.tv_usec));
#define _stop  (_tim, _sum += (1000000*_tv0.tv_sec + _tv0.tv_usec));

std::string i2s(int x) { std::stringstream s2; s2 << x; return s2.str(); }

template <typename urn>
typename urn::value_type edraw(urn& U) {
  auto r = mtgen() % U.size();
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

#ifndef TSP_LOADER_H_
#define TSP_LOADER_H_
#include <math.h>
#include <fstream>
#include <cassert>
#include <vector>
#include <utility>
#include <string>
#include <type_traits>

template<typename T> struct is_pair_t                  : std::false_type {};
template<typename T> struct is_pair_t<std::pair<T, T>> : std::true_type {};

enum edge_weight_t { ATT, CEIL_2D, EUC_2D, EXPLICIT, GEO, UNDEF };

typedef std::pair<double, double> coord_t;


edge_weight_t edge_weight_type = UNDEF;
int opt_length = -1;


inline int nint(double d) { return static_cast<int>(0.5 + d); }
// http://comopt.ifi.uni-heidelberg.de/software/TSPLIB95/tsp95.pdf#page=6
int euc_2d(const coord_t& from, const coord_t& to) {
  double xd = from.first - to.first;
  double yd = from.second - to.second;
  int d = nint(sqrt(xd*xd + yd*yd));
  assert(d <= 32767);
  return d;
}
int dist(const coord_t& from, const coord_t& to) {
  switch (edge_weight_type) {
    case EUC_2D: return euc_2d(from, to);
    default: assert(!"edge_weight_type not implemented");
  }
}

std::ifstream& operator >> (std::ifstream& in, coord_t &c1) {
  in >> c1.first >> c1.second; return in;
}

std::string value(std::ifstream& src, const std::string& pre) {
  std::string line;
  do { getline(src, line); assert(!src.eof()); } while (!line.starts_with(pre));

  if (line.find(":") == line.npos)  return "";

  std::string rgt = line.substr(line.find(":") + 1);

  if (rgt.find_first_not_of(" \t") != line.npos)
    rgt = rgt.substr(rgt.find_first_not_of(" \t"));

  if (rgt.find_last_not_of(" \t") != line.npos)
    rgt.resize(rgt.find_last_not_of(" \t") + 1);

  return rgt;
}

template <typename vt>
void load(std::string fname, std::vector<vt>& pd) {
  bool ipt = is_pair_t<vt>::value;

  std::ifstream tsp(fname); assert(tsp.is_open());

  if (!ipt) {
    std::string cmt = value(tsp, "COMMENT");
    assert(cmt.find("Length") != cmt.npos);
    cmt = cmt.substr(cmt.find("Length") + 6);
    assert(cmt.find_first_of("0123456789") != cmt.npos);
    cmt = cmt.substr(cmt.find_first_of("0123456789"));
    opt_length = stoi(cmt);
  }

  assert(value(tsp, "TYPE") == (ipt ? "TSP" : "TOUR"));

  int dim = stoi(value(tsp, "DIMENSION"));

  std::string etype;
  if (ipt) { etype = value(tsp, "EDGE_WEIGHT_TYPE"); }

  assert(value(tsp, ipt ? "NODE_COORD_SECTION" : "TOUR_SECTION") == "");

  pd = std::vector<vt>(dim);
  for (int i = 1; i <= dim; ++i) {
    if (ipt) { int id; tsp >> id; assert(i == id); }
    tsp >> pd[i-1];
  }

  if (ipt) {
    edge_weight_type = (etype == "ATT" ? ATT
                        : (etype == "CEIL_2D" ? CEIL_2D
                           : (etype == "EUC_2D" ? EUC_2D
                              // : (etype == "EXPLICIT" ? EXPLICIT
                                    : (assert(etype == "GEO"), GEO))));
  }
}
#endif  // TSP_LOADER_H_

#ifndef TSP_RANDOM_ACCESS_LIST_H_
#define TSP_RANDOM_ACCESS_LIST_H_

#include <list>
#include <vector>

template <typename val>
class random_access_list {
  std::list<val> L, B;
  int N;

 public:
  using value_type = val;
  typedef typename std::list<val>::iterator iterator;
  std::vector<iterator> A;

  random_access_list() {
    init(0);
  }

  void init(int _N) {
    N = _N;
    L = std::list<val>(N);
    A = std::vector<iterator>(N);
    L.clear();
    for (int i = 0; i < N; ++i)  A[i] = L.end();
  }

#define __(_L)                                           \
    N = _L.N;                                            \
    L = _L.L;                                            \
    A = std::vector<iterator>(N);                        \
    for (iterator it = L.begin(); it != L.end(); ++it) { \
      A[*it] = it;                                       \
    }

  random_access_list(const random_access_list& _L) { __(_L) }
  random_access_list operator=(const random_access_list& _L) {
    __(_L) return *this;
  }
#undef __

  // Element access
  iterator& operator[](std::size_t i)  { return A[i]; }
  val& back()  { return L.back(); }

  // Iterators
  iterator begin()  { return L.begin(); }
  iterator end()  { return L.end(); }

  // Capacity
  bool empty()  { return L.empty(); }
  size_t size()  { return L.size(); }

  // Modifiers
  iterator insert(iterator it, val& v )  { return A[v] = L.insert(it, v); }
  iterator erase(iterator it)  { A[*it] = L.end(); return L.erase(it); }
  iterator erase(int i)  {
    iterator it = A[i]; A[i] = L.end(); return L.erase(it);
  }
  void push_back(val& v )  { L.push_back(v); A[v] = --L.end(); }

  // Operations
  void sort()  { L.sort(); }

  void restore_point() { B = L; }
  void restore() { L = B;
    for (iterator it = L.begin(); it != L.end(); ++it) { A[*it] = it; }
  }

  val cyclic_prev(val c) {
    return *--(A[c] == L.begin() ? L.end() : A[c]);
  }
  val cyclic_succ(val c) {
    iterator it = A[c];
    it = ++it;
    if (it == L.end()) it = L.begin();
    return *it;
  }
};

#endif  // TSP_RANDOM_ACCESS_LIST_H_

#ifndef TSP_TSP_TOUR_H_
#define TSP_TSP_TOUR_H_

#include <string>
#include <utility>
#include <limits>
#include <algorithm>
#include <vector>

int glob_min = 0;

template <typename config, typename urn>
class tsp_tour {
 public:
  int N;
  const double siz, ran, seq, rad;
  int cost, bcost;
  std::string msg;

  typedef typename config::value_type city_t;

  struct {
    int16_t* vi;
    bool  operator()(int a, int b)  {
      return vi[a] < vi[b];
    }
  } Dless;

  tsp_tour(const std::string& fname, double _siz,
                                     double _ran, double _seq, double _rad):
    siz(_siz), ran(_ran), seq(_seq), rad(_rad),
    cost(0), bcost(0), Dless(), D(NULL) {
    assert(siz >= 0.0 && siz <= 1.0);
    assert(ran+seq+rad == 1.0);
    assert(ran >= 0.0 && seq >= 0.0 && rad >= 0.0);

    load<coord_t>(fname + ".tsp", C);
    load<city_t>(fname + ".opt.tour", Opt);

    assert(C.size() == Opt.size());

    N = C.size();

    CC = C;
  }


  int Cost(config& C) {
    int cst = 0;
    int prev = C.empty() ? -1 : C.back();
    std::for_each(C.begin(), C.end(), [this, &cst, &prev](const int c) {
                                        cst += D[prev][c]; prev = c;
                                      });
    return cst;
  }

  int64_t dist_sum(city_t c) {
    int64_t sum = 0;  // prevent overflow for eg. usa13509
    for (int j = 0; j < N ; ++j) {
      sum += D[c][j];
    }
    return sum;
  }

  city_t ext_sum(int extsum, int64_t (*comp)(int64_t, int64_t)) {
    int extc = -1;
    for (int i = 0; i < N; ++i) {
      int64_t nsum = dist_sum(i);
      if (comp(nsum, extsum) < 0) { extsum = nsum; extc = i; }
    }
    return extc;
  }

  int delta(config& C, city_t c) {
    int prev = C.cyclic_prev(c);
    int succ = C.cyclic_succ(c);
    return D[prev][succ] - D[prev][c] - D[c][succ];
  }

  void init(config &C, std::pair<urn, urn> &Us) {
    C.init(N);
    Us.first.clear();
    Us.second.clear();
    for (int i = 0; i < N; ++i) {
      Us.first.push_back(i);
      Us.second.push_back(i);
    }
    cost = 0;
  }

  void RR_all(config &C, std::pair<urn, urn> &Us, const std::string *src) {
    init(C, Us);
    if (src == NULL) {
      recreate(C, Us);
    } else {
      std::vector<city_t> vc;
      load<city_t>(*src, vc);
      std::for_each(vc.begin(), vc.end(), [&C](city_t i) {
        city_t ct = i - 1;
        C.push_back(ct);
      });
      Us.first.clear();
    }
  }

  void recreate(config& C, std::pair<urn, urn>& Us) {
    while (!Us.first.empty()) {
      city_t c = edraw(Us.first);
      typename config::iterator itend = C.end(); assert(C[c] == itend);
_start
      int mincost = std::numeric_limits<int>::max();
      int prev = C.empty() ? -1 : C.back();
      typename config::iterator best = C.end();
      for (typename config::iterator it = C.begin(); it != C.end(); ++it) {
        int ncost = D[prev][c] + D[c][*it] - D[prev][*it];
        if (ncost < mincost) {
          best = it;
          mincost = ncost;
        }
        prev = *it;
      }
_stop
      C.insert(best, c);
      if (C.size() > 1) {
        cost += mincost;
      }
    }
  }

  int16_t **D;           // distance matrix

  void init_dist() {
    typedef int16_t *pint;
    D = new pint[N];

    for (int from = 0; from < N; ++from) {
      D[from] = new int16_t[N];
      for (int to = 0; to < N; ++to) {
        D[from][to] = dist(CC[from], CC[to]);
      }
    }
  }

  std::vector<coord_t> C;
  std::vector<coord_t> CC;
  std::vector<city_t> Opt;
};
#endif  // TSP_TSP_TOUR_H_

#ifndef TSP_RR_GREEDY_H_
#define TSP_RR_GREEDY_H_

#include <string>
#include <utility>
#include <limits>
#include <algorithm>

int nmutations = 100000;
std::string *src = NULL;

template <typename config, typename urn>
void RR_greedy(const std::string& fname, int seed) {
  std::pair<urn, urn> Us;
  config T;
  tsp_tour<config, urn> P(fname, 0.3,  1.0/3, 1.0/3, 1.0/3);
  seed *= 1;
_start
  P.init_dist();
_stop
  errlog(-1, -1, "init_dist() [" + i2s(_sum) + "us]");
  _sum = 0;

  config O;  // P.Opt is 1-based
  O.init(P.N);
  for (int i = 0; i < P.N; ++i) {
    typename config::value_type c = P.Opt[i] - 1;
    O.push_back(c);
  }
  errlog(-1, glob_min = P.Cost(O), "global minimum");

  P.RR_all(T, Us, src);

  errlog(0, P.cost, "RR_all() [" + i2s(_sum) + "us]");
}
#endif  // TSP_RR_GREEDY_H_

int seed = time(NULL);

void help(const char *argv0) {
  std::cout << argv0
            << " [-i tour_or_mode] [-h] [-m nmut]"\
               " [-s seed] fname\n";
  std::cout << "  -i: file.tour or "\
               "radial_min/radial_max/radial_ran for RR_all()\n";
  std::cout << "  -h: this help\n";
  std::cout << "  -m: #mutations\n";
  std::cout << "  -s: seed\n";
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  int opt;

  const char *opts = "i:hm:s:";

  while ((opt = getopt(argc, argv, opts)) != -1) {
    switch (opt) {
      case 'h':
        help(argv[0]);
        break;
      case 'i':
        src = new std::string(optarg);
        break;
      case 'm':
        nmutations = atoi(optarg);
        break;
      case 's':
        seed = atoi(optarg);
        break;
      default:
        help(argv[0]);
    }
  }

  if (optind >= argc) {
    std::cout << "Expected argument after options\n";
    return EXIT_SUCCESS;
  }

  mtgen.seed(seed);

  RR_greedy<random_access_list<int>, std::vector<int>>(argv[optind], seed);

  return EXIT_SUCCESS;
}
