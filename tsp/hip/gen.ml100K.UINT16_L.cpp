#include <iostream>
#include <stdint.h>

#include "../loader.h"

std::vector<coord_t> Coords;

int main() {
  load<coord_t>("../../data/tsp/extra/mona-lisa100K.tsp", Coords);
  std::cout << Coords.size() << " cities\n";

  uint16_t *D = new uint16_t[4999950000];
  long long int l = 0;
  size_t c = 0;
  for(int i=1; i<100000; ++i)
    for(int j=0; j<i; ++j) {
      int d = dist(Coords[i], Coords[j]);
      assert(0<d);
      assert(d<42500);
      D[c++]=d;
      l+=d;
    }
  std::cout << "sum of lower triangular 100,000×100,000 distance matrix: "
            << l << "\n";
  std::cout << c << " entries in L\n";

  FILE *out = fopen("ml100K.UINT16_L.bin","wb");
  assert(c == fwrite(D, sizeof(uint16_t), c, out));
  fclose(out);

  std::cout << "ml100K.UINT16_L.bin written\n";

  return 0;
}
