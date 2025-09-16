#ifndef TSP_RR_GREEDY_H_
#define TSP_RR_GREEDY_H_

#include <string>
#include <utility>
#include <limits>
#include <algorithm>

#ifdef ezxdisp
#include "./disp_utils.h"
#endif

int nmutations = 100000;
bool rot270 = false;
std::string *src = NULL;
extern int radc;
extern int rsiz;
extern bool force_irr;

template <typename config, typename urn>
void RR_greedy(const std::string& fname, int seed) {
  std::pair<urn, urn> Us;
  config T;
  tsp_tour<config, urn> P(fname, 0.3,  1.0/3, 1.0/3, 1.0/3);

  if (radc >= 0 && rsiz > 0 && force_irr)  assert(rsiz <= ceil(P.siz*P.N));

#ifdef ezxdisp
  ezx_t *e;
  if (single_display) {
    e = ezx_init(wid+2*marx, hei+2*mary,
                 const_cast<char*>(reinterpret_cast<const char*>
                 ("TSP greedy Ruin and Recreate")));
  } else {
    e = ezx_init(3*(wid+2*marx), hei+2*mary,
                 const_cast<char*>(reinterpret_cast<const char*>
                 ("TSP greedy Ruin and Recreate")));
  }

  ezx_tours0(P, e);
//  if (P.N > 50000)
//     (void) ezx_pushbutton(e, NULL, NULL);
#endif
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
  assert(opt_length == P.Cost(O));

  P.RR_all(T, Us, src);

  errlog(0, P.cost, "RR_all() [" + i2s(_sum) + "us]");
  _sum = 0;

  if (radc >= 0 && rsiz > 0) {
    (void) P.draw_rad(T, radc, rsiz, Us);
    P.recreate(T, Us);

    if (force_irr)  P.force_irr(Us);
  }

#ifdef ezxdisp
  config rui, old;
  urn UC;
  bool confirm = true;

  std::cerr << "\n";

  ezx_tours(P, T, rui, Us.first, rui, std::numeric_limits<int>::min(), 0, e);

  if (nmutations > 0)
     (void) ezx_pushbutton(e, NULL, NULL);
#endif

  int oldcost = P.cost;

  for (int i = 1; i <= nmutations; ++i) {
    P.restore_point(T);

#ifdef ezxdisp
    old = T;
    int ret = P.ruin(T, Us);
    rui = T;
    UC = Us.first;
#else
    (void) P.ruin(T, Us);
#endif

    auto oldsum = _sum;
    P.recreate(T, Us);
    int newcost = P.cost;

    if (newcost < oldcost) {
      oldcost = newcost;
      P.msg += " (" + i2s(_sum - oldsum) + "us)          ";
      errlog(i, newcost, P.msg);

#ifdef ezxdisp
      std::cerr << "\n";
      ezx_tours(P, old, rui, UC, T, ret, i, e);

      if (confirm) {
        int b;

        while (0 == (b = ezx_pushbutton(e, NULL, NULL)))  { usleep(10000); }

        confirm = (b != 3);
      } else {
        confirm = (0 != (ezx_sensebutton(e, NULL, NULL) & EZX_BUTTON_LMASK));
      }
#endif
    } else {
      P.restore(T);
    }
  }
  errlog(-1, P.cost,
         "local minimum found ("+i2s(nmutations)+" greedy mutations; seed="
         +i2s(seed)+")");
  errlog(-1, (_sum+500)/1000, "ms (only recreate)");
  // print<config>(T);
#if 0   // print_coords()
  std::cout << "[";
  bool first = true;
  std::for_each(T.begin(), T.end(), [&first, P](int i) {
    if (!first) { std::cout << ","; } else { first = false; }
    std::cout << "[" << P.C[i].first << "," << P.C[i].second << "]";
  });
  std::cout << "]\n";
#endif

  if (P.N == P.NN) {
    std::sort(Us.second.begin(), Us.second.end());
    for (int i = 0; i < P.N; ++i)  assert(Us.second[i] == i);
  } else {
    assert(Us.second.size() == static_cast<unsigned>(rsiz));
  }

  config S = T;
  S.sort();
  urn V(S.begin(), S.end());
  for (int i = 0; i < P.N; ++i)  assert(V[i] == i);

  P.save_tour(T, seed, nmutations);

#ifdef ezxdisp
  if (radc >= 0 && rsiz > 0) {
    c_radial = radc;
    r_radial = dist(P.CC[radc], P.CC[P.rad_nxt[radc][rsiz-1]]);
  } else if (radc >= 0 && rsiz < -1) {
    c_radial = radc;
    r_radial = dist(P.CC[radc], P.CC[P.rad_nxt[radc][abs(rsiz)-1]]);
  }

  config dummy;
  ezx_tours(P, T, O, Us.first, dummy, std::numeric_limits<int>::min(), -1, e);

  while (3 != ezx_pushbutton(e, NULL, NULL))  { usleep(10000); }
#endif
}
#endif  // TSP_RR_GREEDY_H_
