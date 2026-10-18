#include <array>
#include <cctype>
#include <charconv>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <chesswhy/input.hpp>

namespace chesswhy::input {
namespace {

[[noreturn]] void invalid_fen(std::string_view fen, std::string_view reason) {
    throw InputError("invalid FEN '" + std::string(fen) + "': " + std::string(reason));
}

std::vector<std::string_view> split_fields(std::string_view fen) {
    std::vector<std::string_view> fields;
    std::size_t index = 0;
    while (index < fen.size()) {
        while (index < fen.size() && std::isspace(static_cast<unsigned char>(fen[index])) != 0) {
            ++index;
        }
        if (index == fen.size()) {
            break;
        }
        const auto start = index;
        while (index < fen.size() && std::isspace(static_cast<unsigned char>(fen[index])) == 0) {
            ++index;
        }
        fields.emplace_back(fen.substr(start, index - start));
    }
    return fields;
}

bool is_piece(char c) {
    switch (c) {
    case 'K':
    case 'Q':
    case 'R':
    case 'B':
    case 'N':
    case 'P':
    case 'k':
    case 'q':
    case 'r':
    case 'b':
    case 'n':
    case 'p':
        return true;
    default:
        return false;
    }
}

bool is_pawn(char c) {
    return c == 'P' || c == 'p';
}

bool parse_counter(std::string_view text, int minimum, int maximum) {
    if (text.empty()) {
        return false;
    }
    int value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
           value >= minimum && value <= maximum;
}

} // namespace

chess::Board parse_fen(std::string_view fen) {
    const auto fields = split_fields(fen);
    if (fields.size() != 4 && fields.size() != 6) {
        invalid_fen(fen, "expected four or six fields");
    }

    std::array<char, 64> squares{};
    std::array<int, 2> king_counts{};
    int rank = 7;
    int file = 0;
    int slash_count = 0;
    for (const char c : fields[0]) {
        if (c == '/') {
            if (file != 8 || rank == 0) {
                invalid_fen(fen, "invalid piece placement ranks");
            }
            --rank;
            file = 0;
            ++slash_count;
            continue;
        }
        if (c >= '1' && c <= '8') {
            file += c - '0';
            if (file > 8) {
                invalid_fen(fen, "rank exceeds eight squares");
            }
            continue;
        }
        if (!is_piece(c)) {
            invalid_fen(fen, "invalid piece placement character");
        }
        if (file >= 8) {
            invalid_fen(fen, "rank exceeds eight squares");
        }
        if (is_pawn(c) && (rank == 0 || rank == 7)) {
            invalid_fen(fen, "pawns may not be on rank 1 or 8");
        }
        squares[(static_cast<std::size_t>(rank) * 8U) + static_cast<std::size_t>(file)] = c;
        if (c == 'K') {
            ++king_counts[0];
        } else if (c == 'k') {
            ++king_counts[1];
        }
        ++file;
    }
    if (rank != 0 || file != 8 || slash_count != 7) {
        invalid_fen(fen, "piece placement must contain eight complete ranks");
    }
    if (king_counts[0] != 1 || king_counts[1] != 1) {
        invalid_fen(fen, "position must contain exactly one king per side");
    }

    if (fields[1] != "w" && fields[1] != "b") {
        invalid_fen(fen, "side to move must be w or b");
    }

    if (fields[2] != "-") {
        std::array<bool, 4> seen{};
        for (const char c : fields[2]) {
            std::size_t right = 0;
            int king_square = 0;
            int rook_square = 0;
            char king_piece = 'K';
            char rook_piece = 'R';
            switch (c) {
            case 'K':
                right = 0;
                king_square = 4;
                rook_square = 7;
                break;
            case 'Q':
                right = 1;
                king_square = 4;
                rook_square = 0;
                break;
            case 'k':
                right = 2;
                king_square = 60;
                rook_square = 63;
                king_piece = 'k';
                rook_piece = 'r';
                break;
            case 'q':
                right = 3;
                king_square = 60;
                rook_square = 56;
                king_piece = 'k';
                rook_piece = 'r';
                break;
            default:
                invalid_fen(fen, "invalid castling-right character");
            }
            if (seen[right]) {
                invalid_fen(fen, "duplicate castling right");
            }
            seen[right] = true;
            if (squares[static_cast<std::size_t>(king_square)] != king_piece ||
                squares[static_cast<std::size_t>(rook_square)] != rook_piece) {
                invalid_fen(fen, "castling right requires the king and rook on their home squares");
            }
        }
    }

    if (fields[3] != "-") {
        const auto ep = fields[3];
        if (ep.size() != 2 || ep[0] < 'a' || ep[0] > 'h' || (ep[1] != '3' && ep[1] != '6')) {
            invalid_fen(fen, "invalid en-passant square");
        }
        const char expected_rank = fields[1] == "w" ? '6' : '3';
        if (ep[1] != expected_rank) {
            invalid_fen(fen, "en-passant square is on the wrong rank for the side to move");
        }
    }

    const auto half_move = fields.size() == 6 ? fields[4] : std::string_view{"0"};
    const auto full_move = fields.size() == 6 ? fields[5] : std::string_view{"1"};
    if (!parse_counter(half_move, 0, 150) || !parse_counter(full_move, 1, 9999)) {
        invalid_fen(fen, "invalid half-move or full-move counter");
    }

    std::string normalized;
    normalized.reserve(fen.size() + (fields.size() == 4 ? 4 : 0));
    for (const auto field : fields) {
        if (!normalized.empty()) {
            normalized.push_back(' ');
        }
        normalized.append(field);
    }
    if (fields.size() == 4) {
        normalized += " 0 1";
    }

    chess::Board board;
    if (!board.setFen(normalized)) {
        invalid_fen(fen, "chess library rejected the position");
    }
    const auto side_to_move = board.sideToMove();
    if (board.isAttacked(board.kingSq(~side_to_move), side_to_move)) {
        invalid_fen(fen, "side not to move is in check");
    }
    return board;
}

} // namespace chesswhy::input
