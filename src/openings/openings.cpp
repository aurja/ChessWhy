#include "chesswhy/openings.hpp"

#include "chesswhy/digest.hpp"

#include <array>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace chesswhy::openings {
namespace {

[[nodiscard]] std::string row_error(const std::filesystem::path& path, std::size_t line,
                                    std::string_view message) {
    return "openings: " + path.string() + ":" + std::to_string(line) + ": " + std::string(message);
}

[[nodiscard]] std::string_view remove_move_number(std::string_view token) {
    std::size_t digits = 0;
    while (digits < token.size() && token[digits] >= '0' && token[digits] <= '9') {
        ++digits;
    }
    if (digits == 0 || digits == token.size() || token[digits] != '.') {
        return token;
    }

    std::size_t san_start = digits;
    while (san_start < token.size() && token[san_start] == '.') {
        ++san_start;
    }
    return token.substr(san_start);
}

[[nodiscard]] chess::Board replay_pgn(std::string_view pgn, const std::filesystem::path& path,
                                      std::size_t line_number) {
    chess::Board board;
    std::size_t start = 0;
    while (start < pgn.size()) {
        while (start < pgn.size() && std::isspace(static_cast<unsigned char>(pgn[start])) != 0) {
            ++start;
        }
        if (start == pgn.size()) {
            break;
        }

        std::size_t end = start;
        while (end < pgn.size() && std::isspace(static_cast<unsigned char>(pgn[end])) == 0) {
            ++end;
        }
        const std::string_view token = remove_move_number(pgn.substr(start, end - start));
        start = end;
        if (token.empty()) {
            continue;
        }

        try {
            const chess::Move move = chess::uci::parseSan(board, token);
            if (move == chess::Move::NO_MOVE) {
                throw std::runtime_error("SAN parser returned no move");
            }
            board.makeMove(move);
        } catch (const std::exception& error) {
            throw OpeningsError(row_error(path, line_number,
                                          "illegal or unparsable SAN '" + std::string(token) +
                                              "': " + error.what()));
        }
    }
    return board;
}

} // namespace

std::string position_key(const chess::Board& board) {
    std::string key = board.getFen(false);
    if (board.enpassantSq() == chess::Square::NO_SQ) {
        return key;
    }

    chess::Movelist captures;
    chess::movegen::legalmoves<chess::movegen::MoveGenType::CAPTURE>(captures, board);
    for (const chess::Move move : captures) {
        if (move.typeOf() == chess::Move::ENPASSANT) {
            return key;
        }
    }

    key.replace(key.rfind(' ') + 1, std::string::npos, "-");
    return key;
}

Table Table::load(const std::filesystem::path& dir) {
    std::error_code directory_error;
    if (!std::filesystem::is_directory(dir, directory_error) || directory_error) {
        std::string message = "openings: unavailable directory " + dir.string();
        if (directory_error) {
            message += ": " + directory_error.message();
        }
        throw OpeningsError(message);
    }

    Table table;
    std::string loaded_bytes;
    constexpr std::array<std::string_view, 5> filenames{"a.tsv", "b.tsv", "c.tsv", "d.tsv",
                                                        "e.tsv"};
    bool all_pinned = true;
    for (std::size_t file_index = 0; file_index < filenames.size(); ++file_index) {
        const std::string_view filename = filenames[file_index];
        const std::filesystem::path path = dir / filename;
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open()) {
            throw OpeningsError("openings: unable to read file " + path.string());
        }

        const std::size_t file_start = loaded_bytes.size();
        loaded_bytes.append(std::istreambuf_iterator<char>(input),
                            std::istreambuf_iterator<char>());
        if (input.bad()) {
            throw OpeningsError("openings: unable to read file " + path.string());
        }

        const std::string_view contents = std::string_view(loaded_bytes).substr(file_start);
        const std::string file_digest =
            sha256_hex(std::as_bytes(std::span(contents.data(), contents.size())));
        if (file_digest != kDatasetSha256[file_index]) {
            all_pinned = false;
        }

        std::size_t line_start = 0;
        std::size_t line_number = 0;
        while (line_start < contents.size()) {
            const std::size_t newline = contents.find('\n', line_start);
            const std::size_t line_end =
                newline == std::string_view::npos ? contents.size() : newline;
            std::string_view line = contents.substr(line_start, line_end - line_start);
            ++line_number;
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            if (line_number == 1) {
                if (line != "eco\tname\tpgn") {
                    throw OpeningsError(row_error(path, line_number, "bad header"));
                }
            } else {
                const std::size_t first_tab = line.find('\t');
                const std::size_t second_tab = first_tab == std::string_view::npos
                                                   ? std::string_view::npos
                                                   : line.find('\t', first_tab + 1);
                if (first_tab == std::string_view::npos || second_tab == std::string_view::npos ||
                    line.find('\t', second_tab + 1) != std::string_view::npos) {
                    throw OpeningsError(row_error(
                        path, line_number, "malformed row: expected three tab-separated fields"));
                }

                const std::string_view eco = line.substr(0, first_tab);
                const std::string_view name =
                    line.substr(first_tab + 1, second_tab - first_tab - 1);
                const std::string_view pgn = line.substr(second_tab + 1);
                if (eco.empty() || name.empty() || pgn.empty()) {
                    throw OpeningsError(
                        row_error(path, line_number, "malformed row: fields must be non-empty"));
                }

                const chess::Board board = replay_pgn(pgn, path, line_number);
                std::string key = position_key(board);
                const OpeningName opening{.eco = std::string(eco), .name = std::string(name)};
                const auto existing = table.by_key_.find(key);
                if (existing != table.by_key_.end()) {
                    throw OpeningsError(row_error(path, line_number,
                                                  "duplicate position key for '" +
                                                      existing->second.name + "' and '" +
                                                      opening.name + "'"));
                }
                table.by_key_.emplace(std::move(key), opening);
            }
            if (newline == std::string_view::npos) {
                break;
            }
            line_start = newline + 1;
        }
        if (line_number == 0) {
            throw OpeningsError(row_error(path, 1, "missing header"));
        }
    }

    if (!all_pinned) {
        const std::string digest =
            sha256_hex(std::as_bytes(std::span(loaded_bytes.data(), loaded_bytes.size())));
        table.dataset_ = "custom@" + digest.substr(0, 12);
    }
    return table;
}

std::optional<OpeningName> Table::find(const chess::Board& board) const {
    const auto found = by_key_.find(position_key(board));
    if (found == by_key_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::size_t Table::size() const {
    return by_key_.size();
}
const std::string& Table::dataset() const {
    return dataset_;
}

OpeningInfo describe(const Table& table, const chess::Board& board,
                     std::span<const chess::Board> mainline) {
    OpeningInfo info;
    info.dataset = table.dataset();
    info.current = table.find(board);

    for (std::size_t index = mainline.size(); index > 0; --index) {
        if (auto opening = table.find(mainline[index - 1])) {
            info.last_named =
                NamedPly{.opening = std::move(*opening), .ply = static_cast<int>(index)};
            break;
        }
    }
    return info;
}

} // namespace chesswhy::openings
