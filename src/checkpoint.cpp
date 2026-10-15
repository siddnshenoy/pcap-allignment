#include "alignment_internal.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

struct Checkpoint {
    std::vector<int> h;
    std::vector<int> e;
    std::vector<int> f;
};

}  // namespace

Alignment align_checkpoint(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring,
    std::size_t interval
) {
    validate_alignment_inputs(a, b, scoring);
    if (interval == 0 || interval > 4096) {
        throw std::invalid_argument(
            "Checkpoint interval must be between 1 and 4096."
        );
    }

    const std::size_t cols = b.size() + 1;
    const int negative_infinity =
        -std::numeric_limits<int>::max() / 4;

    std::vector<int> previous_h(cols, 0);
    std::vector<int> previous_e(cols, negative_infinity);
    std::vector<int> previous_f(cols, negative_infinity);
    std::vector<int> current_h(cols, 0);
    std::vector<int> current_e(cols, negative_infinity);
    std::vector<int> current_f(cols, negative_infinity);

    std::vector<Checkpoint> checkpoints;
    checkpoints.push_back({
        previous_h, previous_e, previous_f
    });

    Alignment result;

    for (std::size_t i = 1; i <= a.size(); ++i) {
        current_h[0] = 0;
        current_e[0] = negative_infinity;
        current_f[0] = negative_infinity;

        for (std::size_t j = 1; j <= b.size(); ++j) {
            current_e[j] = std::max(
                previous_h[j] - scoring.gap_open,
                previous_e[j] - scoring.gap_extend
            );
            current_f[j] = std::max(
                current_h[j - 1] - scoring.gap_open,
                current_f[j - 1] - scoring.gap_extend
            );

            const int substitution =
                a[i - 1] == b[j - 1]
                ? scoring.match : scoring.mismatch;

            current_h[j] = std::max({
                0,
                previous_h[j - 1] + substitution,
                current_e[j],
                current_f[j]
            });

            if (current_h[j] > result.score) {
                result.score = current_h[j];
                result.end_a = i;
                result.end_b = j;
            }
        }

        previous_h.swap(current_h);
        previous_e.swap(current_e);
        previous_f.swap(current_f);

        if (i % interval == 0 && i < a.size()) {
            checkpoints.push_back({
                previous_h, previous_e, previous_f
            });
        }
    }

    if (result.score == 0) return result;

    std::size_t i = result.end_a;
    std::size_t j = result.end_b;
    char state = 'H';
    bool finished = false;

    while (!finished && i > 0) {
        const std::size_t base = ((i - 1) / interval) * interval;
        const std::size_t height = i - base;
        const std::size_t cells = (height + 1) * cols;

        std::vector<int> h(cells, 0);
        std::vector<int> e(cells, negative_infinity);
        std::vector<int> f(cells, negative_infinity);

        const Checkpoint& checkpoint = checkpoints[base / interval];
        std::copy(checkpoint.h.begin(), checkpoint.h.end(), h.begin());
        std::copy(checkpoint.e.begin(), checkpoint.e.end(), e.begin());
        std::copy(checkpoint.f.begin(), checkpoint.f.end(), f.begin());

        for (std::size_t row = 1; row <= height; ++row) {
            const std::size_t global_i = base + row;

            for (std::size_t col = 1; col <= b.size(); ++col) {
                const std::size_t cell = row * cols + col;
                const std::size_t up = cell - cols;
                const std::size_t left = cell - 1;

                e[cell] = std::max(
                    h[up] - scoring.gap_open,
                    e[up] - scoring.gap_extend
                );
                f[cell] = std::max(
                    h[left] - scoring.gap_open,
                    f[left] - scoring.gap_extend
                );

                const int substitution =
                    a[global_i - 1] == b[col - 1]
                    ? scoring.match : scoring.mismatch;

                h[cell] = std::max({
                    0, h[up - 1] + substitution, e[cell], f[cell]
                });
            }
        }

        while (i > base) {
            const std::size_t row = i - base;
            const std::size_t cell = row * cols + j;

            if (state == 'H') {
                if (h[cell] == 0) {
                    finished = true;
                    break;
                }

                const int substitution =
                    a[i - 1] == b[j - 1]
                    ? scoring.match : scoring.mismatch;

                if (h[cell] == h[cell - cols - 1] + substitution) {
                    result.aligned_a += a[--i];
                    result.aligned_b += b[--j];
                } else if (h[cell] == e[cell]) {
                    state = 'E';
                } else {
                    state = 'F';
                }
            } else if (state == 'E') {
                const std::size_t up = cell - cols;
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
    }

    finalize_alignment(result, i, j);
    return result;
}
