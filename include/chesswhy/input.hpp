// FEN, PGN and move input. Frozen by Planning/milestones/M1.md §Input.
#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string_view>
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
[[nodiscard]] chess::Board parse_fen(std::string_view fen);

struct PgnPosition {
    chess::Board board;                 // position after `ply` half-moves
    int game = 1;                       // 1-based index in the file
    int ply = 0;                        // half-moves played from the game's start position
    std::vector<chess::Board> mainline; // positions after half-moves 1..ply
};

// Mainline only: comments, variations and NAGs are ignored. Honors [FEN]/[SetUp].
// game is 1-based; ply defaults to the final position. Throws InputError for an
// unreadable file, missing game, ply beyond the mainline, or an illegal/unparsable move
// (message names the game, move number and token).
[[nodiscard]] PgnPosition load_pgn(const std::filesystem::path& path, int game,
                                   std::optional<int> ply);

// SAN ("Nf3", "O-O", "exf6", "a8=N", check/mate suffixes optional) or UCI ("g1f3",
// "e1g1", "a7a8n"). Throws InputError for illegal, ambiguous or unparsable text.
[[nodiscard]] chess::Move parse_move(const chess::Board& board, std::string_view text);

} // namespace chesswhy::input
