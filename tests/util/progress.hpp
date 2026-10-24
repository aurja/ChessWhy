#pragma once

#include <chrono>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace chesswhy::test {

[[nodiscard]] inline std::size_t count_csv_data_rows(const std::string& path) {
    std::ifstream input(path);
    std::string line;
    if (!input || !std::getline(input, line)) {
        throw std::runtime_error("cannot count CSV data rows in " + path);
    }
    std::size_t rows = 0;
    while (std::getline(input, line)) {
        ++rows;
    }
    if (input.bad()) {
        throw std::runtime_error("failed reading CSV data rows in " + path);
    }
    return rows;
}

class Progress final {
public:
    using Clock = std::chrono::steady_clock;

    Progress(std::string_view label, std::size_t total)
        : label_(label), total_(total), started_(Clock::now()), last_item_update_(started_) {}

    void item_completed(std::size_t completed, std::string_view detail) {
        if (completed == 0 || completed > total_) {
            return;
        }
        const auto now = Clock::now();
        if (completed != total_ && now - last_item_update_ < std::chrono::seconds{1}) {
            return;
        }

        const double elapsed = std::chrono::duration<double>(now - started_).count();
        const double remaining =
            completed < total_
                ? elapsed * static_cast<double>(total_ - completed) / static_cast<double>(completed)
                : 0.0;
        emit([&](std::ostream& output) {
            output << "progress: " << label_ << ' ' << completed << '/' << total_ << ' ' << detail
                   << " (" << std::fixed << std::setprecision(1) << elapsed << " s elapsed";
            if (completed < total_) {
                output << ", ~" << remaining << " s left";
            }
            output << ')';
        });
        last_item_update_ = now;
    }

    void step_started(std::size_t index, std::string_view action, std::string_view detail) {
        step_started_ = Clock::now();
        emit([&](std::ostream& output) {
            output << "progress: " << label_ << ' ' << action << ' ' << index << '/' << total_
                   << ' ' << detail << " started (" << std::fixed << std::setprecision(1)
                   << elapsed_seconds(step_started_) << " s elapsed)";
        });
    }

    void step_finished(std::size_t index, std::string_view action, std::string_view detail) {
        const auto now = Clock::now();
        emit([&](std::ostream& output) {
            output << "progress: " << label_ << ' ' << action << ' ' << index << '/' << total_
                   << ' ' << detail << " done in " << std::fixed << std::setprecision(1)
                   << std::chrono::duration<double>(now - step_started_).count() << " s ("
                   << elapsed_seconds(now) << " s elapsed)";
        });
    }

private:
    template <typename Write>
    static void emit(Write&& write) {
        const auto flags = std::cerr.flags();
        const auto precision = std::cerr.precision();
        write(std::cerr);
        std::cerr.flags(flags);
        std::cerr.precision(precision);
        std::cerr << '\n' << std::flush;
    }

    [[nodiscard]] double elapsed_seconds(Clock::time_point now) const {
        return std::chrono::duration<double>(now - started_).count();
    }

    std::string_view label_;
    std::size_t total_;
    Clock::time_point started_;
    Clock::time_point last_item_update_;
    Clock::time_point step_started_{};
};

} // namespace chesswhy::test
