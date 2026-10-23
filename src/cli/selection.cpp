#include "selection.hpp"

#include "chesswhy/input.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <string>
#include <system_error>
#include <utility>

#include <chess.hpp>

namespace chesswhy::cli::detail {
namespace {

using Keyword = Selection::Keyword;

constexpr std::array<std::pair<std::string_view, Keyword>, 7> kKeywords{{
    {"best", Keyword::Best},
    {"playable", Keyword::Playable},
    {"inaccuracies", Keyword::Inaccuracies},
    {"mistakes", Keyword::Mistakes},
    {"blunders", Keyword::Blunders},
    {"unplayable", Keyword::Unplayable},
    {"all", Keyword::All},
}};

[[nodiscard]] bool parse_top(std::string_view item, int& value) {
    if (!item.starts_with("top")) {
        return false;
    }

    const std::string_view digits = item.substr(3);
    if (digits.empty()) {
        throw input::InputError("invalid value for --moves: " + std::string(item));
    }
    for (const char digit : digits) {
        if (digit < '0' || digit > '9') {
            throw input::InputError("invalid value for --moves: " + std::string(item));
        }
    }

    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size() || value < 1) {
        throw input::InputError("invalid value for --moves: " + std::string(item));
    }
    return true;
}

} // namespace

ResolvedMoves resolve_moves(const chess::Board& board, std::optional<std::string_view> moves) {
    ResolvedMoves result;
    if (!moves) {
        result.selection.keywords.push_back(Keyword::Best);
        return result;
    }

    std::array<bool, kKeywords.size()> selected_keywords{};
    std::size_t start = 0;
    while (start <= moves->size()) {
        const std::size_t comma = moves->find(',', start);
        const std::size_t end = comma == std::string_view::npos ? moves->size() : comma;
        const std::string_view item = moves->substr(start, end - start);
        if (item.empty()) {
            throw input::InputError("empty item in --moves");
        }

        bool is_keyword = false;
        for (std::size_t index = 0; index < kKeywords.size(); ++index) {
            if (item == kKeywords[index].first) {
                selected_keywords[index] = true;
                is_keyword = true;
                break;
            }
        }
        if (!is_keyword) {
            int top = 0;
            if (parse_top(item, top)) {
                if (!result.selection.top || top > *result.selection.top) {
                    result.selection.top = top;
                }
            } else {
                const chess::Move move = input::parse_move(board, item);
                if (std::find(result.asked.begin(), result.asked.end(), move) ==
                    result.asked.end()) {
                    result.selection.moves.push_back(chess::uci::moveToUci(move, board.chess960()));
                    result.asked.push_back(move);
                }
            }
        }

        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }

    for (std::size_t index = 0; index < kKeywords.size(); ++index) {
        if (selected_keywords[index]) {
            result.selection.keywords.push_back(kKeywords[index].second);
        }
    }
    return result;
}

} // namespace chesswhy::cli::detail
