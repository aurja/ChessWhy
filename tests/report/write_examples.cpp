#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

#include "example_reports.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: report_schema_examples <output-directory>\n";
        return 2;
    }

    const std::filesystem::path output_directory(argv[1]);
    std::filesystem::create_directories(output_directory);
    constexpr std::array<const char*, 8> filenames{
        "terminal_checkmate.json",      "side_in_check.json",      "normal_position.json",
        "pgn_opening.json",              "tactic_with_labels.json", "forced_judgement.json",
        "negative_margin_judgement.json", "refutation_judgement.json"};
    const auto reports = chesswhy::test_examples::all_reports();
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
    return 0;
}
