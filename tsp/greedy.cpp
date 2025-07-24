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

void help(const char *argv0) {
  std::cout << argv0
            << " [-d] [-c] [-i tour_or_mode] [-h] [-m nmut] [-r]"\
               " [-s seed] fname\n";
  std::cout << "  -d: single display\n";
  std::cout << "  -c: small city display\n";
  std::cout << "  -i: file.tour or "\
               "radial_min/radial_max/radial_ran for RR_all()\n";
  std::cout << "  -h: this help\n";
  std::cout << "  -m: #mutations\n";
  std::cout << "  -r: rotate 270°\n";
  std::cout << "  -s: seed\n";
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  int opt;

#ifdef ezxdisp
  const char *opts = "dci:hm:rs:";
#else
  const char *opts = "i:hm:s:";
#endif

  while ((opt = getopt(argc, argv, opts)) != -1) {
    switch (opt) {
#ifdef ezxdisp
      case 'c':
        small_city = true;
        break;
      case 'd':
        single_display = true;
        break;
#endif
      case 'h':
        help(argv[0]);
        break;
      case 'i':
        src = new std::string(optarg);
        break;
      case 'm':
        nmutations = atoi(optarg);
        break;
#ifdef ezxdisp
      case 'r':
        rot270 = true;
        break;
#endif
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
