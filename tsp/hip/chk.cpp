#include <iostream>
#include <cmath>
#include <format>

double f(double val, int loops) {
      for(int i=0; i<loops; ++i) {
        val = sqrt(val);
        val = sqrt(val + 1.0);
        val = sqrt(val + 2.0);
        val = sqrt(val + 3.0);
        val = sqrt(val + 4.0);
        val = sqrt(val + 5.0);
        val = sqrt(val + 6.0);
        val = sqrt(val + 7.0);
        val = sqrt(val + 8.0);
        val = sqrt(val + 9.0);
      }
    return val;
}

int main(int argc, const char *argv[]) {
    const int N = 400'000'000;
    int loops = (argc == 2) ? 1 : atoi(argv[2]);

    std::cout << std::format("{}", f(static_cast<double>(N-1) + 0.5, loops)) << std::endl;

    return 0;
}
