# Compilers & Flags
CXX      := g++
CXXFLAGS := -O3 -std=c++20 -Wall -Wextra -pedantic
LDLIBS   := -lstdc++ -lm

HIPCC    := hipcc
HIP_PLATFORM ?= $(shell hipconfig --platform 2>/dev/null || echo amd)
ifeq ($(HIP_PLATFORM), nvidia)
    ARCH_FLAGS := -arch=sm_60  # Tesla P100
else
    ARCH_FLAGS := --offload-arch=gfx906  # Instinct MI50 & Radeon [Pro] VII
endif
HIPFLAGS := -O3 -std=c++20 $(ARCH_FLAGS)

# Targets
CPU_TARGETS := load_test gen.ml100K.UINT16_L
GPU_TARGETS := opt_tour_length_benchmark opt_tour_length_benchmark_fast sqrt_test register_sqrt_benchmark
ALL_TARGETS := $(CPU_TARGETS) $(GPU_TARGETS)

.PHONY: all clean

all: $(ALL_TARGETS)

# C++ binaries (share identical flags and header dependency)
$(CPU_TARGETS): %: %.cpp ../loader.h
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)

# HIP/GPU binary
opt_tour_length_benchmark: opt_tour_length_benchmark.cpp
	$(HIPCC) $(HIPFLAGS) $< -o $@

opt_tour_length_benchmark_fast: opt_tour_length_benchmark.cpp
	$(HIPCC) $(HIPFLAGS) -DFAST_SQRT $< -o $@

sqrt_test: sqrt_test.cpp
	$(HIPCC) $(HIPFLAGS) $< -o $@

register_sqrt_benchmark: register_sqrt_benchmark.cpp
	$(HIPCC) $(HIPFLAGS) $< -o $@

clean:
	rm -f $(ALL_TARGETS) ml100K.UINT16_L.bin
