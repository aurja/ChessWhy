#include <string>
#include <string_view>

#include <chesswhy/input.hpp>

namespace chesswhy::input {
namespace {

bool is_file(char c) {
    return c >= 'a' && c <= 'h';
}
bool is_rank(char c) {
    return c >= '1' && c <= '8';
}
bool is_promotion(char c) {
    return c == 'N' || c == 'B' || c == 'R' || c == 'Q';
}

bool is_san(std::string_view text) {
    auto core = text;
    if (!core.empty() && (core.back() == '+' || core.back() == '#')) {
        core.remove_suffix(1);
    }
    if (core == "O-O" || core == "O-O-O" || core == "0-0" || core == "0-0-0") {
        return true;
    }
    if (core.empty()) {
        return false;
    }

    bool piece_move = false;
    switch (core.front()) {
    case 'K':
    case 'Q':
    case 'R':
    case 'B':
    case 'N':
        piece_move = true;
        core.remove_prefix(1);
        break;
    default:
        break;
    }

    bool promotion = false;
    const auto equals = core.find('=');
    if (equals != std::string_view::npos) {
        if (piece_move || equals + 2 != core.size() || !is_promotion(core.back())) {
            return false;
        }
        promotion = true;
        core.remove_suffix(2);
    }

    if (core.size() < 2 || !is_file(core[core.size() - 2]) || !is_rank(core.back())) {
        return false;
    }
    const auto prefix = core.substr(0, core.size() - 2);
    bool capture = false;
    auto disambiguation = prefix;
    if (!disambiguation.empty() && disambiguation.back() == 'x') {
        capture = true;
        disambiguation.remove_suffix(1);
    }
    if (disambiguation.find('x') != std::string_view::npos) {
        return false;
    }

    if (piece_move) {
        const auto size = disambiguation.size();
        return size == 0 ||
               (size == 1 && (is_file(disambiguation[0]) || is_rank(disambiguation[0]))) ||
               (size == 2 && is_file(disambiguation[0]) && is_rank(disambiguation[1]));
    }
    if (promotion && core.back() != '8' && core.back() != '1') {
        return false;
    }
    if (capture) {
        return disambiguation.size() == 1 && is_file(disambiguation[0]);
    }
    return disambiguation.empty();
}

std::string move_error(std::string_view category, std::string_view token) {
    return std::string(category) + " " + std::string(token);
}

} // namespace

chess::Move parse_move(const chess::Board& board, std::string_view text) {
    if (chess::uci::isUciMove(text)) {
        const auto move = chess::uci::uciToMove(board, text);
        if (move == chess::Move::NO_MOVE) {
            throw InputError(move_error("illegal move", text));
        }
        chess::Movelist legal_moves;
        chess::movegen::legalmoves(legal_moves, board);
        for (const auto& legal : legal_moves) {
            if (legal == move) {
                return move;
            }
        }
        throw InputError(move_error("illegal move", text));
    }

    auto san = text;
    if (!is_san(text)) {
        throw InputError(move_error("cannot parse move", text));
    }
    if (!text.empty() && (text.back() == '+' || text.back() == '#')) {
        san.remove_suffix(1);
    }

    try {
        const auto move = chess::uci::parseSan(board, san);
        if (move == chess::Move::NO_MOVE) {
            throw InputError(move_error("illegal move", text));
        }
        return move;
    } catch (const chess::uci::AmbiguousMoveError&) {
        throw InputError(move_error("ambiguous move", text));
    } catch (const chess::uci::SanParseError&) {
        throw InputError(move_error("illegal move", text));
    }
}

} // namespace chesswhy::input
