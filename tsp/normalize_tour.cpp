#include <iostream>

#include "./loader.h"

typedef int city_t;

std::vector<city_t> opt;
typedef std::vector<city_t>::iterator iterator;

int main(int argc, char *argv[]) {
  assert(argc >= 2 && argc <=3);
  std::string fname(argv[1]);

  load<city_t>(fname, opt);
  // std::cout << opt_length << "\n";

  iterator it;
  for (it = opt.begin(); *it != 1; ++it) {}
  do {
    std::cout << *it <<"\n";
    if (argc == 3) {
      it += 1;
      if (it == opt.end())  it = opt.begin();
    } else {
      if (it == opt.begin())  it = opt.end();
      it -= 1;
    }
  } while (*it != 1);

  return 0;
}


