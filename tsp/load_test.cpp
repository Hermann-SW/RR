#include <iostream>

#include "./loader.h"

typedef int city_t;

std::vector<coord_t> Coords;
std::vector<city_t> opt;

int main(int argc, char *argv[]) {
  assert(argc == 2);
  std::string fname(argv[1]);

  load<coord_t>(fname + ".tsp", Coords);
  std::cout << edge_weight_type << "\n";

  std::cout << Coords.back().first << "," << Coords.back().second << "\n";
  std::cout << Coords.size() << "\n";
  std::cout << opt_length << "\n";

  load<city_t>(fname + ".opt.tour", opt);
  std::cout << opt.back() << "\n";
  std::cout << opt_length << "\n";

  return 0;
}


