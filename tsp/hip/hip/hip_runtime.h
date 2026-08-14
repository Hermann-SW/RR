#ifndef LOCAL_HIP_RUNTIME_SHIM_H
#define LOCAL_HIP_RUNTIME_SHIM_H

#include <cuda_runtime.h>
#include <stdio.h>

// Memory Management
#define hipMalloc                cudaMalloc
#define hipFree                  cudaFree
#define hipMemcpy                cudaMemcpy
#define hipMemcpyHostToDevice    cudaMemcpyHostToDevice
#define hipMemcpyDeviceToHost    cudaMemcpyDeviceToHost
#define hipMemcpyDeviceToDevice  cudaMemcpyDeviceToDevice
#define hipMemset                cudaMemset

// Error Handling
#define hipSuccess               cudaSuccess
#define hipError_t               cudaError_t
#define hipGetLastError          cudaGetLastError
#define hipGetErrorString        cudaGetErrorString

// Device & Stream Operations
#define hipDeviceSynchronize     cudaDeviceSynchronize
#define hipStream_t              cudaStream_t
#define hipStreamCreate          cudaStreamCreate
#define hipStreamDestroy         cudaStreamDestroy
#define hipDeviceProp_t          cudaDeviceProp
#define hipGetDeviceProperties   cudaGetDeviceProperties
#define hipSetDevice             cudaSetDevice
#define hipGetDeviceCount        cudaGetDeviceCount

// Timing Events
#define hipEvent_t               cudaEvent_t
#define hipEventCreate           cudaEventCreate
#define hipEventRecord           cudaEventRecord
#define hipEventSynchronize      cudaEventSynchronize
#define hipEventElapsedTime      cudaEventElapsedTime
#define hipEventDestroy          cudaEventDestroy

// Kernel Launch & Dynamic Shared Memory
#define hipLaunchKernelGGL(kernel, grid, block, shared, stream, ...) \
    kernel<<<grid, block, shared, stream>>>(__VA_ARGS__)

#define HIP_DYNAMIC_SHARED(type, name) extern __shared__ type name[];

#endif // LOCAL_HIP_RUNTIME_SHIM_H
