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

template <typename config, typename urn>
void RR_greedy(const std::string& fname, int seed) {
  std::pair<urn, urn> Us;
  config T;
  tsp_tour<config, urn> P(fname, 0.3,  1.0/3, 1.0/3, 1.0/3);

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
  if (P.N > 50000)
     (void) ezx_pushbutton(e, NULL, NULL);
#endif
  P.init_dist();

  config O;  // P.Opt is 1-based
  O.init(P.N);
  for (int i = 0; i < P.N; ++i) {
    typename config::value_type c = P.Opt[i] - 1;
    O.push_back(c);
  }
  errlog(-1, glob_min = P.cost(O), "global minimum");

  P.RR_all(T, Us, src);

  errlog(0, P.cost(T), "RR_all() [" + i2s(_sum) + "us]");
  _sum = 0;

#ifdef ezxdisp
  config rui, old;
  urn UC;
  bool confirm = true;

  std::cerr << "\n";

  ezx_tours(P, T, rui, Us.first, rui, std::numeric_limits<int>::min(), 0, e);

  if (nmutations > 0)
     (void) ezx_pushbutton(e, NULL, NULL);
#endif

  int oldcost = P.cost(T);

  for (int i = 1; i <= nmutations; ++i) {
    T.restore_point();

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
    int newcost = P.cost(T);

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
      T.restore();
    }
  }
  errlog(-1, P.cost(T),
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

  std::sort(Us.second.begin(), Us.second.end());
  for (int i = 0; i < P.N; ++i)  assert(Us.second[i] == i);

  config S = T;
  S.sort();
  urn V(S.begin(), S.end());
  for (int i = 0; i < P.N; ++i)  assert(V[i] == i);

  P.save_tour(T, seed, nmutations);

#ifdef ezxdisp
  config dummy;
  ezx_tours(P, T, O, Us.first, dummy, std::numeric_limits<int>::min(), -1, e);

  while (3 != ezx_pushbutton(e, NULL, NULL))  { usleep(10000); }
#endif
}
#endif  // TSP_RR_GREEDY_H_
