#include <charconv>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <chesswhy/input.hpp>

namespace chesswhy::input {
namespace {

enum class TokenKind : std::uint8_t { word, nag, tag, result, end };

struct Token {
    TokenKind kind = TokenKind::end;
    std::string text;
    std::string value;
};

bool is_space(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\f' ||
           value == '\v';
}

bool is_horizontal_space(char value) {
    return value == ' ' || value == '\t' || value == '\r';
}

class Lexer {
public:
    explicit Lexer(std::string_view text) : text_(text) {}

    Token next() {
        while (position_ < text_.size()) {
            const char value = peek();
            if (is_space(value)) {
                consume();
                continue;
            }
            if (value == '{') {
                skip_brace_comment();
                continue;
            }
            if (value == ';' || (value == '%' && line_start_)) {
                skip_line();
                continue;
            }
            if (value == '(') {
                consume();
                skip_variation();
                continue;
            }
            if (value == '}') {
                consume();
                return {.kind = TokenKind::word, .text = "}", .value = {}};
            }
            if (value == ')') {
                consume();
                return {.kind = TokenKind::word, .text = ")", .value = {}};
            }
            if (value == '[' && line_start_) {
                return read_tag();
            }
            if (value == '$') {
                return read_nag();
            }
            return read_word();
        }
        return {};
    }

private:
    [[nodiscard]] char peek() const { return text_[position_]; }

    char consume() {
        const char value = text_[position_++];
        if (value == '\n') {
            line_start_ = true;
        } else if (!is_space(value)) {
            line_start_ = false;
        }
        return value;
    }

    void skip_line() {
        while (position_ < text_.size() && peek() != '\n') {
            consume();
        }
        if (position_ < text_.size()) {
            consume();
        }
    }

    void skip_brace_comment() {
        consume();
        while (position_ < text_.size()) {
            if (consume() == '}') {
                return;
            }
        }
    }

    void skip_variation() {
        int depth = 1;
        while (position_ < text_.size() && depth > 0) {
            const char value = peek();
            if (value == '{') {
                skip_brace_comment();
            } else if (value == ';' || (value == '%' && line_start_)) {
                skip_line();
            } else if (value == '(') {
                consume();
                ++depth;
            } else if (value == ')') {
                consume();
                --depth;
            } else {
                consume();
            }
        }
    }

    Token read_tag() {
        const auto start = position_;
        consume(); // [
        while (position_ < text_.size() && is_horizontal_space(peek())) {
            consume();
        }

        std::string key;
        while (position_ < text_.size() && !is_space(peek()) && peek() != '"' && peek() != ']') {
            key.push_back(consume());
        }
        while (position_ < text_.size() && is_horizontal_space(peek())) {
            consume();
        }
        if (key.empty() || position_ == text_.size() || consume() != '"') {
            return {.kind = TokenKind::word,
                    .text = std::string(text_.substr(start, position_ - start)),
                    .value = {}};
        }

        std::string value;
        bool closed_quote = false;
        while (position_ < text_.size()) {
            const char character = consume();
            if (character == '"') {
                closed_quote = true;
                break;
            }
            if (character == '\\' && position_ < text_.size()) {
                const char escaped = peek();
                if (escaped == '"' || escaped == '\\') {
                    value.push_back(consume());
                } else {
                    value.push_back(character);
                }
            } else {
                value.push_back(character);
            }
        }
        while (position_ < text_.size() && is_horizontal_space(peek())) {
            consume();
        }
        if (!closed_quote || position_ == text_.size() || consume() != ']') {
            return {.kind = TokenKind::word,
                    .text = std::string(text_.substr(start, position_ - start)),
                    .value = {}};
        }
        return {.kind = TokenKind::tag, .text = std::move(key), .value = std::move(value)};
    }

    Token read_nag() {
        std::string number;
        consume();
        while (position_ < text_.size() && peek() >= '0' && peek() <= '9') {
            number.push_back(consume());
        }
        if (number.empty()) {
            return {.kind = TokenKind::word, .text = "$", .value = {}};
        }
        return {.kind = TokenKind::nag, .text = std::move(number), .value = {}};
    }

    Token read_word() {
        std::string word;
        while (position_ < text_.size()) {
            const char value = peek();
            if (is_space(value) || value == '{' || value == '}' || value == '(' || value == ')' ||
                value == ';' || value == '$') {
                break;
            }
            word.push_back(consume());
        }
        if (word == "1-0" || word == "0-1" || word == "1/2-1/2" || word == "*") {
            return {.kind = TokenKind::result, .text = std::move(word), .value = {}};
        }
        return {.kind = TokenKind::word, .text = std::move(word), .value = {}};
    }

