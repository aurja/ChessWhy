#include "chesswhy/openings.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

namespace {

using chesswhy::NamedPly;
using chesswhy::OpeningInfo;
using chesswhy::OpeningName;
using chesswhy::openings::OpeningsError;
using chesswhy::openings::Table;
using nlohmann::json;

[[nodiscard]] std::filesystem::path openings_directory() {
    return std::filesystem::path(CHESSWHY_ASSETS_DIR) / "data" / "openings";
}

[[nodiscard]] json read_unit_fixtures() {
    const auto path =
        std::filesystem::path(CHESSWHY_SOURCE_DIR) / "tests" / "fixtures" / "positions.json";
    std::ifstream input(path);
    REQUIRE(input.is_open());
    const json fixtures = json::parse(input);
    REQUIRE(fixtures.contains("unit_fixtures"));
    return fixtures.at("unit_fixtures");
}

[[nodiscard]] json fixture_by_id(const json& fixtures, std::string_view id) {
    for (const auto& fixture : fixtures) {
        if (fixture.at("id").get<std::string>() == id) {
            return fixture;
        }
    }
    FAIL("Unit fixture not found: " << id);
    return json::object();
}

void replay(chess::Board& board, std::vector<chess::Board>& mainline,
            std::initializer_list<std::string_view> san_moves) {
    for (const std::string_view san : san_moves) {
        const chess::Move move = chess::uci::parseSan(board, san);
        REQUIRE(move != chess::Move::NO_MOVE);
        board.makeMove(move);
        mainline.push_back(board);
    }
}

void require_opening(const std::optional<OpeningName>& actual, const json& expected) {
    if (expected.is_null()) {
        CHECK(!actual.has_value());
        return;
    }
    if (!actual.has_value()) {
        FAIL("Expected an opening name");
        return;
    }
    const OpeningName& opening = *actual;
    CHECK(opening.eco == expected.at("eco").get<std::string>());
    CHECK(opening.name == expected.at("name").get<std::string>());
}

void require_named_ply(const std::optional<NamedPly>& actual, const json& expected) {
    if (expected.is_null()) {
        CHECK(!actual.has_value());
        return;
    }
    if (!actual.has_value()) {
        FAIL("Expected a named ply");
        return;
    }
    const NamedPly& named_ply = *actual;
    CHECK(named_ply.opening.eco == expected.at("eco").get<std::string>());
    CHECK(named_ply.opening.name == expected.at("name").get<std::string>());
    CHECK(named_ply.ply == expected.at("ply").get<int>());
}

void require_described(const OpeningInfo& actual, const json& expected) {
    CHECK(actual.dataset == chesswhy::openings::kDataset);
    require_opening(actual.current, expected.at("current"));
    require_named_ply(actual.last_named, expected.at("last_named"));
}

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        static std::atomic<unsigned long long> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto base = std::filesystem::temp_directory_path();
        for (int attempt = 0; attempt < 100; ++attempt) {
            path_ = base / ("chesswhy-openings-" + std::to_string(timestamp) + "-" +
                            std::to_string(sequence.fetch_add(1)));
            std::error_code error;
            if (std::filesystem::create_directory(path_, error)) {
                return;
            }
            if (error) {
                throw std::runtime_error("cannot create temporary openings directory: " +
                                         error.message());
            }
        }
        throw std::runtime_error("cannot create unique temporary openings directory");
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

void write_file(const std::filesystem::path& path, std::string_view contents) {
    std::ofstream output(path);
    REQUIRE(output.is_open());
    output << contents;
    REQUIRE(output.good());
}

template <typename Function>
[[nodiscard]] std::string expect_openings_error(Function&& function) {
    try {
        function();
    } catch (const OpeningsError& error) {
        return error.what();
    }
    FAIL("Expected chesswhy::openings::OpeningsError");
    return {};
}

} // namespace

