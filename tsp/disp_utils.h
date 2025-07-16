#ifndef TSP_DISP_UTILS_H_
#define TSP_DISP_UTILS_H_

#include <unistd.h>
#include <ezxdisp.h>

#include <limits>
#include <utility>

const int mar = 8;
const int wid = 3000;
const int hei = 3800;
const int Div = 5;

void mp(int x, int y, int s, std::pair<int, int>& a) {
  a = std::pair<int, int>(mar+x/Div+s*(2*mar+wid/Div), mar+(hei-y)/Div);
}

void city(const std::pair<int, int>& c, ezx_t *e) {
  ezx_fillrect_2d(e, c.first-2, c.second-2, c.first+2, c.second+2, &ezx_black);
}

void city2(const std::pair<int, int>& c, ezx_t *e) {
  ezx_fillrect_2d(e, c.first-4, c.second-4, c.first+4, c.second+4, &ezx_orange);
}

void city2a(const std::pair<int, int>& c, ezx_t *e) {
  ezx_fillrect_2d(e, c.first-6, c.second-6, c.first+6, c.second+6, &ezx_orange);
}

template <typename config, typename urn>
void ezx_tours(tsp_tour<config, urn>& P, config& T, config& R, urn& U,
               config& N, int ret, int mut, ezx_t*& e) {
  bool initial = (ret == std::numeric_limits<int>::min());
  ezx_wipe(e);

  ezx_line_2d(e, wid/Div+2*mar, hei/Div+2*mar, wid/Div+2*mar, 0, &ezx_black, 1);
  ezx_line_2d(e, 2*(wid/Div+2*mar), hei/Div+2*mar, 2*(wid/Div+2*mar), 0,
              &ezx_black, 1);

  ezx_str_2d(e, 5, 10, const_cast<char *>(reinterpret_cast<const char*>
                         (initial ? (mut == 0 ? "RR_all" : "minimum found")
                                  : "previous")), &ezx_black);
  std::stringstream s2;
  s2 << P.cost(T);
  ezx_str_2d(e, 50, hei/Div+mar,
             const_cast<char *>(reinterpret_cast<const char*>
              (s2.str().c_str())), &ezx_black);
  if (ret != std::numeric_limits<int>::min()) {
    s2 = std::stringstream();
    s2 << mut << ": ";
    if (ret == std::numeric_limits<int>::max())
      s2 << "ran";
    else
      s2 << (ret >= 0 ? "rad" : "seq");
    s2 << "(" << U.size() << ") ";
    s2 << P.cost(R);
    ezx_str_2d(e, 50+wid/Div+2*mar, hei/Div+mar,
               const_cast<char*>(reinterpret_cast<const char *>
                 (s2.str().c_str())), &ezx_black);
    s2 = std::stringstream();
    s2 << P.cost(N) << " (" << P.cost(N) - P.cost(T) << ")";
    ezx_str_2d(e, 50+2*(wid/Div+2*mar), hei/Div+mar,
               const_cast<char*>(reinterpret_cast<const char *>
                 (s2.str().c_str())), &ezx_black);

    ezx_str_2d(e, 5+wid/Div+2*mar, 10,
               const_cast<char*>(reinterpret_cast<const char*>("ruined")),
               &ezx_black);
    ezx_str_2d(e, 5+2*(wid/Div+2*mar), 10,
               const_cast<char*>(reinterpret_cast<const char*>("recreated")),
               &ezx_black);
  } else if (mut != 0) {
    ezx_str_2d(e, 5+wid/Div+2*mar, 10,
               const_cast<char*>(reinterpret_cast<const char*>
                 ("global minimum")), &ezx_black);

    s2 = std::stringstream();
    s2 << P.cost(R);
    ezx_str_2d(e, 50+wid/Div+2*mar, hei/Div+mar,
               const_cast<char*>(reinterpret_cast<const char *>
                 (s2.str().c_str())), &ezx_black);
  }

  if (ret != std::numeric_limits<int>::max() && ret >= 0) {
    std::pair<int, int> c;
    mp(P.C[ret].first, P.C[ret].second, 0, c);
    int r = P.D[ret][P.rad_nxt[ret][U.size()-1]];
    ezx_circle_2d(e, c.first, c.second, r/Div, &ezx_orange, 2);
    ezx_circle_2d(e, c.first+2*(wid/Div+2*mar), c.second, r/Div,
                  &ezx_orange, 2);
  }

  int prev = T.back();
  std::pair<int, int> p;
  mp(P.C[prev].first, P.C[prev].second, 0, p);

  std::for_each(T.begin(), T.end(), [&p, &e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i].first, P.C[i].second, 0, c);
    ezx_line_2d(e, p.first, p.second, c.first, c.second, &ezx_blue, 1);
    p = c;
  });

  int hig = (ret < 0) ? -(1 + ret)
                      : (ret != std::numeric_limits<int>::max()) ? ret : -1;
  std::for_each(U.begin(), U.end(), [hig, &e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i].first, P.C[i].second, 1, c);
    if (i == hig) { city2a(c, e); } else { city2(c, e); }
  });

  prev = R.back();
  mp(P.C[prev].first, P.C[prev].second, 1,  p);

  std::for_each(R.begin(), R.end(), [&p, &e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i].first, P.C[i].second, 1, c);
    ezx_line_2d(e, p.first, p.second, c.first, c.second, &ezx_blue, 1);
    p = c;
  });

  prev = N.back();
  mp(P.C[prev].first, P.C[prev].second, 2,  p);

  std::for_each(N.begin(), N.end(), [&p, &e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i].first, P.C[i].second, 2, c);
    ezx_line_2d(e, p.first, p.second, c.first, c.second, &ezx_blue, 1);
    p = c;
  });

  std::for_each(T.begin(), T.end(), [ret, &e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i].first, P.C[i].second, 0, c);
    city(c, e);
    if (ret != std::numeric_limits<int>::min()) {
      mp(P.C[i].first, P.C[i].second, 2, c);
      city(c, e);
    }
  });

  std::for_each(R.begin(), R.end(), [&e, &P](int i) {
    std::pair<int, int> c;
    mp(P.C[i].first, P.C[i].second, 1, c);
    city(c, e);
  });

  // ezx_window_name(e, "57123 -> 53207 -> 51219");
  ezx_redraw(e);
  usleep(10000);
}

#endif  // TSP_DISP_UTILS_H_
