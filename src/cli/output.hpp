#pragma once

#include "chesswhy/cli.hpp"

namespace chesswhy::cli::detail {

void write_output(const Options& options, const Report& report, std::ostream& out);

} // namespace chesswhy::cli::detail