TEST_CASE("openings names match known positions and mainline ancestors", "[openings]") {
    const std::filesystem::path assets = openings_directory();
    REQUIRE(std::filesystem::exists(assets));
    REQUIRE(std::filesystem::is_directory(assets));
    const Table table = Table::load(assets);
    REQUIRE(table.size() == 3865);

    const json fixtures = read_unit_fixtures();

    const json u01 = fixture_by_id(fixtures, "U01");
    chess::Board board01;
    std::vector<chess::Board> line01;
    replay(board01, line01, {"c4", "e6", "Nc3", "Nf6", "d4", "Bb4"});
    CHECK(board01.getFen() == u01.at("fen").get<std::string>());
    require_described(chesswhy::openings::describe(table, board01, line01), u01);

    const json u02 = fixture_by_id(fixtures, "U02");
    chess::Board board02;
    std::vector<chess::Board> line02;
    replay(board02, line02, {"a3", "a6", "h3", "h6"});
    CHECK(board02.getFen() == u02.at("fen").get<std::string>());
    require_described(chesswhy::openings::describe(table, board02, line02), u02);

    const json u03 = fixture_by_id(fixtures, "U03");
    chess::Board board03;
    std::vector<chess::Board> line03;
    replay(board03, line03, {"d4", "d5", "c4", "e6", "Nc3", "Nf6"});
    CHECK(board03.getFen() == u03.at("fen").get<std::string>());
    require_described(chesswhy::openings::describe(table, board03, line03), u03);

    const json u04 = fixture_by_id(fixtures, "U04");
    chess::Board board04("8/P7/8/8/8/8/2k5/4K3 w - - 0 1");
    std::vector<chess::Board> line04;
    replay(board04, line04, {"a8=Q", "Kd3"});
    CHECK(board04.getFen() == u04.at("fen").get<std::string>());
    require_described(chesswhy::openings::describe(table, board04, line04), u04);

    const json u05 = fixture_by_id(fixtures, "U05");
    const chess::Board board05(u05.at("fen_input").get<std::string>());
    CHECK(chesswhy::openings::position_key(board05) == u05.at("key").get<std::string>());
    const std::vector<chess::Board> no_moves;
    require_described(chesswhy::openings::describe(table, board05, no_moves), u05);

    const json u06 = fixture_by_id(fixtures, "U06");
    const chess::Board board06(u06.at("fen_input").get<std::string>());
    CHECK(chesswhy::openings::position_key(board06) == u06.at("key").get<std::string>());
    require_described(chesswhy::openings::describe(table, board06, no_moves), u06);

    chess::Board fools_mate;
    std::vector<chess::Board> fools_mate_line;
    replay(fools_mate, fools_mate_line, {"f3", "e5", "g4", "Qh4#"});
    const auto fools_mate_opening = table.find(fools_mate);
    if (fools_mate_opening.has_value()) {
        const OpeningName& opening = *fools_mate_opening;
        CHECK(opening.eco == "A00");
        CHECK(opening.name == "Barnes Opening: Fool's Mate");
    } else {
        FAIL("Expected Fool's Mate to have an opening name");
    }

    chess::Board italian;
    std::vector<chess::Board> italian_line;
    replay(italian, italian_line, {"e4", "e5", "Nf3", "Nc6", "Bc4", "Bc5"});
    const auto italian_opening = table.find(italian);
    if (italian_opening.has_value()) {
        const OpeningName& opening = *italian_opening;
        CHECK(opening.eco == "C50");
        CHECK(opening.name == "Italian Game: Giuoco Piano");
    } else {
        FAIL("Expected the Italian position to have an opening name");
    }
}

TEST_CASE("openings table load failures identify their source", "[openings]") {
    SECTION("duplicate position key") {
        const TemporaryDirectory temporary;
        write_file(temporary.path() / "a.tsv",
                   "eco\tname\tpgn\nA00\tFirst Opening\t1. e4\nB00\tSecond Opening\t1. e4\n");
        const std::string error =
            expect_openings_error([&] { (void)Table::load(temporary.path()); });
        CHECK(error.find("openings") != std::string::npos);
        CHECK(error.find((temporary.path() / "a.tsv").string()) != std::string::npos);
        CHECK(error.find(":3:") != std::string::npos);
        CHECK(error.find("First Opening") != std::string::npos);
        CHECK(error.find("Second Opening") != std::string::npos);
    }

    SECTION("malformed field count") {
        const TemporaryDirectory temporary;
        write_file(temporary.path() / "a.tsv", "eco\tname\tpgn\nA00\tOnly Two Fields\n");
        const std::string error =
            expect_openings_error([&] { (void)Table::load(temporary.path()); });
        CHECK(error.find("openings") != std::string::npos);
        CHECK(error.find((temporary.path() / "a.tsv").string()) != std::string::npos);
        CHECK(error.find(":2:") != std::string::npos);
    }

    SECTION("illegal SAN") {
        const TemporaryDirectory temporary;
        write_file(temporary.path() / "a.tsv",
                   "eco\tname\tpgn\nA00\tBroken Opening\t1. NotAMove\n");
        const std::string error =
            expect_openings_error([&] { (void)Table::load(temporary.path()); });
        CHECK(error.find("openings") != std::string::npos);
        CHECK(error.find((temporary.path() / "a.tsv").string()) != std::string::npos);
        CHECK(error.find(":2:") != std::string::npos);
        CHECK(error.find("NotAMove") != std::string::npos);
    }

    SECTION("missing directory") {
        const TemporaryDirectory temporary;
        const auto missing = temporary.path() / "missing";
        const std::string error = expect_openings_error([&] { (void)Table::load(missing); });
        CHECK(error.find("openings") != std::string::npos);
        CHECK(error.find(missing.string()) != std::string::npos);
    }
}
