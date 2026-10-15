#include "alignment_internal.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

static Alignment align_cpu(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring,
    bool parallel,
    std::size_t tile_size = 0
) {
    validate_alignment_inputs(a, b, scoring);
    const int limit = std::numeric_limits<int>::max() / 4;
    const std::size_t rows = a.size() + 1;
    const std::size_t cols = b.size() + 1;
    const std::size_t cells = rows * cols;
    const int negative_infinity = -limit;
    std::vector<int> h(cells, 0);
    std::vector<int> e(cells, negative_infinity);
    std::vector<int> f(cells, negative_infinity);

    auto compute_cell = [&](std::size_t i, std::size_t j) {
        const std::size_t cell = i * cols + j;
        const std::size_t up = (i - 1) * cols + j;
        const std::size_t left = cell - 1;
        const std::size_t diagonal = up - 1;

        e[cell] = std::max(
            h[up] - scoring.gap_open,
            e[up] - scoring.gap_extend
        );
        f[cell] = std::max(
            h[left] - scoring.gap_open,
            f[left] - scoring.gap_extend
        );

        const int substitution =
            a[i - 1] == b[j - 1]
            ? scoring.match : scoring.mismatch;

        h[cell] = std::max({
            0, h[diagonal] + substitution, e[cell], f[cell]
        });
    };

    if (parallel && tile_size > 0 && !a.empty() && !b.empty()) {
        const std::size_t tile_rows =
            a.size() / tile_size + (a.size() % tile_size != 0);
        const std::size_t tile_cols =
            b.size() / tile_size + (b.size() % tile_size != 0);

        #pragma omp parallel
        {
            for (std::size_t diagonal = 0;
                 diagonal < tile_rows + tile_cols - 1; ++diagonal) {
                const std::size_t first =
                    diagonal >= tile_cols
                    ? diagonal - tile_cols + 1 : 0;
                const std::size_t last =
                    std::min(tile_rows - 1, diagonal);

                #pragma omp for schedule(static)
                for (std::ptrdiff_t row =
                         static_cast<std::ptrdiff_t>(first);
                     row <= static_cast<std::ptrdiff_t>(last); ++row) {
                    const std::size_t tile_row =
                        static_cast<std::size_t>(row);
                    const std::size_t tile_col = diagonal - tile_row;

                    const std::size_t begin_i = tile_row * tile_size + 1;
                    const std::size_t begin_j = tile_col * tile_size + 1;
                    const std::size_t end_i =
                        begin_i - 1 +
                        std::min(tile_size, a.size() - begin_i + 1);
                    const std::size_t end_j =
                        begin_j - 1 +
                        std::min(tile_size, b.size() - begin_j + 1);

                    for (std::size_t i = begin_i; i <= end_i; ++i) {
                        for (std::size_t j = begin_j; j <= end_j; ++j) {
                            compute_cell(i, j);
                        }
                    }
                }
            }
        }
    } else if (parallel) {
        #pragma omp parallel
        {
            for (std::size_t diagonal = 2;
                 diagonal < rows + cols - 1; ++diagonal) {
                const std::size_t first =
                    diagonal > b.size() ? diagonal - b.size() : 1;
                const std::size_t last =
                    std::min(a.size(), diagonal - 1);

                #pragma omp for schedule(static)
                for (std::ptrdiff_t row =
                         static_cast<std::ptrdiff_t>(first);
                     row <= static_cast<std::ptrdiff_t>(last); ++row) {
                    const std::size_t i = static_cast<std::size_t>(row);
                    compute_cell(i, diagonal - i);
                }
            }
        }
    } else {
        for (std::size_t i = 1; i < rows; ++i) {
            for (std::size_t j = 1; j < cols; ++j) {
                compute_cell(i, j);
            }
        }
    }

    return traceback_alignment(a, b, scoring, h, e, f);
}

Alignment align_scalar(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring
) {
    return align_cpu(a, b, scoring, false);
}

Alignment align_openmp(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring
) {
    return align_cpu(a, b, scoring, true);
}

Alignment align_openmp_tiled(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring,
    std::size_t tile_size
) {
    if (tile_size == 0 || tile_size > 1024) {
        throw std::invalid_argument("Tile size must be between 1 and 1024.");
    }
    return align_cpu(a, b, scoring, true, tile_size);
}
