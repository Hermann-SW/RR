#ifndef TSP_LOADER_H_
#define TSP_LOADER_H_
#include <math.h>
#include <fstream>
#include <cassert>
#include <vector>
#include <utility>
#include <string>
#include <type_traits>

template<typename T> struct is_pair_t                  : std::false_type {};
template<typename T> struct is_pair_t<std::pair<T, T>> : std::true_type {};

enum edge_weight_t { ATT, CEIL_2D, EUC_2D, EXPLICIT, GEO, UNDEF };

typedef std::pair<double, double> coord_t;


edge_weight_t edge_weight_type = UNDEF;
int opt_length = -1;


inline int nint(double d) { return static_cast<int>(0.5 + d); }
// http://comopt.ifi.uni-heidelberg.de/software/TSPLIB95/tsp95.pdf#page=6
int euc_2d(const coord_t& from, const coord_t& to) {
  double xd = from.first - to.first;
  double yd = from.second - to.second;
  int d = nint(sqrt(xd*xd + yd*yd));
#ifdef MEMOPT
  assert(d <= 32767);
#endif
  return nint(sqrt(xd*xd + yd*yd));
}
int ceil_2d(const coord_t& from, const coord_t& to) {
  double xd = from.first - to.first;
  double yd = from.second - to.second;
  double d = ceil(sqrt(xd*xd + yd*yd));
#ifdef MEMOPT
  assert(d <= 32767);
#endif
  return static_cast<int>(ceil(sqrt(xd*xd + yd*yd)));
}
int att(const coord_t& from, const coord_t& to) {
  double xd = from.first - to.first;
  double yd = from.second - to.second;
  double rij = sqrt((xd*xd + yd*yd) / 10.0);
  double tij = nint(rij);
  double d = tij < rij ? tij + 1 : tij;
#ifdef MEMOPT
  assert(abs(d) <= 32767);
#endif
  return static_cast<int>(d);
}
double deg2rad(double xy) {
  double PI = 3.141592;
  int deg = static_cast<int>(xy);  // chapter 2.4: nint(xy) — in tsplib95 int()
  double min = xy - deg;
  return PI * (deg + 5.0 * min / 3.0 ) / 180.0;
}
int geo(const coord_t& from, const coord_t& to) {
  double laf = deg2rad(from.first);
  double lat = deg2rad(to.first);
  double lof = deg2rad(from.second);
  double lot = deg2rad(to.second);
#ifdef MEMOPT
// maximal distance is 20,000 — no assert neccessary
#endif
  double RRR = 6378.388;
  double q1 = cos(lof - lot);
  double q2 = cos(laf - lat);
  double q3 = cos(laf + lat);
  return static_cast<int>(RRR * acos(0.5*((1.0+q1)*q2 - (1.0-q1)*q3)) + 1.0);
}
int dist(const coord_t& from, const coord_t& to) {
  switch (edge_weight_type) {
    case CEIL_2D: return ceil_2d(from, to);
    case EUC_2D: return euc_2d(from, to);
    case ATT: return att(from, to);
    case GEO: return geo(from, to);
    default: assert(!"edge_weight_type not implemented");
  }
}

std::ifstream& operator >> (std::ifstream& in, coord_t &c1) {
  in >> c1.first >> c1.second; return in;
}

std::string value(std::ifstream& src, const std::string& pre) {
  std::string line;
  do { getline(src, line); assert(!src.eof()); } while (!line.starts_with(pre));

  if (line.find(":") == line.npos)  return "";

  std::string rgt = line.substr(line.find(":") + 1);

  if (rgt.find_first_not_of(" \t") != line.npos)
    rgt = rgt.substr(rgt.find_first_not_of(" \t"));

  if (rgt.find_last_not_of(" \t") != line.npos)
    rgt.resize(rgt.find_last_not_of(" \t") + 1);

  return rgt;
}

template <typename vt>
void load(std::string fname, std::vector<vt>& pd) {
  bool ipt = is_pair_t<vt>::value;

  std::ifstream tsp(fname); assert(tsp.is_open());

  if (!ipt) {
    std::string cmt = value(tsp, "COMMENT");
    assert(cmt.find("Length") != cmt.npos);
    cmt = cmt.substr(cmt.find("Length") + 6);
    assert(cmt.find_first_of("0123456789") != cmt.npos);
    cmt = cmt.substr(cmt.find_first_of("0123456789"));
    opt_length = stoi(cmt);
  }

  assert(value(tsp, "TYPE") == (ipt ? "TSP" : "TOUR"));

  int dim = stoi(value(tsp, "DIMENSION"));

  std::string etype;
  if (ipt) { etype = value(tsp, "EDGE_WEIGHT_TYPE"); }

  assert(value(tsp, ipt ? "NODE_COORD_SECTION" : "TOUR_SECTION") == "");

  pd = std::vector<vt>(dim);
  for (int i = 1; i <= dim; ++i) {
    if (ipt) { int id; tsp >> id; assert(i == id); }
    tsp >> pd[i-1];
  }

  if (ipt) {
    edge_weight_type = (etype == "ATT" ? ATT
                        : (etype == "CEIL_2D" ? CEIL_2D
                           : (etype == "EUC_2D" ? EUC_2D
                              // : (etype == "EXPLICIT" ? EXPLICIT
                                    : (assert(etype == "GEO"), GEO))));
  }
}
#endif  // TSP_LOADER_H_
