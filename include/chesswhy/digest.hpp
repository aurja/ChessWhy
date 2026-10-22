// SHA-256 digests of loaded data files (PostM3_EM2 §4.5). In-tree implementation of
// FIPS 180-4, no dependency. Frozen by Planning/milestones/PostM3_EM2.md §6.
#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace chesswhy {

// SHA-256 of `bytes` as 64 lower-case hex digits.
[[nodiscard]] std::string sha256_hex(std::span<const std::byte> bytes);

} // namespace chesswhy