    std::string_view text_;
    std::size_t position_ = 0;
    bool line_start_ = true;
};

bool strip_move_number(std::string_view& word) {
    std::size_t digits = 0;
    while (digits < word.size() && word[digits] >= '0' && word[digits] <= '9') {
        ++digits;
    }
    if (digits == 0) {
        return false;
    }
    std::size_t dots = digits;
    while (dots < word.size() && word[dots] == '.') {
        ++dots;
    }
    const auto dot_count = dots - digits;
    if (dot_count != 1 && dot_count != 3) {
        return false;
    }
    word.remove_prefix(dots);
    return true;
}

int suffix_nag(std::string_view& san) {
    if (san.size() >= 2) {
        const auto suffix = san.substr(san.size() - 2);
        if (suffix == "!!" || suffix == "??" || suffix == "!?" || suffix == "?!") {
            san.remove_suffix(2);
            if (suffix == "!!") {
                return 3;
            }
            if (suffix == "??") {
                return 4;
            }
            return suffix == "!?" ? 5 : 6;
        }
    }
    if (!san.empty() && (san.back() == '!' || san.back() == '?')) {
        const char suffix = san.back();
        san.remove_suffix(1);
        return suffix == '!' ? 1 : 2;
    }
    return 0;
}

std::string_view header_value(const PgnGame& game, std::string_view key) {
    for (const auto& [header_key, value] : game.headers) {
        if (header_key == key) {
            return value;
        }
    }
    return {};
}

chess::Board game_start(const PgnGame& game, const std::filesystem::path& path) {
    const auto fen = header_value(game, "FEN");
    const auto setup = header_value(game, "SetUp");
    if (setup == "1" && fen.empty()) {
        throw InputError("game " + std::to_string(game.game) +
                         ": invalid FEN: [SetUp] requires a [FEN] tag in " + path.string());
    }
    try {
        return fen.empty() ? chess::Board{} : parse_fen(fen);
    } catch (const InputError& error) {
        throw InputError("game " + std::to_string(game.game) + ": " + error.what() + " in " +
                         path.string());
    }
}

void append_move(PgnGame& game, chess::Board& board, std::string_view token,
                 const std::filesystem::path& path) {
    const int nag = suffix_nag(token);
    const auto move_number = board.fullMoveNumber();
    chess::Move move = chess::Move::NO_MOVE;
    try {
        move = parse_move(board, token);
    } catch (const InputError& error) {
        throw InputError("game " + std::to_string(game.game) + ", move " +
                         std::to_string(move_number) + ": " + error.what() + " in " +
                         path.string());
    }
    PgnMove parsed{.ply = static_cast<int>(game.moves.size()) + 1,
                   .move = move,
                   .san = chess::uci::moveToSan(board, move),
                   .nags = {}};
    if (nag != 0) {
        parsed.nags.push_back(nag);
    }
    board.makeMove(move);
    game.moves.push_back(std::move(parsed));
}

std::vector<PgnGame> parse_games(std::string_view text, const std::filesystem::path& path,
                                 std::optional<int> stop_after = std::nullopt) {
    Lexer lexer(text);
    std::vector<PgnGame> games;
    int game_number = 1;
    Token token = lexer.next();

    while (token.kind != TokenKind::end) {
        PgnGame game;
        game.game = game_number++;
        while (token.kind == TokenKind::tag) {
            game.headers.emplace_back(std::move(token.text), std::move(token.value));
            token = lexer.next();
        }

        auto board = game_start(game, path);
        game.start = board;
        while (token.kind != TokenKind::end && token.kind != TokenKind::tag) {
            if (token.kind == TokenKind::result) {
                token = lexer.next();
                break;
            }
            if (token.kind == TokenKind::nag) {
                if (!game.moves.empty()) {
                    int nag = 0;
                    const auto* first = token.text.data();
                    const auto* last = first + token.text.size();
                    const auto [end, error] = std::from_chars(first, last, nag);
                    if (error == std::errc{} && end == last) {
                        game.moves.back().nags.push_back(nag);
                    }
                }
                token = lexer.next();
                continue;
            }
            if (token.kind == TokenKind::word) {
                const std::string word = std::move(token.text);
                token = lexer.next();
                std::string_view move_token(word);
                if (strip_move_number(move_token) && move_token.empty()) {
                    continue;
                }
                if (move_token.empty()) {
                    continue;
                }
                append_move(game, board, move_token, path);
                continue;
            }
            token = lexer.next();
        }

        if (!stop_after.has_value() || game.game == *stop_after) {
            games.push_back(std::move(game));
            if (stop_after.has_value()) {
                return games;
            }
        }
    }
    return games;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        throw InputError("cannot read PGN file " + path.string());
    }
    std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad()) {
        throw InputError("cannot read PGN file " + path.string());
    }
    return text;
}

} // namespace

std::vector<PgnGame> read_games(const std::filesystem::path& path) {
    return parse_games(read_file(path), path);
}

PgnPosition load_pgn(const std::filesystem::path& path, int game, std::optional<int> ply) {
    if (game < 1) {
        throw InputError("game " + std::to_string(game) + " must be 1-based");
    }
    if (ply.has_value() && *ply < 0) {
        throw InputError("ply " + std::to_string(*ply) + " must not be negative");
    }

    auto games = parse_games(read_file(path), path, game);
    if (games.empty()) {
        throw InputError("game " + std::to_string(game) + " not found in " + path.string());
    }
    const auto& selected = games.back();
    const auto selected_ply = ply.value_or(static_cast<int>(selected.moves.size()));
    if (static_cast<std::size_t>(selected_ply) > selected.moves.size()) {
        throw InputError("ply " + std::to_string(selected_ply) + " is out of range for game " +
                         std::to_string(game) + " in " + path.string());
    }

    PgnPosition result;
    result.game = game;
    result.ply = selected_ply;
    result.board = selected.start;
    result.mainline.reserve(static_cast<std::size_t>(selected_ply));
    for (int index = 0; index < selected_ply; ++index) {
        result.board.makeMove(selected.moves[static_cast<std::size_t>(index)].move);
        result.mainline.push_back(result.board);
    }
    return result;
}

} // namespace chesswhy::input
