#include<fstream>
#include<cassert>

int main(int argc, char *argv[]) {
  assert(argc > 1 && argc <4);
  int N = atoi(argv[1]);
  int w = argc > 2 ? atoi(argv[2]) : 10;
  assert(N%2 == 0 && w >= 1);

  std::ofstream os("sq.tsp");
  os << "NAME : sq.tsp\n";
  os << "COMMENT: spquare TSP\n";
  os << "TYPE : TSP\n";
  os << "DIMENSION : " << N*N << "\n";
  os << "EDGE_WEIGHT_TYPE : EUC_2D\n";
  os << "NODE_COORD_SECTION\n";
  for (int y = 0; y < N; ++y)
    for (int x=0; x < N; ++x)
      os << y*N+x+1 << " " << y*w << " " << x*w << "\n";
  os << "EOF\n";

  std::ofstream oss("sq.opt.tour");
  oss << "NAME : sq.opt.tour\n";
  oss << "COMMENT : simple polygon tour (Length " << N*N*w << ")\n";
  oss << "TYPE : TOUR\n";
  oss << "DIMENSION : " << N*N << "\n";
  oss << "TOUR_SECTION\n";
  for (int x=0; x < N; ++x)
    oss << x+1 << "\n";
  for (int y=1; y < N-1; ++y) {
    if (y%2) {
      for (int x=N-1; x > 0; --x)
        oss << y*N+x+1 << "\n";
    } else {
      for (int x=1; x < N; ++x)
        oss << y*N+x+1 << "\n";
    }
  }
  for (int x=N-1; x >= 0; --x)
    oss << (N-1)*N+x+1 << "\n";
  for (int y=N-2; y > 0; --y)
    oss << y*N+1 << "\n";
  oss << "-1\n";
  oss << "EOF\n";

  return 0;
}
