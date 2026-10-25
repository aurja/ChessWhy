// Deterministic template rendering of a report (M1 default output, later --no-llm and
// the fact-check fallback). Frozen by Planning/milestones/M1.md §Template.
#pragma once

#include "chesswhy/report.hpp"
namespace chesswhy {
struct MoveReportDocument;
} // namespace chesswhy

#include <string>

namespace chesswhy::render {

// Uses only report fields; adds no chess claim.
[[nodiscard]] std::string text(const Report& report);
[[nodiscard]] std::string text(const MoveReportDocument& document);

} // namespace chesswhy::render
