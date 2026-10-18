#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <chesswhy/input.hpp>
#include <nlohmann/json.hpp>

namespace {

using Json = nlohmann::json;

std::filesystem::path fixture_path(const std::string& name) {
    return std::filesystem::path(CHESSWHY_SOURCE_DIR) / "tests" / "fixtures" / name;
}

Json fixture_data() {
    const auto path = fixture_path("positions.json");
    std::ifstream input(path);
    REQUIRE(input.good());
    Json data;
    input >> data;
    return data;
}

int integer_argument(const Json& args, const std::string& option, int fallback) {
    for (std::size_t index = 0; index + 1 < args.size(); ++index) {
        if (args[index] == option) {
            return std::stoi(args[index + 1].get<std::string>());
        }
    }
    return fallback;
}

std::optional<std::string> string_argument(const Json& args, const std::string& option) {
    for (std::size_t index = 0; index + 1 < args.size(); ++index) {
        if (args[index] == option) {
            return args[index + 1].get<std::string>();
        }
    }
    return std::nullopt;
}

} // namespace

TEST_CASE("all move parsing fixtures resolve to their expected UCI moves", "[input]") {
    const auto fixtures = fixture_data().at("parse_fixtures");
    REQUIRE(fixtures.size() == 13);
    for (const auto& fixture : fixtures) {
        const auto board = chesswhy::input::parse_fen(fixture.at("fen").get<std::string>());
        const auto move = chesswhy::input::parse_move(board, fixture.at("move").get<std::string>());
        CHECK(chess::uci::moveToUci(move) == fixture.at("uci").get<std::string>());
    }
}

TEST_CASE("PGN unit fixtures preserve mainline positions", "[input]") {
    const auto fixtures = fixture_data().at("unit_fixtures");
    for (const auto& fixture : fixtures) {
        const auto id = fixture.at("id").get<std::string>();
        if (id < "U01" || id > "U04") {
            continue;
        }
        const auto game = fixture.at("game").get<int>();
        const auto ply = fixture.at("ply").get<int>();
        const auto path = fixture_path(fixture.at("pgn").get<std::string>());
        const auto expected_fen = fixture.at("fen").get<std::string>();

        const auto explicit_position = chesswhy::input::load_pgn(path, game, ply);
        CHECK(explicit_position.game == game);
        CHECK(explicit_position.ply == ply);
        CHECK(explicit_position.board.getFen() == expected_fen);
        REQUIRE(explicit_position.mainline.size() == static_cast<std::size_t>(ply));
        if (ply > 0) {
            CHECK(explicit_position.mainline.back().getFen() == explicit_position.board.getFen());
        }

        if (id == "U03") {
            const auto default_position = chesswhy::input::load_pgn(path, game, std::nullopt);
            CHECK(default_position.ply == ply);
            CHECK(default_position.board.getFen() == expected_fen);
            REQUIRE(default_position.mainline.size() == static_cast<std::size_t>(ply));
            CHECK(default_position.mainline.back().getFen() == default_position.board.getFen());
        }
    }
}

