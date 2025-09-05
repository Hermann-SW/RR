

// https://www.geeksforgeeks.org/dsa/minimum-distance-from-a-point-to-the-line-segment-using-vectors/
//
#include <bits/stdc++.h>

// To store the point
#define Point std::pair<double, double>
#define F first
#define S second

double minDistance(const Point& A, const Point& B, const Point& E);

// https://www.geeksforgeeks.org/dsa/check-if-two-given-line-segments-intersect/
//
bool doIntersect(std::vector<std::vector<std::vector<int>>>& points);


int X = 2*22; //4;
int Y = 2*18; //5;
int D = 28; // mona-lisa100K.tsp minimal city distance

double dist(const Point& A, const Point& B) {
  return sqrt((B.F - A.F)*(B.F - A.F) + (B.S - A.S)*(B.S - A.S));
}

typedef std::pair<double, double> coord_t;

inline int nint(double d) { return static_cast<int>(0.5 + d); }
// http://comopt.ifi.uni-heidelberg.de/software/TSPLIB95/tsp95.pdf#page=6
int euc_2d(const Point& from, const Point& to) {
  double xd = from.F - to.F;
  double yd = from.S - to.S;
  int d = nint(sqrt(xd*xd + yd*yd));
  return d;
}

int mind(const Point& A, const Point& B, const Point& E, const Point& F) {
  int mi = euc_2d(A, B);
  if (euc_2d(A, E) < mi)  mi = euc_2d(A, E);
  if (euc_2d(A, F) < mi)  mi = euc_2d(A, F);
  if (euc_2d(B, E) < mi)  mi = euc_2d(B, E);
  if (euc_2d(B, F) < mi)  mi = euc_2d(B, F);
  if (euc_2d(E, F) < mi)  mi = euc_2d(E, F);
  return mi;
}

int side(const Point& A, const Point& B, const Point& E) {
  std::pair<double, double> AB;
  AB.F = B.F - A.F;
  AB.S = B.S - A.S;

  std::pair<double, double> AE;
  AE.F = E.F - A.F,
  AE.S = E.S - A.S;

  double x1 = AB.F;
  double y1 = AB.S;
  double x2 = AE.F;
  double y2 = AE.S;

  return x1 * y2 - y1 * x2;
}

int main() {
  Point A = std::make_pair(0, 0);
  Point B = std::make_pair(X, Y);

  int mix =  -X-Y;
  int max = 2*X+Y;
  int miy =  -Y-X;
  int may = 2*Y+X;
  int susq = ceil(sqrt(X*X + Y*Y));

  std::vector<Point> t, b;

  for (int y=may; y >= miy; --y) {
    for (int x=mix; x <= max; ++x) {
      Point E = std::make_pair(x, y);
      if (minDistance(A, B, E) <= susq) {
	int sid = side(A, B, E);
        if (sid < 0) b.push_back(E);
	else if (sid > 0) t.push_back(E);
      }
    }
  }

  std::for_each(t.begin(), t.end(), [b, susq, A, B, t](const Point& p) {
    std::for_each(b.begin(), b.end(), [p, susq, A, B, t](const Point& q) {
      if (dist(p, q) <= susq) {
        std::vector<std::vector<std::vector<int>>> points = 
          {{{A.F, A.S}, {B.F, B.S}}, {{p.F, p.S}, {q.F, q.S}}};
        if (doIntersect(points)) {
	  if ((euc_2d(A, B) + euc_2d(B, p) + euc_2d(p, q) + euc_2d(q, A) <
               euc_2d(A, p) + euc_2d(p, B) + euc_2d(B, q) + euc_2d(q, A)) &&
	      mind(A, B, p, q) >= D) {
	    std::cout << p.F << "," << p.S << " " << q.F << "," << q.S
                      << "  (" << mind(A, B, p, q) << ")\n";
	  } else
	  if ((euc_2d(A, B) + euc_2d(B, q) + euc_2d(q, p) + euc_2d(p, A) <
               euc_2d(A, p) + euc_2d(p, B) + euc_2d(B, q) + euc_2d(q, A)) &&
	      mind(A, B, p, q) >= D) {
	    std::cout << p.F << "," << p.S << " " << q.F << "," << q.S
                      << "  (" << mind(A, B, p, q) << ")\n";
	  }
	}
      }
    });
  });

  return 0;
}


