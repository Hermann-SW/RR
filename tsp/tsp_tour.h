#ifndef TSP_TSP_TOUR_H_
#define TSP_TSP_TOUR_H_

#include <string>
#include <utility>
#include <limits>
#include <algorithm>
#include <vector>

#include "./utils.h"

#ifdef ezxdisp
extern double scale;
extern bool single_display;
extern bool rot270;
extern int wid, hei;
extern int c_radial;
extern int r_radial;
#endif
int glob_min = 0;

template <typename config, typename urn>
class tsp_tour {
 public:
  int N;
  const double siz, ran, seq, rad;
  std::string msg;

  typedef typename config::value_type city_t;

  struct {
#ifdef MEMOPT
    int16_t* vi;
#else
    int* vi;
#endif
    bool  operator()(int a, int b)  {
      return vi[a] < vi[b];
    }
  } Dless;

  tsp_tour(const std::string& fname, double _siz,
                                     double _ran, double _seq, double _rad):
  siz(_siz), ran(_ran), seq(_seq), rad(_rad), Dless(), D(NULL), rad_nxt(NULL) {
    assert(siz >= 0.0 && siz <= 1.0);
    assert(ran+seq+rad == 1.0);
    assert(ran >= 0.0 && seq >= 0.0 && rad >= 0.0);

    load<coord_t>(fname + ".tsp", C);
    load<city_t>(fname + ".opt.tour", Opt);

    assert(C.size() == Opt.size());

    N = C.size();

#ifdef ezxdisp
    if (rot270) {
      for (int i = 0; i < static_cast<int>(C.size()); ++i) {
        double f = C[i].first;
        double s = C[i].second;
        C[i].first = wid-s;
        C[i].second = f;
      }
    }

    double xmin = C[0].first, ymin = C[0].second;
    double xmax = C[0].first, ymax = C[0].second;
    for (int i = 1; i < static_cast<int>(C.size()); ++i) {
      if (C[i].first < xmin)  xmin = C[i].first;
      if (C[i].second < ymin)  ymin = C[i].second;
      if (C[i].first > xmax)  xmax = C[i].first;
      if (C[i].second > ymax)  ymax = C[i].second;
    }
    double dx = xmax-xmin;
    double dy = ymax-ymin;
    double d = dx < dy ? dy : dx;
    scale = d/wid;
    if (single_display) {
      wid = 1800;
      hei = 900;
      if (dx < 2*dy) {
        scale = dy/hei;
      } else {
        scale = dx/wid;
      }
    }
    CC = C;
    for (int i = 0; i < static_cast<int>(C.size()); ++i) {
      C[i].first = (C[i].first - xmin)/scale;
      C[i].second = (C[i].second - ymin)/scale;
    }
#else
    CC = C;
#endif
  }


  int cost(config& C) {
    int cost = 0;
    int prev = C.empty() ? -1 : C.back();
    std::for_each(C.begin(), C.end(), [this, &cost, &prev](const int c) {
                                        cost += D[prev][c]; prev = c;
                                      });
    return cost;
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

  void init(config &C, std::pair<urn, urn> &Us) {
    C.init(N);
    Us.first.clear();
    Us.second.clear();
    for (int i = 0; i < N; ++i)  Us.first.push_back(i);
  }

  void RR_all(config &C, std::pair<urn, urn> &Us, const std::string *src) {
    init(C, Us);
    if (src == NULL) {
      recreate(C, Us);
    } else if (src->starts_with("radial_")) {
      int extc = -1;
      if (src->starts_with("radial_ran")) {
        extc = random() % N;
      } else if (src->starts_with("radial_min")) {
        extc = ext_sum(std::numeric_limits<int>::max(),
                       [](int64_t a, int64_t b){return a-b;});
      } else if (src->starts_with("radial_max")) {
        extc = ext_sum(std::numeric_limits<int>::min(),
                       [](int64_t a, int64_t b){return b-a;});
      }
      assert(extc != -1);
#ifdef ezxdisp
      c_radial = extc;
      r_radial = dist_sum(extc) / N;
#endif
      Us.first.clear();
      for (int j = 0; j < N ; ++j) {
        Us.first.push_back(rad_nxt[extc][j]);
      }
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

  int draw_rad(config& C, int size, std::pair<urn, urn>& Us) {
    auto center = random() % C.size();
    msg = "rad(" + i2s(center) + "," + i2s(size) + ")";
    Us.first.clear();
    std::for_each_n(rad_nxt[center].begin(), size, [&C, &Us](auto& c) {
      C.erase(c);
      Us.first.push_back(c);
    });
    return center;
  }

  int draw_seq(config& C, int size, std::pair<urn, urn>& Us) {
    auto start = random() % C.size();
    msg = "seq(" + i2s(start) + "," + i2s(size) + ")";
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
    msg = "ran(" + i2s(size) + ")";
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
      city_t c = edraw(Us.first);
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

#ifdef MEMOPT
  int16_t **D;           // distance matrix
#else
  int **D;           // distance matrix
#endif

  std::vector<int> *rad_nxt;     // radial next

  void init_dist() {
#ifdef MEMOPT
    typedef int16_t *pint;
#else
    typedef int *pint;
#endif
    D = new pint[N];
    rad_nxt = new std::vector<int>[N];

    for (int from = 0; from < N; ++from) {
#ifdef MEMOPT
      D[from] = new int16_t[N];
#else
      D[from] = new int[N];
#endif
      for (int to = 0; to < N; ++to) {
        D[from][to] = dist(CC[from], CC[to]);
        rad_nxt[from].push_back(to);
      }
    }

    for (int from = 0; from < N; ++from) {
      Dless.vi = D[from];
      std::sort(rad_nxt[from].begin(), rad_nxt[from].end(), Dless);
    }
  }

  void save_tour(config& C, int seed, int m) {
    std::string fname = "solution.sm_"+i2s(seed)+"_"+i2s(m)+".tour";
    std::ofstream os(fname);
    os << "NAME : " << fname << ".tour\n";
    os << "COMMENT : Length " << i2s(cost(C)) << "\n";
    os << "TYPE : TOUR\n";
    os << "DIMENSION : " << i2s(N) << "\n";
    os << "TOUR_SECTION\n";
    print(C, os, '\n', 1);
    os << "-1\nEOF\n";
  }

  std::vector<coord_t> C;
  std::vector<coord_t> CC;
  std::vector<city_t> Opt;
};
#endif  // TSP_TSP_TOUR_H_
