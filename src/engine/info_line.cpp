#include "chesswhy/engine.hpp"

#include <charconv>
#include <cstddef>
#include <string_view>

namespace chesswhy::engine {
namespace {

std::string_view next_token(std::string_view text, std::size_t& offset) {
    while (offset < text.size() && (text[offset] == ' ' || text[offset] == '\t' ||
                                    text[offset] == '\r' || text[offset] == '\n')) {
        ++offset;
    }
    const auto start = offset;
    while (offset < text.size() && text[offset] != ' ' && text[offset] != '\t' &&
           text[offset] != '\r' && text[offset] != '\n') {
        ++offset;
    }
    return text.substr(start, offset - start);
}

template <typename Integer>
bool parse_integer(std::string_view token, Integer& value) {
    if (token.empty()) {
        return false;
    }
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    return result.ec == std::errc{} && result.ptr == token.data() + token.size();
}

bool is_bound(std::string_view token) {
    return token == "lowerbound" || token == "upperbound";
}

} // namespace

std::optional<Line> parse_info_line(std::string_view text) {
    std::size_t offset = 0;
    if (next_token(text, offset) != "info") {
        return std::nullopt;
    }

    Line line;
    bool have_depth = false;
    bool have_score = false;
    bool have_wdl = false;
    bool have_pv = false;
    bool have_move = false;

    while (offset < text.size()) {
        const auto token = next_token(text, offset);
        if (token.empty()) {
            break;
        }
        if (is_bound(token)) {
            return std::nullopt;
        }
        if (token == "pv") {
            have_pv = true;
            while (offset < text.size()) {
                const auto move = next_token(text, offset);
                if (move.empty()) {
                    continue;
                }
                if (is_bound(move)) {
                    return std::nullopt;
                }
                line.pv_uci.emplace_back(move);
                have_move = true;
            }
            break;
        }
        if (token == "depth") {
            const auto value = next_token(text, offset);
            if (!parse_integer(value, line.depth)) {
                return std::nullopt;
            }
            have_depth = true;
            continue;
        }
        if (token == "multipv") {
            if (!parse_integer(next_token(text, offset), line.multipv) || line.multipv < 1) {
                return std::nullopt;
            }
            continue;
        }
        if (token == "score") {
            const auto type = next_token(text, offset);
            const auto value = next_token(text, offset);
            int score = 0;
            if (!parse_integer(value, score)) {
                return std::nullopt;
            }
            if (type == "cp") {
                line.cp = score;
                line.mate.reset();
            } else if (type == "mate") {
                line.mate = score;
                line.cp.reset();
            } else {
                return std::nullopt;
            }
            have_score = true;
            continue;
        }
        if (token == "wdl") {
            if (!parse_integer(next_token(text, offset), line.win) ||
                !parse_integer(next_token(text, offset), line.draw) ||
                !parse_integer(next_token(text, offset), line.loss)) {
                return std::nullopt;
            }
            have_wdl = true;
            continue;
        }
        if (token == "string") {
            return std::nullopt;
        }
        if (token == "currmove") {
            if (next_token(text, offset).empty()) {
                return std::nullopt;
            }
            continue;
        }
        if (token == "seldepth" || token == "time" || token == "nodes" || token == "nps" ||
            token == "hashfull" || token == "tbhits" || token == "cpuload" ||
            token == "currmovenumber") {
            std::uint64_t ignored = 0;
            if (!parse_integer(next_token(text, offset), ignored)) {
                return std::nullopt;
            }
        }
    }

    if (!have_depth || !have_score || !have_wdl || !have_pv || !have_move) {
        return std::nullopt;
    }
    return line;
}

} // namespace chesswhy::engine