// Function to return the minimum distance
// between a line segment AB and a point E
double minDistance(const Point& A, const Point& B, const Point& E) {
    // vector AB
    std::pair<double, double> AB;
    AB.F = B.F - A.F;
    AB.S = B.S - A.S;

    // vector BP
    std::pair<double, double> BE;
    BE.F = E.F - B.F;
    BE.S = E.S - B.S;

    // vector AP
    std::pair<double, double> AE;
    AE.F = E.F - A.F,
    AE.S = E.S - A.S;

    // Variables to store dot product
    double AB_BE, AB_AE;

    // Calculating the dot product
    AB_BE = (AB.F * BE.F + AB.S * BE.S);
    AB_AE = (AB.F * AE.F + AB.S * AE.S);

    // Minimum distance from
    // point E to the line segment
    double reqAns = 0;

    // Case 1
    if (AB_BE > 0) {
        // Finding the magnitude
        double y = E.S - B.S;
        double x = E.F - B.F;
        reqAns = sqrt(x * x + y * y);

    // Case 2
    } else if (AB_AE < 0) {
        double y = E.S - A.S;
        double x = E.F - A.F;
        reqAns = sqrt(x * x + y * y);

    // Case 3
    } else {
        // Finding the perpendicular distance
        double x1 = AB.F;
        double y1 = AB.S;
        double x2 = AE.F;
        double y2 = AE.S;
        double mod = sqrt(x1 * x1 + y1 * y1);
        reqAns = abs(x1 * y2 - y1 * x2) / mod;
    }
    return reqAns;
}


// function to check if point q lies on line segment 'pr'
bool onSegment(std::vector<int>& p, std::vector<int>& q, std::vector<int>& r) {
    return (q[0] <= std::max(p[0], r[0]) && 
            q[0] >= std::min(p[0], r[0]) &&
            q[1] <= std::max(p[1], r[1]) && 
            q[1] >= std::min(p[1], r[1]));
}

// function to find orientation of ordered triplet (p, q, r)
// 0 --> p, q and r are collinear
// 1 --> Clockwise
// 2 --> Counterclockwise
int orientation(std::vector<int>& p, std::vector<int>& q, std::vector<int>& r) {
    int val = (q[1] - p[1]) * (r[0] - q[0]) -
              (q[0] - p[0]) * (r[1] - q[1]);

    // collinear
    if (val == 0) return 0;

    // clock or counterclock wise
    // 1 for clockwise, 2 for counterclockwise
    return (val > 0) ? 1 : 2;
}


// function to check if two line segments intersect
bool doIntersect(std::vector<std::vector<std::vector<int>>>& points) {

    // find the four orientations needed
    // for general and special cases
    int o1 = orientation(points[0][0], points[0][1], points[1][0]);
    int o2 = orientation(points[0][0], points[0][1], points[1][1]);
    int o3 = orientation(points[1][0], points[1][1], points[0][0]);
    int o4 = orientation(points[1][0], points[1][1], points[0][1]);

    // general case
    if (o1 != o2 && o3 != o4)
        return true;

    // special cases
    // p1, q1 and p2 are collinear and p2 lies on segment p1q1
    if (o1 == 0 && 
    onSegment(points[0][0], points[1][0], points[0][1])) return true;

    // p1, q1 and q2 are collinear and q2 lies on segment p1q1
    if (o2 == 0 && 
    onSegment(points[0][0], points[1][1], points[0][1])) return true;

    // p2, q2 and p1 are collinear and p1 lies on segment p2q2
    if (o3 == 0 && 
    onSegment(points[1][0], points[0][0], points[1][1])) return true;

    // p2, q2 and q1 are collinear and q1 lies on segment p2q2 
    if (o4 == 0 && 
    onSegment(points[1][0], points[0][1], points[1][1])) return true;

    return false;
}
