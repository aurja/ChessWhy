#include "chesswhy/render.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "../report/example_reports.hpp"

TEST_CASE("rendered example reports match the golden text", "[unit]") {
    constexpr std::array<const char*, 4> filenames{"terminal_checkmate.txt", "side_in_check.txt",
                                                   "normal_position.txt", "pgn_opening.txt"};
    const auto reports = chesswhy::test_examples::all_reports();

    for (std::size_t index = 0; index < reports.size(); ++index) {
        CAPTURE(filenames[index]);
        const std::string path =
            std::string(CHESSWHY_SOURCE_DIR) + "/tests/render/golden/" + filenames[index];
        const std::ifstream golden_file(path, std::ios::binary);
        REQUIRE(golden_file.good());
        std::ostringstream golden_content;
        golden_content << golden_file.rdbuf();
        const std::string rendered = chesswhy::render::text(reports[index]);
        CHECK(rendered == golden_content.str());

        std::istringstream rendered_lines(rendered);
        std::string line;
        while (std::getline(rendered_lines, line)) {
            if (line.empty()) {
                continue;
            }
            const std::size_t provenance = line.rfind(" [");
            CHECK(provenance != std::string::npos);
            if (provenance != std::string::npos) {
                CHECK(line.back() == ']');
                CHECK(provenance + 3 < line.size());
                CHECK(line.find(']', provenance + 2) == line.size() - 1);
            }
        }
    }
}

TEST_CASE("rendering is ASCII and ignores the requested explanation level", "[unit]") {
    auto report = chesswhy::test_examples::pgn_opening();
    const std::string club_text = chesswhy::render::text(report);
    report.input.level = chesswhy::Level::Beginner;
    CHECK(chesswhy::render::text(report) == club_text);
    CHECK(std::ranges::all_of(
        club_text, [](char character) { return static_cast<unsigned char>(character) <= 0x7f; }));
}

TEST_CASE("position evaluation renders the mate score when available", "[unit]") {
    auto report = chesswhy::test_examples::normal_position();
    report.position.eval_white = chesswhy::Eval{.es_units = 1000, .cp = std::nullopt, .mate = 3};
    const std::string rendered = chesswhy::render::text(report);
    CHECK(rendered.find("Position evaluation: 50.0% ES, mate=3 [engine]") != std::string::npos);
}
