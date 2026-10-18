#include "chesswhy/engine.hpp"

#include <charconv>
#include <cstddef>
#include <string_view>
#include <utility>

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

struct PvInfo {
    int multipv = 1;
    std::string_view first_move;
    bool bound = false;
};

std::optional<PvInfo> info_pv(std::string_view text) {
    std::size_t offset = 0;
    if (next_token(text, offset) != "info") {
        return std::nullopt;
    }

    PvInfo info;
    while (offset < text.size()) {
        const auto token = next_token(text, offset);
        if (token.empty() || token == "string") {
            return std::nullopt;
        }
        if (is_bound(token)) {
            info.bound = true;
        } else if (token == "multipv") {
            if (!parse_integer(next_token(text, offset), info.multipv) || info.multipv < 1) {
                return std::nullopt;
            }
        } else if (token == "pv") {
            info.first_move = next_token(text, offset);
            if (info.first_move.empty()) {
                return std::nullopt;
            }
            while (offset < text.size()) {
                if (is_bound(next_token(text, offset))) {
                    info.bound = true;
                }
            }
            return info;
        }
    }
    return std::nullopt;
}

bool already_selected(const std::vector<Line*>& selected, std::string_view move) {
    for (const auto* line : selected) {
        if (line->pv_uci.front() == move) {
            return true;
        }
    }
    return false;
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

void MultiPvCollector::add(std::string_view text) {
    auto info = info_pv(text);
    auto exact = parse_info_line(text);
    const bool is_exact = exact.has_value();
    if (exact) {
        exact_lines_.push_back(std::move(*exact));
    }
    if (!info || (!is_exact && !info->bound)) {
        return;
    }

    if (info->multipv == 1) {
        last_batch_.clear();
        has_last_batch_ = true;
    }
    if (!has_last_batch_) {
        return;
    }

    auto slot = last_batch_.begin();
    while (slot != last_batch_.end() && slot->first < info->multipv) {
        ++slot;
    }
    if (slot != last_batch_.end() && slot->first == info->multipv) {
        slot->second.assign(info->first_move);
    } else {
        last_batch_.emplace(slot, info->multipv, info->first_move);
    }
}

std::vector<Line> MultiPvCollector::finish() && {
    if (exact_lines_.empty()) {
        throw EngineError("engine search returned no complete info lines");
    }

    std::vector<Line*> selected;
    selected.reserve(last_batch_.size());
    for (const auto& [multipv, move] : last_batch_) {
        if (already_selected(selected, move)) {
            continue;
        }

        Line* choice = nullptr;
        for (auto line = exact_lines_.rbegin(); line != exact_lines_.rend(); ++line) {
            if (line->pv_uci.front() == move) {
                choice = &*line;
                break;
            }
        }
        if (choice == nullptr) {
            for (auto line = exact_lines_.rbegin(); line != exact_lines_.rend(); ++line) {
                if (line->multipv == multipv) {
                    choice = &*line;
                    break;
                }
            }
            if (choice != nullptr && already_selected(selected, choice->pv_uci.front())) {
                choice = nullptr;
            }
        }
        if (choice != nullptr) {
            selected.push_back(choice);
        }
    }

    std::vector<Line> lines;
    lines.reserve(selected.size());
    for (auto* line : selected) {
        lines.push_back(std::move(*line));
        lines.back().multipv = static_cast<int>(lines.size());
    }
    return lines;
}

} // namespace chesswhy::engine
