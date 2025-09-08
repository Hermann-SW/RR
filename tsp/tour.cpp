#include <iostream>
#include <algorithm>
#include <vector>
#include <cassert>

typedef typename std::vector<int>::iterator iterator;

int main() {
  int c, n;
  std::vector<int> v;

  do {
    std::cin >> c >> n;
    if (std::cin.eof())  break;
    iterator it = (n == -1) ? v.end() : std::find(v.begin(), v.end(), n);
    assert(n == -1 || it != v.end());
    v.insert(it, c);
  } while (std::cin);

  std::cout << "NAME : part.tour\n";
  std::cout << "COMMENT : partial tour (Length 0)\n";
  std::cout << "TYPE : TOUR\n";
  std::cout << "DIMENSION : " << v.size() << "\n";
  std::cout << "TOUR_SECTION\n";

  std::for_each(v.begin(), v.end(), [](int c) { std::cout << c << "\n"; });

  std::cout << "-1\nEOF\n";
}