TEST_CASE("four- and six-field FEN fixture normalization", "[input]") {
    const auto fixtures = fixture_data().at("unit_fixtures");
    for (const auto& fixture : fixtures) {
        const auto id = fixture.at("id").get<std::string>();
        if (id != "U05" && id != "U06") {
            continue;
        }
        const auto board = chesswhy::input::parse_fen(fixture.at("fen_input").get<std::string>());
        CHECK(board.getFen() == fixture.at("fen").get<std::string>());
    }
    const auto four_fields =
        chesswhy::input::parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -");
    CHECK(four_fields.getFen() == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

TEST_CASE("input error fixtures report the required function-level fragments", "[input]") {
    const auto fixtures = fixture_data().at("error_fixtures");
    for (const auto& fixture : fixtures) {
        const auto id = fixture.at("id").get<std::string>();
        if (id < "E01" || id > "E11") {
            continue;
        }
        const auto& args = fixture.at("args");
        std::string message;
        try {
            if (const auto fen = string_argument(args, "--fen")) {
                if (const auto token = string_argument(args, "--move")) {
                    const auto board = chesswhy::input::parse_fen(*fen);
                    static_cast<void>(chesswhy::input::parse_move(board, *token));
                } else {
                    static_cast<void>(chesswhy::input::parse_fen(*fen));
                }
            } else if (const auto pgn = string_argument(args, "--pgn")) {
                const auto game = integer_argument(args, "--game", 1);
                std::optional<int> ply;
                for (std::size_t index = 0; index + 1 < args.size(); ++index) {
                    if (args[index] == "--ply") {
                        ply = std::stoi(args[index + 1].get<std::string>());
                    }
                }
                static_cast<void>(chesswhy::input::load_pgn(fixture_path(*pgn), game, ply));
            } else {
                FAIL("unexpected fixture arguments for " + id);
            }
        } catch (const chesswhy::input::InputError& error) {
            message = error.what();
        }
        REQUIRE(!message.empty());
        for (const auto& fragment : fixture.at("contains")) {
            CHECK(message.find(fragment.get<std::string>()) != std::string::npos);
        }
    }
}

TEST_CASE("FEN rejects invalid king, pawn, and castling boundaries", "[input]") {
    const auto invalid_fens = {
        "P3k3/8/8/8/8/8/8/4K3 w - - 0 1",
        "4k3/8/8/8/8/8/8/3KK3 w - - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w K - 0 1",
    };
    for (const auto* const fen : invalid_fens) {
        std::string message;
        try {
            static_cast<void>(chesswhy::input::parse_fen(fen));
            FAIL("invalid FEN was accepted");
        } catch (const chesswhy::input::InputError& error) {
            message = error.what();
        }
        CHECK(message.find("invalid FEN") != std::string::npos);
        CHECK(message.find(fen) != std::string::npos);
    }
}

TEST_CASE("negative PGN ply is rejected", "[input]") {
    try {
        static_cast<void>(chesswhy::input::load_pgn(fixture_path("two_games.pgn"), 1, -1));
        FAIL("negative ply was accepted");
    } catch (const chesswhy::input::InputError& error) {
        CHECK(std::string(error.what()).find("ply -1") != std::string::npos);
    }
}

TEST_CASE("annotated PGN reader preserves mainline SAN, NAGs, and headers", "[input]") {
    const auto games = chesswhy::input::read_games(fixture_path("annotated.pgn"));
    REQUIRE(games.size() == 5);
    CHECK(games[0].moves.size() == 10);
    CHECK(games[1].moves.size() == 2);
    CHECK(games[2].moves.size() == 1);
    CHECK(games[3].moves.size() == 1);
    CHECK(games[4].moves.size() == 1);

    const std::vector<std::string> expected_san = {"e4", "e5",  "Nf3", "Nc6", "Bb5",
                                                   "a6", "Ba4", "Nf6", "O-O", "Be7"};
    for (std::size_t index = 0; index < expected_san.size(); ++index) {
        CHECK(games[0].moves[index].san == expected_san[index]);
    }
    CHECK((games[0].moves[0].nags == std::vector<int>{1}));
    CHECK((games[0].moves[1].nags == std::vector<int>{1}));
    CHECK((games[0].moves[2].nags == std::vector<int>{2}));
    CHECK((games[0].moves[3].nags == std::vector<int>{3}));
    CHECK((games[0].moves[4].nags == std::vector<int>{4, 7}));
    CHECK((games[0].moves[5].nags == std::vector<int>{5}));
    CHECK((games[0].moves[6].nags == std::vector<int>{6}));
    CHECK((games[0].moves[7].nags == std::vector<int>{19}));
    CHECK((games[0].moves[8].nags == std::vector<int>{4}));
    CHECK((games[0].moves[9].nags == std::vector<int>{18}));

    REQUIRE(games[0].headers.size() == 8);
    const std::vector<std::string> expected_headers = {"Event",     "White", "Black", "Result",
                                                       "Annotator", "Site",  "Date",  "PlyCount"};
    for (std::size_t index = 0; index < expected_headers.size(); ++index) {
        CHECK(games[0].headers[index].first == expected_headers[index]);
    }
    CHECK((games[0].headers[0] == std::pair<std::string, std::string>{"Event", "Annotated One"}));
    CHECK((games[0].headers[4] ==
           std::pair<std::string, std::string>{"Annotator", "A \"quote\" and \\ slash"}));
    CHECK(games[0].moves[0].ply == 1);
    CHECK(games[0].moves[9].ply == 10);
    CHECK(games[1].start.sideToMove() == chess::Color::BLACK);
    CHECK(games[1].moves[0].san == "Kf7");

    CHECK(games[1].start.getFen() == "4k3/8/8/8/8/8/4K3/8 b - - 0 1");
    CHECK(games[1].moves[0].ply == 1);

    CHECK(games[2].moves[0].san == "exd6");
    CHECK(chess::uci::moveToUci(games[2].moves[0].move) == "e5d6");
    CHECK(games[3].moves[0].san == "a8=Q+");

    CHECK(games[4].start.sideToMove() == chess::Color::BLACK);
    CHECK(games[4].start.fullMoveNumber() == 12);
    CHECK(games[4].moves[0].san == "Kf7");
    CHECK(games[4].moves[0].ply == 1);
}

TEST_CASE("load_pgn selects requested annotated mainline positions", "[input]") {
    const auto path = fixture_path("annotated.pgn");
    const auto first = chesswhy::input::load_pgn(path, 1, 5);
    CHECK(first.ply == 5);
    CHECK(first.mainline.size() == 5);
    CHECK(first.board.getFen() ==
          "r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3");

    const auto black_start = chesswhy::input::load_pgn(path, 2, 0);
    CHECK(black_start.board.sideToMove() == chess::Color::BLACK);
    CHECK(black_start.ply == 0);
    const auto en_passant = chesswhy::input::load_pgn(path, 3, 1);
    CHECK(en_passant.mainline.size() == 1);
    const auto promotion = chesswhy::input::load_pgn(path, 4, std::nullopt);
    CHECK(promotion.ply == 1);
    CHECK(promotion.board.at(chess::Square("a8")) ==
          chess::Piece(chess::PieceType::QUEEN, chess::Color::WHITE));
    const auto numbered = chesswhy::input::load_pgn(path, 5, 1);
    CHECK(numbered.ply == 1);
    CHECK(numbered.board.fullMoveNumber() == 13);
}

TEST_CASE("load_pgn stops before errors in later games", "[input]") {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      ("chesswhy-later-error-" + std::to_string(stamp) + ".pgn");
    {
        std::ofstream output(path);
        REQUIRE(output.good());
        output << "[Event \"Good\"]\n1. e4 e5 *\n\n[Event \"Bad\"]\n1. Ke3 *\n";
    }

    const auto selected = chesswhy::input::load_pgn(path, 1, std::nullopt);
    CHECK(selected.ply == 2);
    CHECK(selected.board.fullMoveNumber() == 2);
    CHECK_THROWS_AS(chesswhy::input::read_games(path), chesswhy::input::InputError);
    std::filesystem::remove(path);
}
