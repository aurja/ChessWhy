#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <istream>
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
    explicit Lexer(std::istream& input) : input_(input) {}

    Token next() {
        while (has_character()) {
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
    bool has_character() {
        if (buffer_position_ < buffer_size_) {
            return true;
        }
        if (eof_ || input_.bad()) {
            return false;
        }
        input_.read(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
        buffer_size_ = static_cast<std::size_t>(input_.gcount());
        buffer_position_ = 0;
        eof_ = input_.eof();
        return buffer_position_ < buffer_size_;
    }

    [[nodiscard]] char peek() const { return buffer_[buffer_position_]; }

    char consume() {
        const char value = buffer_[buffer_position_++];
        if (value == '\n') {
            line_start_ = true;
        } else if (!is_space(value)) {
            line_start_ = false;
        }
        return value;
    }

    void skip_line() {
        while (has_character() && peek() != '\n') {
            consume();
        }
        if (has_character()) {
            consume();
        }
    }

    void skip_brace_comment() {
        consume();
        while (has_character()) {
            if (consume() == '}') {
                return;
            }
        }
    }

    void skip_variation() {
        int depth = 1;
        while (has_character() && depth > 0) {
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
        std::string raw;
        const auto take = [&raw, this]() {
            const char value = consume();
            raw.push_back(value);
            return value;
        };
        take(); // [
        while (has_character() && is_horizontal_space(peek())) {
            take();
        }

        std::string key;
        while (has_character() && !is_space(peek()) && peek() != '"' && peek() != ']') {
            key.push_back(take());
        }
        while (has_character() && is_horizontal_space(peek())) {
            take();
        }
        if (key.empty() || !has_character() || take() != '"') {
            return {.kind = TokenKind::word, .text = std::move(raw), .value = {}};
        }

        std::string value;
        bool closed_quote = false;
        while (has_character()) {
            const char character = take();
            if (character == '"') {
                closed_quote = true;
                break;
            }
            if (character == '\\' && has_character()) {
                const char escaped = peek();
                if (escaped == '"' || escaped == '\\') {
                    value.push_back(take());
                } else {
                    value.push_back(character);
                }
            } else {
                value.push_back(character);
            }
        }
        while (has_character() && is_horizontal_space(peek())) {
            take();
        }
        if (!closed_quote || !has_character() || take() != ']') {
            return {.kind = TokenKind::word, .text = std::move(raw), .value = {}};
        }
        return {.kind = TokenKind::tag, .text = std::move(key), .value = std::move(value)};
    }

    Token read_nag() {
        std::string number;
        consume();
        while (has_character() && peek() >= '0' && peek() <= '9') {
            number.push_back(consume());
        }
        if (number.empty()) {
            return {.kind = TokenKind::word, .text = "$", .value = {}};
        }
        return {.kind = TokenKind::nag, .text = std::move(number), .value = {}};
    }

    Token read_word() {
        std::string word;
        while (has_character()) {
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

    std::istream& input_;
    std::array<char, static_cast<std::size_t>(64) * 1024> buffer_{};
    std::size_t buffer_position_ = 0;
    std::size_t buffer_size_ = 0;
    bool eof_ = false;
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
    if (board.halfMoveClock() > 150) {
        throw InputError("game " + std::to_string(game.game) + ", move " +
                         std::to_string(move_number) +
                         ": half-move clock exceeds 150 (75-move rule) after move '" +
                         std::string(token) + "' in " + path.string());
    }
    game.moves.push_back(std::move(parsed));
}

std::string file_error(std::string message, const std::filesystem::path& path) {
    constexpr std::string_view stream_name = "<stream>";
    if (message == "cannot read PGN stream") {
        return "cannot read PGN file " + path.string();
    }
    for (auto index = message.find(stream_name); index != std::string::npos;
         index = message.find(stream_name, index + path.string().size())) {
        message.replace(index, stream_name.size(), path.string());
    }
    return message;
}

} // namespace

struct PgnReader::Impl {
    Impl(std::istream& input, PgnReadOptions options)
        : input_(input), lexer_(input), options_(options), current_(lexer_.next()) {
        if (options_.max_plies.has_value() && *options_.max_plies < 0) {
            throw InputError("max_plies must not be negative");
        }
        check_input();
    }

    std::optional<PgnReadResult> next() {
        if (finished_ || current_.kind == TokenKind::end) {
            finished_ = true;
            return std::nullopt;
        }

        PgnGame game;
        game.game = game_number_++;
        while (current_.kind == TokenKind::tag) {
            game.headers.emplace_back(std::move(current_.text), std::move(current_.value));
            advance();
        }

        std::optional<std::string> error;
        chess::Board board;
        try {
            board = game_start(game, "<stream>");
            game.start = board;
        } catch (const InputError& failure) {
            error = failure.what();
        }

        while (!error.has_value() && current_.kind != TokenKind::end &&
               current_.kind != TokenKind::tag) {
            if (current_.kind == TokenKind::result) {
                advance();
                break;
            }
            if (current_.kind == TokenKind::nag) {
                if (!game.moves.empty()) {
                    int nag = 0;
                    const auto* first = current_.text.data();
                    const auto* last = first + current_.text.size();
                    const auto [end, conversion_error] = std::from_chars(first, last, nag);
                    if (conversion_error == std::errc{} && end == last) {
                        game.moves.back().nags.push_back(nag);
                    }
                }
                advance();
                continue;
            }
            if (current_.kind == TokenKind::word) {
                const std::string word = std::move(current_.text);
                advance();
                std::string_view move_token(word);
                if (strip_move_number(move_token) && move_token.empty()) {
                    continue;
                }
                if (move_token.empty() ||
                    (options_.max_plies.has_value() &&
                     game.moves.size() >= static_cast<std::size_t>(*options_.max_plies))) {
                    continue;
                }
                try {
                    append_move(game, board, move_token, "<stream>");
                } catch (const InputError& failure) {
                    error = failure.what();
                    break;
                }
                continue;
            }
            advance();
        }

        if (error.has_value()) {
            discard_broken_game();
        }
        return PgnReadResult{.game = std::move(game), .error = std::move(error)};
    }

private:
    void check_input() {
        if (input_.bad()) {
            throw InputError("cannot read PGN stream");
        }
    }

    void advance() {
        current_ = lexer_.next();
        check_input();
    }

    void discard_broken_game() {
        while (current_.kind != TokenKind::tag && current_.kind != TokenKind::end) {
            const bool result = current_.kind == TokenKind::result;
            advance();
            if (result) {
                break;
            }
        }
    }

    std::istream& input_;
    Lexer lexer_;
    PgnReadOptions options_;
    Token current_;
    int game_number_ = 1;
    bool finished_ = false;
};
PgnReader::PgnReader(std::istream& in, PgnReadOptions options)
    : impl_(std::make_unique<Impl>(in, options)) {}

PgnReader::~PgnReader() = default;
PgnReader::PgnReader(PgnReader&&) noexcept = default;
PgnReader& PgnReader::operator=(PgnReader&&) noexcept = default;

std::optional<PgnReadResult> PgnReader::next() {
    return impl_->next();
}

std::vector<PgnGame> read_games(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        throw InputError("cannot read PGN file " + path.string());
    }

    PgnReader reader(stream);
    std::vector<PgnGame> games;
    while (true) {
        std::optional<PgnReadResult> result;
        try {
            result = reader.next();
        } catch (const InputError& error) {
            throw InputError(file_error(error.what(), path));
        }
        if (!result.has_value()) {
            return games;
        }
        if (result->error.has_value()) {
            throw InputError(file_error(*result->error, path));
        }
        games.push_back(std::move(result->game));
    }
}

PgnPosition load_pgn(const std::filesystem::path& path, int game, std::optional<int> ply) {
    if (game < 1) {
        throw InputError("game " + std::to_string(game) + " must be 1-based");
    }
    if (ply.has_value() && *ply < 0) {
        throw InputError("ply " + std::to_string(*ply) + " must not be negative");
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        throw InputError("cannot read PGN file " + path.string());
    }
    PgnReader reader(stream);
    std::optional<PgnGame> selected;
    while (!selected.has_value()) {
        std::optional<PgnReadResult> result;
        try {
            result = reader.next();
        } catch (const InputError& error) {
            throw InputError(file_error(error.what(), path));
        }
        if (!result.has_value()) {
            break;
        }
        if (result->error.has_value()) {
            throw InputError(file_error(*result->error, path));
        }
        if (result->game.game == game) {
            selected = std::move(result->game);
        }
    }
    if (!selected.has_value()) {
        throw InputError("game " + std::to_string(game) + " not found in " + path.string());
    }
    const auto selected_ply = ply.value_or(static_cast<int>(selected->moves.size()));
    if (static_cast<std::size_t>(selected_ply) > selected->moves.size()) {
        throw InputError("ply " + std::to_string(selected_ply) + " is out of range for game " +
                         std::to_string(game) + " in " + path.string());
    }

    PgnPosition result;
    result.game = game;
    result.ply = selected_ply;
    result.board = selected->start;
    result.mainline.reserve(static_cast<std::size_t>(selected_ply));
    for (int index = 0; index < selected_ply; ++index) {
        result.board.makeMove(selected->moves[static_cast<std::size_t>(index)].move);
        result.mainline.push_back(result.board);
    }
    return result;
}
} // namespace chesswhy::input
