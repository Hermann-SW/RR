# Compilers & Flags
CXX      := g++
CXXFLAGS := -O3 -std=c++20 -Wall -Wextra -pedantic
LDLIBS   := -lstdc++ -lm

HIPCC    := hipcc
HIPFLAGS := -O3 -std=c++20 --offload-arch=gfx906

# Targets
CPU_TARGETS := load_test gen.ml100K.UINT16_L
GPU_TARGETS := opt_tour_length_benchmark
ALL_TARGETS := $(CPU_TARGETS) $(GPU_TARGETS)

.PHONY: all clean

all: $(ALL_TARGETS)

# C++ binaries (share identical flags and header dependency)
$(CPU_TARGETS): %: %.cpp ../loader.h
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)

# HIP/GPU binary
opt_tour_length_benchmark: opt_tour_length_benchmark.cpp
	$(HIPCC) $(HIPFLAGS) $< -o $@

clean:
	rm -f $(ALL_TARGETS) ml100K.UINT16_L.bin
