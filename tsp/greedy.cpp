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
#include "./RR_greedy.h"

int seed = time(NULL);

int main(int argc, char *argv[]) {
  int opt;

  while ((opt = getopt(argc, argv, "m:rs:")) != -1) {
    switch (opt) {
      case 'm':
        nmutations = atoi(optarg);
        break;
      case 'r':
        rot270 = true;
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

  RR_greedy<random_access_list<int>, std::vector<int>>(argv[optind], seed);

  return EXIT_SUCCESS;
}
