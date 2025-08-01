# Ruin and Recreate

This repo is followup on [pcb442.cpp gist](https://gist.github.com/Hermann-SW/1218d13dc7fb95aa90687ad8baa06787).  

Initially TSP implementation of Ruin&Recreate from [Record Breaking Optimization Results Using the Ruin and Recreate Principle](https://www.semanticscholar.org/paper/Record-Breaking-Optimization-Results-Using-the-Ruin-Schrimpf-Schneider/4f80e70e51e368858c3df0787f05c3aa2b9650b4) 2000 Journal of Computational Physics paper.

Sofar 111 TSP sample problems have been added, and length of most (92) of 108 optimal tours have been verified. See README.md documentation here:  
[data/tsp/](data/tsp/)  

Now code from pcb442.cpp gist is split and works. The new top level file is [tsp/greedy.cpp](tsp/greedy.cpp). And it already allows to load other than pcb442 TSP problems. I did 500 mutations for 13,509 cities problem usa13509 again with new code. Complete (5.6GHz!) output [here](tsp/greedy.usa13509.md). As discussed in [this posting](https://gist.github.com/Hermann-SW/1218d13dc7fb95aa90687ad8baa06787?permalink_comment_id=5673617#gistcomment-5673617) maximal time for the recreate step was 437ms (for Ruin and Recreate of 29.7% cities). Below output shows the time for the initial ```RR_all()``` as well, 872ms. After a bit more cleanup work, the recreate step will be parallelized onto 3,840 cores of one of my 9 Vega20 GPUs. Each core will have to do ```ceil(13509/3840)=4``` best insert computatations for "its" 4 cities (which can be done now easily because of use of newly used ```random_access_list``` datatype). Then in 12 more steps the minimum of the 3,840 minimal values can be determined and the CPU can take that and continue the Recreate loop.  

Here top and bottom:
```
hermann@7950x:~/RR/tsp$ rm nohup.out 
hermann@7950x:~/RR/tsp$ nohup perf stat -e cycles,task-clock ./greedy 205 ../data/tsp/usa13509
nohup: ignoring input and appending output to 'nohup.out'
hermann@7950x:~/RR/tsp$ sed "s/^M/\n/g" nohup.out 
0: 22831078           RR_all() [872447us]
2: 22820588           ran(169) (18955us)          
3: 22732564           ran(1478) (158392us)          
7: 22702848           seq(7932,3777) (378945us)          
8: 22573125           ran(2254) (251673us)          
...
382: 21694504           ran(1258) (169839us)          
403: 21694366           rad(10721,280) (38561us)          
426: 21691992           seq(1341,43) (6034us)          
431: 21691193           ran(613) (84939us)          
443: 21691026           ran(1539) (205806us)          
453: 21686442           ran(640) (88553us)          
458: 21686259           ran(291) (40888us)          
475: 21679707           ran(1016) (138679us)          
483: 21676735           ran(1750) (233416us)          
486: 21676089           ran(1421) (190850us)          
490: 21675779           ran(74) (10566us)          
21675779           local minimum found (after 100,000 greedy mutations)

124341           ms (only recreate)

19982859           global minimum


 Performance counter stats for './greedy 205 ../data/tsp/usa13509':

   742,225,108,027      cycles                           #    5.599 GHz                       
        132,569.21 msec task-clock                       #    1.000 CPUs utilized             

     132.578677925 seconds time elapsed

     132.077723000 seconds user
       0.490965000 seconds sys

hermann@7950x:~/RR/tsp$ 
```

## usa13509.tsp

A lot new options, including [graphics display](https://github.com/Hermann-SW/RR/tree/main/tsp#graphics-display) (here single display of usa13509.tsp):  
![https://github.com/Hermann-SW/RR/blob/main/tsp/res/usa13509.1disp.png](https://github.com/Hermann-SW/RR/blob/main/tsp/res/usa13509.1disp.png)

## Mona Lisa TSP Challenge

Nice, first time view of mona-lisa100K.tsp problem with 100,000 cities (and [1,000 USD price money](https://www.math.uwaterloo.ca/tsp/data/ml/monalisa.html)) with Ruin and Recreate greedy solver. Initial RR_all() took 301 seconds. Soon I will parallelize the recreate step onto an AMD Vega20 type GPU with 3,840 cores — I can't wait to see the runtime reduction possible (ceil(100,000/3,840)=27 steps for a core to compute best fit cost of new city at "its" cities, then 12 steps to compute overall minimum of the 3,840 minimal costs). Not my fastest machine, but the only that could deal with the currently 58.1g resident RAM for greedy process (already reduced from 74.9g by storing the entries in distance matrix as ```int16_t``` instead of 32bit ```int``` — maximal mona-lisa100K.tsp distance is less than 30,000).
```
hermann@E5-2680v4:~/RR/tsp$ time ./greedy -d -c ../data/tsp/extra/mona-lisa100K
5757191             best known tour, lower bound 5757084
0: 6184100           RR_all() [300858747us]
1: 6184089           ran(1) (8478us)          
2: 6180983           ran(1249) (7177095us)          
3: 6164674           ran(10939) (61598114us)          
8: 6145662           ran(16683) (96589032us)          
9: 6129113           ran(23012) (131223728us)          
10: 6120088           ran(7571) (48347901us)          
13: 6110181           ran(24313) (142942000us)    
```

![tsp/res/mona-lisa100K.part.png](tsp/res/mona-lisa100K.part.png)
