
## optimal TSP tour length benchmark

[opt_tour_length_benchmark.cpp](opt_tour_length_benchmark)
```
hermann@Radeon-pro-vii:~/RR/tsp/hip$ make opt_tour_length_benchmark 
hipcc -O3 -std=c++20 --offload-arch=gfx906 opt_tour_length_benchmark.cpp -o opt_tour_length_benchmark
hermann@Radeon-pro-vii:~/RR/tsp/hip$ ./opt_tour_length_benchmark ../../data/tsp/extra/mona-lisa100K
=== Optimal TSP Tour Length Benchmark ===
Total Tour Length Computations: 10000000
Total Cities per Tour         : 100000
Total Distance Calculations   : 1000000000000

Launching Kernel across 10000000 threads...

=================== RESULTS ===================
Single Tour Length           : 5757191
Total Sum (10000000 Computations): 57571910000000
-----------------------------------------------
Total GPU Kernel Runtime     : 7990.98 ms (7.99098 s)
Throughput                   : 125.14 Gsqrt/s
hermann@Radeon-pro-vii:~/RR/tsp/hip$ ./opt_tour_length_benchmark ../../data/tsp/usa13509
=== Optimal TSP Tour Length Benchmark ===
Total Tour Length Computations: 10000000
Total Cities per Tour         : 13509
Total Distance Calculations   : 135090000000

Launching Kernel across 10000000 threads...

=================== RESULTS ===================
Single Tour Length           : 19982859
Total Sum (10000000 Computations): 199828590000000
-----------------------------------------------
Total GPU Kernel Runtime     : 1081.3 ms (1.0813 s)
Throughput                   : 124.93 Gsqrt/s
hermann@Radeon-pro-vii:~/RR/tsp/hip$ 
```

## synthetic benchmark

This is synthetic benchmark to measure peak double sqrt performance.

```
f=benchmark_sqrt
hipcc -O3 --offload-arch=gfx906 $f.cpp -o $f  # AMD Instinct MI50
hipcc -O3 -arch=sm_60 -x cu $f.cpp -o $f      # NVIDIA Tesla P100
```

