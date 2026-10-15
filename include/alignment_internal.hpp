#pragma once

#include "alignment.hpp"
#include <vector>

Alignment traceback_alignment(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring,
    const std::vector<int>& h,
    const std::vector<int>& e,
    const std::vector<int>& f
);

void validate_alignment_inputs(
    const std::string& a,
    const std::string& b,
    const Scoring& scoring
);

void finalize_alignment(
    Alignment& result,
    std::size_t begin_a,
    std::size_t begin_b
);
