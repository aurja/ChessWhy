// FEN, PGN and move input. Frozen by Planning/milestones/M1.md §Input.
#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <chess.hpp>

namespace chesswhy::input {

// User-facing input error. what() is the message printed after "chesswhy: error: ".
class InputError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Six-field FEN (four fields accepted; counters default to "0 1"). Rejects: wrong field
// count, bad characters, rank not summing to 8, not exactly one king per side, pawns on
// rank 1/8, side not to move in check. Castling rights without the king/rook on their
// home squares are rejected. An en-passant square without a legal capture is dropped.
// Counters: half-move clock 0..150 (FIDE 75-move rule), full-move number 1..9999; outside
// that range: "invalid half-move or full-move counter". chess-library stores them in 8/16
// bits, so larger values would wrap.
[[nodiscard]] chess::Board parse_fen(std::string_view fen);

struct PgnPosition {
    chess::Board board;                 // position after `ply` half-moves
    int game = 1;                       // 1-based index in the file
    int ply = 0;                        // half-moves played from the game's start position
    std::vector<chess::Board> mainline; // positions after half-moves 1..ply
};

// Mainline only: comments, variations and NAGs are ignored. Honors [FEN]/[SetUp].
// game is 1-based; ply defaults to the final position. Throws InputError for an
// unreadable file, missing game, ply beyond the mainline, an illegal/unparsable move, or
// a move that takes the half-move clock above 150 (message names the game, move number
// and token).
[[nodiscard]] PgnPosition load_pgn(const std::filesystem::path& path, int game,
                                   std::optional<int> ply);

// One mainline half-move of a game as read from the file.
struct PgnMove {
    int ply = 0;           // half-moves played after this move (1 = first move of the game)
    chess::Move move;      // legal in the position after ply - 1 half-moves
    std::string san;       // chess-library SAN with check suffix, e.g. "Nf3", "Qxf7#"
    std::vector<int> nags; // annotation glyphs on this move in file order: "$n" as n;
                           // suffixes ! ? !! ?? !? ?! as 1 2 3 4 5 6
};

struct PgnGame {
    int game = 1;                                             // 1-based index in the file
    std::vector<std::pair<std::string, std::string>> headers; // tag pairs in file order
    chess::Board start;                                       // [FEN] position or standard start
    std::vector<PgnMove> moves;                               // mainline only
};

// Every game in file order. Mainline only: comments (brace and ";"), variations (nested),
// NAGs and move-number indications ("12.", "12...") never become moves. Comment text is
// not returned. Same InputError rules and messages as load_pgn; load_pgn uses this parser.
[[nodiscard]] std::vector<PgnGame> read_games(const std::filesystem::path& path);

// SAN ("Nf3", "O-O", "exf6", "a8=N", check/mate suffixes optional) or UCI ("g1f3",
// "e1g1", "a7a8n"). Throws InputError for illegal, ambiguous or unparsable text.
[[nodiscard]] chess::Move parse_move(const chess::Board& board, std::string_view text);

} // namespace chesswhy::input
