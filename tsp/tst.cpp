

// https://www.geeksforgeeks.org/dsa/minimum-distance-from-a-point-to-the-line-segment-using-vectors/
//
#include <bits/stdc++.h>

// To store the point
#define Point std::pair<double, double>
#define F first
#define S second

double minDistance(const Point& A, const Point& B, const Point& E);

int X = 4;
int Y = 5;

int main() {
  Point A = std::make_pair(0, 0);
  Point B = std::make_pair(X, Y);

  int mix =  -X-Y;
  int max = 2*X+Y;
  int miy =  -Y-X;
  int may = 2*Y+X;
  int susq = ceil(sqrt(X*X + Y*Y));

  for (int y=may; y >= miy; --y) {
    for (int x=mix; x <= max; ++x) {
      Point E = std::make_pair(x, y);
      if (x == 0 && y == 0)
        std::cout << "0";
      else if (x == X && y == Y)
        std::cout << "Z";
      else if (minDistance(A, B, E) <= susq)
        std::cout << ".";
      else
        std::cout << " ";
      std::cout << " ";
    }
    std::cout << "\n";
  }

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
