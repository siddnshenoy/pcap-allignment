#include "alignment.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <string>

int parse_integer(const std::string& text) {
    std::size_t consumed = 0;
    const int value = std::stoi(text, &consumed);
    if (consumed != text.size()) {
        throw std::invalid_argument("Expected an integer: " + text);
    }
    return value;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr
            << "Usage: " << argv[0] << " SEQUENCE1 SEQUENCE2 [options]\n"
            << "Options: --match N --mismatch N "
            << "--gap-open N --gap-extend N --backend scalar|openmp|openmp-tiled|cuda|cuda-tiled|checkpoint --tile-size N --checkpoint-rows N --repetitions N\n";
        return 1;
    }

    try {
        const std::string a = argv[1];
        const std::string b = argv[2];

        if (a.empty() || b.empty() ||
            a.find_first_not_of("ACGT") != std::string::npos ||
            b.find_first_not_of("ACGT") != std::string::npos) {
            throw std::invalid_argument(
                "Use nonempty DNA sequences containing uppercase A, C, G, T."
            );
        }

        Scoring scoring;
        std::string backend = "scalar";
        int tile_size = 32;
        int checkpoint_rows = 64;
        int repetitions = 1;
        bool show_timing = false;

        for (int k = 3; k < argc; k += 2) {
            const std::string option = argv[k];
            if (k + 1 >= argc) {
                throw std::invalid_argument(
                    "Missing value for " + option
                );
            }

            if (option == "--backend") {
                backend = argv[k + 1];
                if (backend != "scalar" && backend != "openmp" &&
                    backend != "openmp-tiled" && backend != "cuda" &&
                    backend != "cuda-tiled" &&
                    backend != "checkpoint") {
                    throw std::invalid_argument(
                        "Backend must be scalar, openmp, openmp-tiled, cuda, cuda-tiled, or checkpoint."
                    );
                }
                continue;
            }

            const int value = parse_integer(argv[k + 1]);

            if (option == "--repetitions") {
                if (value < 1 || value > 100) {
                    throw std::invalid_argument(
                        "Repetitions must be between 1 and 100."
                    );
                }
                repetitions = value;
                show_timing = true;
            }
            else if (option == "--checkpoint-rows") {
                if (value < 1 || value > 4096) {
                    throw std::invalid_argument(
                        "Checkpoint rows must be between 1 and 4096."
                    );
                }
                checkpoint_rows = value;
            }
            else if (option == "--tile-size") {
                if (value < 1 || value > 1024) {
                    throw std::invalid_argument(
                        "Tile size must be between 1 and 1024."
                    );
                }
                tile_size = value;
            }
            else if (option == "--match") scoring.match = value;
            else if (option == "--mismatch") scoring.mismatch = value;
            else if (option == "--gap-open") scoring.gap_open = value;
            else if (option == "--gap-extend") scoring.gap_extend = value;
            else {
                throw std::invalid_argument("Unknown option: " + option);
            }
        }

        auto run_alignment = [&]() -> Alignment {
            return backend == "checkpoint"
            ? align_checkpoint(
                a, b, scoring,
                static_cast<std::size_t>(checkpoint_rows))
            : backend == "cuda-tiled"
            ? align_cuda_tiled(
                a, b, scoring, static_cast<std::size_t>(tile_size))
            : backend == "cuda"
            ? align_cuda(a, b, scoring)
            : backend == "openmp-tiled"
            ? align_openmp_tiled(
                a, b, scoring, static_cast<std::size_t>(tile_size))
            : backend == "openmp"
            ? align_openmp(a, b, scoring)
            : align_scalar(a, b, scoring);
        };

        Alignment result;
        std::vector<double> times_ms;

        if (show_timing) {
            (void)run_alignment();  // Warmup.
        }

        for (int run = 0; run < repetitions; ++run) {
            const auto start_time = std::chrono::steady_clock::now();
            result = run_alignment();
            const auto end_time = std::chrono::steady_clock::now();

            if (show_timing) {
                times_ms.push_back(
                    std::chrono::duration<double, std::milli>(
                        end_time - start_time
                    ).count()
                );
            }
        }

        std::cout
            << "Score: " << result.score << '\n'
            << "Sequence 1: " << result.aligned_a << '\n'
            << "Sequence 2: " << result.aligned_b << '\n'
            << "CIGAR: " << result.cigar << '\n'
            << "Range 1: [" << result.begin_a
            << ", " << result.end_a << ")\n"
            << "Range 2: [" << result.begin_b
            << ", " << result.end_b << ")\n";

        if (show_timing) {
            std::sort(times_ms.begin(), times_ms.end());
            const std::size_t middle = times_ms.size() / 2;
            const double median =
                times_ms.size() % 2 == 0
                ? (times_ms[middle - 1] + times_ms[middle]) / 2.0
                : times_ms[middle];

            std::cout
                << "Median_ms: " << median << '\n'
                << "Min_ms: " << times_ms.front() << '\n'
                << "Max_ms: " << times_ms.back() << '\n';
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
