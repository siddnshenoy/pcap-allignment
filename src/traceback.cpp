#include "alignment_internal.hpp"

#include <algorithm>

Alignment traceback_alignment(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring,
    const std::vector<int>& h,
    const std::vector<int>& e,
    const std::vector<int>& f
) {
    const std::size_t rows = a.size() + 1;
    const std::size_t cols = b.size() + 1;
    Alignment result;

    // Row-major scanning preserves the scalar endpoint tie rule.
    for (std::size_t row = 1; row < rows; ++row) {
        for (std::size_t col = 1; col < cols; ++col) {
            const int value = h[row * cols + col];
            if (value > result.score) {
                result.score = value;
                result.end_a = row;
                result.end_b = col;
            }
        }
    }

    std::size_t i = result.end_a;
    std::size_t j = result.end_b;
    char state = 'H';

    while (true) {
        const std::size_t cell = i * cols + j;

        if (state == 'H') {
            if (h[cell] == 0) break;

            const int substitution =
                a[i - 1] == b[j - 1]
                ? scoring.match : scoring.mismatch;

            if (h[cell] ==
                h[(i - 1) * cols + j - 1] + substitution) {
                result.aligned_a += a[--i];
                result.aligned_b += b[--j];
            } else if (h[cell] == e[cell]) {
                state = 'E';
            } else {
                state = 'F';
            }
        } else if (state == 'E') {
            const std::size_t up = (i - 1) * cols + j;
            const bool opened =
                h[up] - scoring.gap_open >=
                e[up] - scoring.gap_extend;

            result.aligned_a += a[--i];
            result.aligned_b += '-';
            state = opened ? 'H' : 'E';
        } else {
            const std::size_t left = cell - 1;
            const bool opened =
                h[left] - scoring.gap_open >=
                f[left] - scoring.gap_extend;

            result.aligned_a += '-';
            result.aligned_b += b[--j];
            state = opened ? 'H' : 'F';
        }
    }

    result.begin_a = i;
    result.begin_b = j;

    std::reverse(result.aligned_a.begin(), result.aligned_a.end());
    std::reverse(result.aligned_b.begin(), result.aligned_b.end());

    char previous = '\0';
    std::size_t count = 0;

    for (std::size_t k = 0; k < result.aligned_a.size(); ++k) {
        const char x = result.aligned_a[k];
        const char y = result.aligned_b[k];
        const char operation =
            x == '-' ? 'D' :
            y == '-' ? 'I' :
            x == y ? '=' : 'X';

        if (operation != previous && count > 0) {
            result.cigar += std::to_string(count);
            result.cigar += previous;
            count = 0;
        }

        previous = operation;
        ++count;
    }

    if (count > 0) {
        result.cigar += std::to_string(count);
        result.cigar += previous;
    }

    return result;
}
