#ifndef TSP_RANDOM_ACCESS_LIST_H_
#define TSP_RANDOM_ACCESS_LIST_H_

#include <list>
#include <vector>

template <typename val>
class random_access_list {
  std::list<val> L;
  int N;

 public:
  using value_type = val;
  typedef typename std::list<val>::iterator iterator;
  std::vector<iterator> A;

  random_access_list() {
    init(0);
  }

  void init(int _N) {
    N = _N;
    L = std::list<val>(N);
    A = std::vector<iterator>(N);
    L.clear();
    for (int i = 0; i < N; ++i)  A[i] = L.end();
  }

  // Element access
  iterator& operator[](std::size_t i)  { return A[i]; }
  val& back()  { return L.back(); }

  // Iterators
  iterator begin()  { return L.begin(); }
  iterator end()  { return L.end(); }

  // Capacity
  bool empty()  { return L.empty(); }
  size_t size()  { return L.size(); }

  // Modifiers
  iterator insert(iterator it, val& v )  { return A[v] = L.insert(it, v); }
  iterator erase(iterator it)  { A[*it] = L.end(); return L.erase(it); }
  iterator erase(int i)  {
    iterator it = A[i]; A[i] = L.end(); return L.erase(it);
  }
  void push_back(val& v )  { L.push_back(v); A[v] = --L.end(); }

  // Operations
  void sort()  { L.sort(); }
};

#endif  // TSP_RANDOM_ACCESS_LIST_H_
