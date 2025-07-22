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

## graphics display 
A lot of options have been added, including saving and loading of tours:  
```
pi@raspberrypi5:~/RR/tsp $ ./greedy -h
./greedy [-d] [-c] [-i tour_or_mode] [-h] [-m nmut] [-r] [-s seed] fname
  -d: single display
  -c: small city display
  -i: file.tour or radial_min/radial_max/radial_ran for RR_all()
  -h: this help
  -m: #mutations
  -r: rotate 270°
  -s: seed
pi@raspberrypi5:~/RR/tsp $ 
```

This creates the usual 3 tours display, "-r" rotates by 270°, "-c" for small cities, for 13,509 cities of US:  
```
pi@raspberrypi5:~/RR/tsp $ make ezxdisp
g++ -O3 -std=c++20  -Wall -Wextra -pedantic greedy.cpp -o greedy -lstdc++ -lm -Dezxdisp -lezx -lX11
pi@raspberrypi5:~/RR/tsp $ ./greedy -r -c ../data/tsp/usa13509
19982859           global minimum
0: 22884006           RR_all() [3401927us]
2: 22875528           ran(169) (109481us)          
3: 22807828           ran(1478) (841438us)          
4: 22804898           seq(9020,375) (224667us)          
```
Looks like this:  
![res/usa13509.3disp.png](res/usa13509.3disp.png)


Same command with "-d" option for single display (click on image for 1:1 display):  
![res/usa13509.1disp.png](res/usa13509.1disp.png)


Alternatively to random ```RR_all()```, the "-i" option allows to specify one of three "radial_..." modes for initial configuration creation. Below highlights the city with minimal sum of distances to all other cities. The circle radius is avarage distance of all cities to the chosen city. ```RR_all()``` inserts the cities not randomly in this mode, but in increasing distance from the determined city. Each city gets insered with "best insert" as done with random ```RR_all()```:  
```
pi@raspberrypi5:~/RR/tsp $ ./greedy -m 0 -c -r -d -i radial_min ../data/tsp/usa13509
```
![res/usa13509.1disp.radial_min.png](res/usa13509.1disp.radial_min.png)
