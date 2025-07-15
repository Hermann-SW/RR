# TSP

## load_test demo
```
hermann@j4105:~/RR/tsp$ make load_test 
g++ -O3 -std=c++20  -Wall -Wextra -pedantic load_test.cpp -o load_test -lstdc++ -lm
hermann@j4105:~/RR/tsp$ 
hermann@j4105:~/RR/tsp$ ./load_test ../data/tsp/usa13509
2
490000,1.22264e+06
13509
-1
7930
19982885
hermann@j4105:~/RR/tsp$ 
```

## greedy

```
hermann@j4105:~/RR/tsp$ make greedy
g++ -O3 -std=c++20  -Wall -Wextra -pedantic greedy.cpp -o greedy -lstdc++ -lm
hermann@j4105:~/RR/tsp$ 
hermann@j4105:~/RR/tsp$ time ./greedy 205
50933           local minimum found (after 100,000 greedy mutations)
21896           ms (only recreate)
50778           global minimum

real	0m26,143s
user	0m26,086s
sys	0m0,015s
hermann@j4105:~/RR/tsp$ 
```
