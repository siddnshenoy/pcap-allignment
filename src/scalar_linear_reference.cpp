#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " SEQUENCE1 SEQUENCE2\n";
        return 1;
    }

    const std::string a = argv[1];
    const std::string b = argv[2];

    if (a.empty() || b.empty() ||
        a.find_first_not_of("ACGT") != std::string::npos ||
        b.find_first_not_of("ACGT") != std::string::npos) {
        std::cerr << "Use nonempty DNA sequences containing uppercase A, C, G, T.\n";
        return 1;
    }

    const std::size_t rows = a.size() + 1;
    const std::size_t cols = b.size() + 1;
    std::vector<int> scores(rows * cols, 0);
    std::vector<char> directions(rows * cols, 0);

    int best_score = 0;
    std::size_t best_i = 0, best_j = 0;

    for (std::size_t i = 1; i < rows; ++i) {
        for (std::size_t j = 1; j < cols; ++j) {
            const int diagonal = scores[(i - 1) * cols + j - 1]
                               + (a[i - 1] == b[j - 1] ? 2 : -1);
            const int up = scores[(i - 1) * cols + j] - 2;
            const int left = scores[i * cols + j - 1] - 2;
            const int value = std::max({0, diagonal, up, left});
            const std::size_t cell = i * cols + j;

            scores[cell] = value;
            if (value > 0) {
                if (value == diagonal) directions[cell] = 'D';
                else if (value == up) directions[cell] = 'U';
                else directions[cell] = 'L';
            }

            if (value > best_score) {
                best_score = value;
                best_i = i;
                best_j = j;
            }
        }
    }

    std::string aligned_a, aligned_b;
    std::size_t i = best_i, j = best_j;

    while (scores[i * cols + j] > 0) {
        const char direction = directions[i * cols + j];
        if (direction == 'D') {
            aligned_a += a[--i];
            aligned_b += b[--j];
        } else if (direction == 'U') {
            aligned_a += a[--i];
            aligned_b += '-';
        } else {
            aligned_a += '-';
            aligned_b += b[--j];
        }
    }

    std::reverse(aligned_a.begin(), aligned_a.end());
    std::reverse(aligned_b.begin(), aligned_b.end());

    std::cout << "Score: " << best_score << '\n'
              << "Sequence 1: " << aligned_a << '\n'
              << "Sequence 2: " << aligned_b << '\n';
    return 0;
}