Similar [AVX512 synthetic benchmark](https://gist.github.com/Hermann-SW/c4e40e823d274d03094d5e6d5071017d?permalink_comment_id=6237349#gistcomment-6237349):
| CPU       | #core | [double Gsqrt/s] | |TSP tourlength|
|----------:|------:|--------:|-|-:|
| AMD 7950X | 16    | 43.5 | |39.7|
| AMD 9950X | 16    | 90.7 | |83.7|

[benchmark_sqrt.cpp](./benchmark_sqrt.cpp) reports for
| GPU                    | #CU/#SM | [double Gsqrt/s] | 200 loops |
|-----------------------:|--------:|--------:|-------------------:|
| NVIDIA Tesla P100 PCIE | 56      | 193.7 |             205.2    |
| AMD Radeon VII         | 60      | 363.5 |             557.6    |
| AMD Radeon Pro VII     | 60      | 417.3 |            1052.4    |
| AMD Instinct MI50s     | 60      | 426.9-436.7 | 1053.2-1064.6  |

```
hermann@W-2225:~$ ./benchmark_sqrt 0
Device ID 0 (Tesla P100-PCIE-16GB) UUID: GPU-8225733aafcc75d8e31bec550b39eb2e
Number of CUs/SMs: 56
Allocating 6103 MB of VRAM...
Launching Kernel across 1785715 thread blocks...
--------------------------------------------------------
Execution Completed Successfully.
Execution Time: 0.0206519 seconds
Total Sqrt Operations: 4e+09
Verification Check (Last Element): 3.51286
193.686 double Gsqrt/s 
hermann@W-2225:~$ 
```

```
hermann@Radeon-vii:~$ ./benchmark_sqrt 0
Device ID 0 (AMD Radeon VII) UUID: GPU-3f52314172fc1a63
Number of CUs/SMs: 60
Allocating 6103 MB of VRAM...
Launching Kernel across 1666667 thread blocks...
--------------------------------------------------------
Execution Completed Successfully.
Execution Time: 0.0110056 seconds
Total Sqrt Operations: 4e+09
Verification Check (Last Element): 3.51286
363.451 double Gsqrt/s 
hermann@Radeon-vii:~$ 
```

```
hermann@7600x:~$ ./benchmark_sqrt 0
Device ID 0 (AMD Radeon Graphics) UUID: GPU-4124412172e62126
Number of CUs/SMs: 60
Allocating 6103 MB of VRAM...
Launching Kernel across 1666667 thread blocks...
--------------------------------------------------------
Execution Completed Successfully.
Execution Time: 0.00930763 seconds
Total Sqrt Operations: 4e+09
Verification Check (Last Element): 3.51286
429.755 double Gsqrt/s 
hermann@7600x:~$ 
hermann@7600x:~$ ./benchmark_sqrt 1
Device ID 1 (AMD Instinct MI50/MI60) UUID: GPU-c49e19417337ece3
Number of CUs/SMs: 60
Allocating 6103 MB of VRAM...
Launching Kernel across 1666667 thread blocks...
--------------------------------------------------------
Execution Completed Successfully.
Execution Time: 0.00936955 seconds
Total Sqrt Operations: 4e+09
Verification Check (Last Element): 3.51286
426.915 double Gsqrt/s 
hermann@7600x:~$ 
hermann@7600x:~$ ./benchmark_sqrt 2
Device ID 2 (AMD Instinct MI50/MI60) UUID: GPU-6a0e7961732c730d
Number of CUs/SMs: 60
Allocating 6103 MB of VRAM...
Launching Kernel across 1666667 thread blocks...
--------------------------------------------------------
Execution Completed Successfully.
Execution Time: 0.00935643 seconds
Total Sqrt Operations: 4e+09
Verification Check (Last Element): 3.51286
427.513 double Gsqrt/s 
hermann@7600x:~$ 
hermann@7600x:~$ ./benchmark_sqrt 3
Device ID 3 (AMD Instinct MI50/MI60) UUID: GPU-13c24061732c730c
Number of CUs/SMs: 60
Allocating 6103 MB of VRAM...
Launching Kernel across 1666667 thread blocks...
--------------------------------------------------------
Execution Completed Successfully.
Execution Time: 0.00916043 seconds
Total Sqrt Operations: 4e+09
Verification Check (Last Element): 3.51286
436.661 double Gsqrt/s 
hermann@7600x:~$ 
hermann@7600x:~$ ./benchmark_sqrt 4
Device ID 4 (AMD Instinct MI50/MI60) UUID: GPU-304c70e172dc768c
Number of CUs/SMs: 60
Allocating 6103 MB of VRAM...
Launching Kernel across 1666667 thread blocks...
--------------------------------------------------------
Execution Completed Successfully.
Execution Time: 0.00930139 seconds
Total Sqrt Operations: 4e+09
Verification Check (Last Element): 3.51286
430.043 double Gsqrt/s 
hermann@7600x:~$ 
hermann@7600x:~$ ./benchmark_sqrt 5
Device ID 5 (AMD Instinct MI50/MI60) UUID: GPU-6e56508172dc76b6
Number of CUs/SMs: 60
Allocating 6103 MB of VRAM...
Launching Kernel across 1666667 thread blocks...
--------------------------------------------------------
Execution Completed Successfully.
Execution Time: 0.00927963 seconds
Total Sqrt Operations: 4e+09
Verification Check (Last Element): 3.51286
431.052 double Gsqrt/s 
hermann@7600x:~$ 
hermann@7600x:~$ ./benchmark_sqrt 6
Device ID 6 (AMD Instinct MI50/MI60) UUID: GPU-d64a58a17330f0ed
Number of CUs/SMs: 60
Allocating 6103 MB of VRAM...
Launching Kernel across 1666667 thread blocks...
--------------------------------------------------------
Execution Completed Successfully.
Execution Time: 0.00935898 seconds
Total Sqrt Operations: 4e+09
Verification Check (Last Element): 3.51286
427.397 double Gsqrt/s 
hermann@7600x:~$ 
hermann@7600x:~$ ./benchmark_sqrt 7
Device ID 7 (AMD Instinct MI50/MI60) UUID: GPU-f890794172e62691
Number of CUs/SMs: 60
Allocating 6103 MB of VRAM...
Launching Kernel across 1666667 thread blocks...
--------------------------------------------------------
Execution Completed Successfully.
Execution Time: 0.00935211 seconds
Total Sqrt Operations: 4e+09
Verification Check (Last Element): 3.51286
427.711 double Gsqrt/s 
hermann@7600x:~$ 
```


With 200 loops of 10 sqrts each:  
```
hermann@Radeon-pro-vii:~/RR/tsp/hip$ ./benchmark_sqrt 0 200
Device ID 0 (AMD Radeon (TM) Pro VII) UUID: GPU-bf1478a17337ecdb
Number of CUs/SMs: 60
Allocating 6103 MB of VRAM...
Launching Kernel across 1666667 thread blocks...
--------------------------------------------------------
Execution Completed Successfully.
Execution Time: 0.760156 seconds
Total Sqrt Operations: 8e+11
Verification Check (Last Element): 3.51283
1052.42 double Gsqrt/s 
hermann@Radeon-pro-vii:~/RR/tsp/hip$ 
```
