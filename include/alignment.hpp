#pragma once

#include <cstddef>
#include <string>

struct Scoring {
    int match = 2;
    int mismatch = -1;
    int gap_open = 2;
    int gap_extend = 2;
};

// A gap of length k costs gap_open + (k - 1) * gap_extend.
// Both gap penalties are stored as positive numbers.
// Defaults reproduce our existing linear-gap scoring.

struct Alignment {
    int score = 0;

    std::string aligned_a;
    std::string aligned_b;
    std::string cigar;

    // Zero-based positions in the original sequences.
    // Begin is inclusive; end is exclusive.
    std::size_t begin_a = 0;
    std::size_t end_a = 0;
    std::size_t begin_b = 0;
    std::size_t end_b = 0;
};

Alignment align_scalar(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring = Scoring{}
);

Alignment align_openmp(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring = Scoring{}
);

Alignment align_openmp_tiled(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring = Scoring{},
    std::size_t tile_size = 32
);

Alignment align_cuda(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring = Scoring{}
);

Alignment align_cuda_tiled(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring = Scoring{},
    std::size_t tile_size = 32
);

