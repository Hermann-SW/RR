#ifndef TSP_RR_GREEDY_H_
#define TSP_RR_GREEDY_H_

#include <string>
#include <utility>
#include <limits>
// #include <sstream>
// #include <iostream>

// #include "./loader.h"
// #include "./random_access_list.h"
// #include "./tsp_tour.h"
#ifdef ezxdisp
#include "./disp_utils.h"
#endif

int nmutations = 100000;
bool rot270 = false;

template <typename config, typename urn>
void RR_greedy(std::string fname, int seed) {
  std::pair<urn, urn> Us;
  config T;
  tsp_tour<config, urn> P(fname, 0.3,  1.0/3, 1.0/3, 1.0/3);

#ifdef ezxdisp
  ezx_t *e = ezx_init(3*(wid/Div+2*marx), hei/Div+2*mary,
                      const_cast<char*>(reinterpret_cast<const char*>
                      ("TSP greedy Ruin and Recreate")));

  if (rot270) {
    for (int i = 0; i < static_cast<int>(P.C.size()); ++i) {
      double f = P.C[i].first;
      double s = P.C[i].second;
      P.C[i].first = wid-s;
      P.C[i].second = f;
    }
  }
#endif

  P.RR_all(T, Us);

  errlog(0, P.cost(T), "RR_all() [" + i2s(_sum) + "us]");
  _sum = 0;

#ifdef ezxdisp
  config RC;
  urn UC;
  bool confirm = true;

  std::cerr << "\n";

  ezx_tours(P, T, RC, Us.first, RC, std::numeric_limits<int>::min(), 0, e);

  (void) ezx_pushbutton(e, NULL, NULL);
#endif

  for (int i = 1; i <= nmutations; ++i) {
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
      int b;

      std::cerr << "\n";
      ezx_tours(P, T, RC, UC, R, ret, i, e);

      if (confirm) {
        while (0 == (b = ezx_pushbutton(e, NULL, NULL)))  { usleep(10000); }

        confirm = (b != 3);
      } else {
        confirm = (0 != (ezx_sensebutton(e, NULL, NULL) & EZX_BUTTON_LMASK));
      }
#endif

      T = R;
      UsR.second.clear();
      for (typename config::iterator it = T.begin(); it != T.end(); ++it) {
        T[*it] = it;
        UsR.second.push_back(*it);
      }
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
#endif  // TSP_RR_GREEDY_H_
