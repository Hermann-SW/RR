#include<iostream>
#include<cassert>
#include<vector>
#include<algorithm>

void numa(std::vector<int>& v) {
  v.clear();
  FILE *src = ::popen("lscpu -p", "r");
  char line[1000];
  int cpu, core, socket, node, i = 0;
  while (fgets(line, 1000, src)) {
    if (line[0] != '#') {
      sscanf(line, "%d,%d,%d,%d", &cpu, &core, &socket, &node);
      assert(cpu == i++); v.push_back(node);
    }
  }
}

int main() {
  std::vector<int> nodes;
  numa(nodes);
  std::for_each(nodes.begin(), nodes.end(),
                [](int i) {std::cout << i << "\n";});
  return 0;
}
