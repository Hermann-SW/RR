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

#include "./loader.h"
#include "./random_access_list.h"
#include "./tsp_tour.h"
#ifdef ezxdisp
#include "./disp_utils.h"
#endif

int nmutations = 100000;
int seed = time(NULL);

template <typename config, typename urn>
void RR(std::string fname) {
  std::pair<urn, urn> Us;
  config T;
  tsp_tour<config, urn> P(fname, 0.3,  1.0/3, 1.0/3, 1.0/3);

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
  errlog(-1, P.cost(T),
         "local minimum found (after "+i2s(nmutations)+" greedy mutations)");
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


int main(int argc, char *argv[]) {
  int opt;

  while ((opt = getopt(argc, argv, "m:")) != -1) {
    switch (opt) {
      case 'm':
        nmutations = atoi(optarg);
        break;
      case 's':
        seed = atoi(optarg);
        break;
      default:
        std::cout << argv[0] << " [-m nmut] [-s seed] fname\n";
        exit(EXIT_FAILURE);
    }
  }

  if (optind >= argc) {
    std::cout << "Expected argument after options\n";
    return EXIT_SUCCESS;
  }

  srandom(seed);

  RR<random_access_list<int>, std::vector<int>>(argv[optind]);

  return EXIT_SUCCESS;
}
