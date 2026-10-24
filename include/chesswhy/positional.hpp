// Positional features of a board and their changes along a line. Frozen by
// Planning/milestones/M3a.md §4 (features), §5 (changes) and §7 (API). Pure functions of
// the board; source: rule. No engine call.
#pragma once

#include "chesswhy/report.hpp"

#include <span>
#include <vector>

#include <chess.hpp>

namespace chesswhy::positional {

// Lead thresholds (M3a §4.4, P4).
inline constexpr int kMobilityLead = 10;
inline constexpr int kSpaceLead = 3;
inline constexpr int kDevelopmentLead = 2;

// M3a §4.4 counts for one side.
[[nodiscard]] int mobility(const chess::Board& board, chess::Color side);
[[nodiscard]] int space(const chess::Board& board, chess::Color side);
[[nodiscard]] int undeveloped(const chess::Board& board, chess::Color side);

// Every feature of M3a §4 for both sides, in §4.6 order.
[[nodiscard]] std::vector<PositionalFeature> features(const chess::Board& board);

// M3a §4 identity (the tag's Identity column, plus tag and side); false for different tags
// or sides.
[[nodiscard]] bool same_identity(const PositionalFeature& a, const PositionalFeature& b);

// M3a §5: changes along `line`, which is already cut to the walked plies and legal from
// `root`. Ordered by ply, then appears before disappears, then §4.6.
[[nodiscard]] std::vector<PositionalChange> line_changes(const chess::Board& root,
                                                         std::span<const chess::Move> line);
enum class StaticScaleGroup : std::uint8_t {
    KingSafety,
    Material,
    PawnStructure,
    Pieces,
    SpaceMobilityDevelopment
};
[[nodiscard]] StaticScaleGroup static_scale_group(PositionalTag tag);
[[nodiscard]] bool is_dynamic(PositionalTag tag);
[[nodiscard]] std::optional<Side> favours(const PositionalChange& change); // §6.4
// §6.5 and §6.6 on the output of line_changes(root, line); computes the root features.
void mark_use(std::vector<PositionalChange>& changes, const chess::Board& root,
              std::span<const chess::Move> line, bool tempo_dependent);
// §6.7 positional order; stable.
void impact_order(std::vector<PositionalChange>& changes, bool tempo_dependent);

} // namespace chesswhy::positional
