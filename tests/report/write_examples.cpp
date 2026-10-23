#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

#include "chesswhy/move_report.hpp"

#include "example_reports.hpp"

int main(int argc, char** argv) noexcept {
    try {
        if (argc != 2) {
            std::cerr << "usage: report_schema_examples <output-directory>\n";
            return 2;
        }

        const std::filesystem::path output_directory(argv[1]);
        std::filesystem::create_directories(output_directory);
        constexpr std::array<const char*, 8> filenames{
            "terminal_checkmate.json",        "side_in_check.json",
            "normal_position.json",           "pgn_opening.json",
            "tactic_with_labels.json",        "forced_opportunity.json",
            "negative_margin_opportunity.json", "refutation_opportunity.json"};
        constexpr std::array<const char*, 8> move_filenames{
            "moves_terminal_checkmate.json", "moves_side_in_check.json", "moves_normal_position.json",
            "moves_pgn_opening.json", "moves_tactic_with_labels.json", "moves_forced_opportunity.json",
            "moves_negative_margin_opportunity.json", "moves_refutation_opportunity.json"};
        const auto reports = chesswhy::test_examples::schema_reports();
        for (std::size_t index = 0; index < reports.size(); ++index) {
            std::ofstream output(output_directory / filenames[index], std::ios::binary);
            if (!output) {
                std::cerr << "cannot write schema example " << filenames[index] << '\n';
                return 1;
            }
            output << chesswhy::to_json(reports[index]).dump(2);
            if (!output) {
                std::cerr << "failed writing schema example " << filenames[index] << '\n';
                return 1;
            }
        }
        for (std::size_t index = 0; index < reports.size(); ++index) {
            std::ofstream output(output_directory / move_filenames[index], std::ios::binary);
            if (!output) {
                std::cerr << "cannot write move schema example " << move_filenames[index] << '\n';
                return 1;
            }
            output << chesswhy::to_json(chesswhy::move_reports(reports[index])).dump(2);
            if (!output) {
                std::cerr << "failed writing move schema example " << move_filenames[index] << '\n';
                return 1;
            }
        }
        return 0;
    } catch (...) {
        std::terminate();
    }
}
