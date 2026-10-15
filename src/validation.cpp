#include "alignment_internal.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

void validate_alignment_inputs(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring
) {
    const int limit = std::numeric_limits<int>::max() / 4;
    if (scoring.match <= 0 || scoring.match > limit ||
        scoring.mismatch > 0 || scoring.mismatch < -limit ||
        scoring.gap_open <= 0 || scoring.gap_open > limit ||
        scoring.gap_extend <= 0 ||
        scoring.gap_extend > scoring.gap_open) {
        throw std::invalid_argument("Invalid scoring parameters.");
    }

    if (std::min(a.size(), b.size()) >
        static_cast<std::size_t>(
            std::numeric_limits<int>::max() / scoring.match)) {
        throw std::overflow_error("Alignment score could exceed int range.");
    }

    const std::size_t rows = a.size() + 1;
    const std::size_t cols = b.size() + 1;
    if (rows == 0 || cols == 0 ||
        rows > std::numeric_limits<std::size_t>::max() / cols) {
        throw std::overflow_error("Matrix dimensions are too large.");
    }

}
