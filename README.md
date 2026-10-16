# PCAP DNA Sequence Alignment

Exact Smith-Waterman local alignment with affine gap penalties. Backends: scalar CPU, OpenMP, tiled OpenMP, CUDA, and tiled CUDA.

## Build

Requirements: CMake 3.24+, C++17 compiler, OpenMP, CUDA Toolkit, NVIDIA GPU, Python 3, and Ninja.

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build

## Run

    ./build/pcap AAAACCCCGGGG AAAAGGGG --gap-open 3 --gap-extend 1 --backend cuda-tiled --tile-size 32

Use --backend to select scalar, openmp, openmp-tiled, cuda, or cuda-tiled. Set OMP_NUM_THREADS to control CPU thread count. Run ./build/pcap without arguments to see all options.

## Tests and results

    ctest --test-dir build --output-on-failure

The correctness suite includes 10 tests; CUDA tests can take several minutes. Benchmark scripts are in tests/. Curated measurements and environment details are in analysis/. Plots are in figures/. Raw outputs in results/ are excluded from Git.
