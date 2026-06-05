Radeon VII GPU has 3.36 TFLOPS FP64:
https://www.techpowerup.com/gpu-specs/radeon-vii.c3358

With 4 cycles per double sqrt operation on that GPU, theoretical limit is 3.36 / 4 = 840 double sqrt GFLOPS.

```
hipcc -O3 --amdgpu-target=gfx906 benchmark_sqrt.cpp -o benchmark_sqrt
rocm-smi --gpureset -d 0
```

(gemini) [benchmark_sqrt.cpp](./benchmark_sqrt) reports 257 GFLOPS measured:
```
$ rocprofv2 -i <(echo "pmc : L2CacheHit, VALUUtilization") ./benchmark_sqrt
Allocating 1525 MB of VRAM...
ROCProfilerV2: Collecting the following counters:
- L2CacheHit
- VALUUtilization
Enabling Counter Collection
Launching Kernel across 390625 thread blocks...
--------------------------------------------------------
Execution Completed Successfully.
Execution Time: 0.00388257 seconds
Total Sqrt Operations: 1e+09
Verification Check (Last Element): 3.51285
257.561 Genuine double sqrt GFLOPS
Dispatch_ID(0), GPU_ID(1), Queue_ID(1), Process_ID(13221), Thread_ID(13221), Grid_Size(100000000), Workgroup_Size(256), LDS_Per_Workgroup(0), Scratch_Per_Workitem(0), Arch_VGPR(8), Accum_VGPR(0), SGPR(16), Wave_Size(64), Kernel_Name("genuine_sqrt_kernel(double const*, double*, int) (.kd)"), Begin_Timestamp(27595851401228), End_Timestamp(27595854073395), Correlation_ID(0), L2CacheHit(0.000735), VALUUtilization(100.000000)
$
```

profiler timestamps show that less execution time happened, only 2.67ms:
```
echo "(27595854073395-27595851401228)/10^9" | bc -ql
.00267216700000000000
```

That time corresponds to 374 double sqrt GFLOPS, which is 44.5% of theoretical maximum.
