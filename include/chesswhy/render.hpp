// Deterministic template rendering of a report (M1 default output, later --no-llm and
// the fact-check fallback). Frozen by Planning/milestones/M1.md §Template.
#pragma once

#include "chesswhy/report.hpp"

#include <string>

namespace chesswhy::render {

// Uses only report fields; adds no chess claim. Ignores report.input.level in M1.
[[nodiscard]] std::string text(const Report& report);

} // namespace chesswhy::render
