#include "output.hpp"

#include "chesswhy/move_report.hpp"
#include "chesswhy/render.hpp"

#include <ostream>

#include <nlohmann/json.hpp>

namespace chesswhy::cli::detail {

void write_output(const Options& options, const Report& report, std::ostream& out) {
    if (options.full_report) {
        if (options.json) {
            out << to_json(report).dump(2) << '\n';
        } else {
            out << render::text(report);
        }
        return;
    }

    const MoveReportDocument document = move_reports(report);
    if (options.json) {
        out << to_json(document).dump(2) << '\n';
    } else {
        out << render::text(document);
    }
}

} // namespace chesswhy::cli::detail
