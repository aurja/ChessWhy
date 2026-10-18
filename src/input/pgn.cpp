#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <chesswhy/input.hpp>

namespace chesswhy::input {
namespace {

class MainlineVisitor final : public chess::pgn::Visitor {
public:
    explicit MainlineVisitor(int requested_game) : requested_game_(requested_game) {}

    void startPgn() override { begin_game(); }

    void header(std::string_view key, std::string_view value) override {
        if (!selected_) {
            return;
        }
        if (key == "FEN") {
            fen_.assign(value);
        } else if (key == "SetUp") {
            setup_.assign(value);
        }
    }

    void startMoves() override {
        if (!active_) {
            begin_game();
        }
        if (!selected_) {
            return;
        }
        if (setup_ == "1" && fen_.empty()) {
            throw InputError("game " + std::to_string(requested_game_) +
                             ": invalid FEN: [SetUp] requires a [FEN] tag");
        }
        start_board_ = chess::Board{};
        if (!fen_.empty()) {
            start_board_ = parse_fen(fen_);
        }
        board_ = start_board_;
        positions_.clear();
    }

    void move(std::string_view token, [[maybe_unused]] std::string_view comment) override {
        if (!selected_ || token.empty()) {
            return;
        }
        const auto move_number = board_.fullMoveNumber();
        chess::Move parsed = chess::Move::NO_MOVE;
        try {
            parsed = parse_move(board_, token);
        } catch (const InputError& error) {
            throw InputError("game " + std::to_string(requested_game_) + ", move " +
                             std::to_string(move_number) + ": " + error.what());
        }
        board_.makeMove(parsed);
        positions_.push_back(board_);
    }

    void endPgn() override {
        if (selected_) {
            completed_start_ = start_board_;
            completed_positions_ = positions_;
            found_ = true;
        }
        active_ = false;
        selected_ = false;
    }

    [[nodiscard]] int games_seen() const { return games_seen_; }
    [[nodiscard]] bool found() const { return found_; }
    [[nodiscard]] const chess::Board& start_board() const { return completed_start_; }
    [[nodiscard]] const std::vector<chess::Board>& positions() const {
        return completed_positions_;
    }

private:
    void begin_game() {
        ++games_seen_;
        active_ = true;
        selected_ = games_seen_ == requested_game_;
        fen_.clear();
        setup_.clear();
        positions_.clear();
    }

    int requested_game_;
    int games_seen_ = 0;
    bool active_ = false;
    bool selected_ = false;
    bool found_ = false;
    std::string fen_;
    std::string setup_;
    chess::Board start_board_;
    chess::Board board_;
    std::vector<chess::Board> positions_;
    chess::Board completed_start_;
    std::vector<chess::Board> completed_positions_;
};

} // namespace

PgnPosition load_pgn(const std::filesystem::path& path, int game, std::optional<int> ply) {
    if (game < 1) {
        throw InputError("game " + std::to_string(game) + " must be 1-based");
    }
    if (ply.has_value() && *ply < 0) {
        throw InputError("ply " + std::to_string(*ply) + " must not be negative");
    }

    std::ifstream stream(path);
    if (!stream.is_open()) {
        throw InputError("cannot read PGN file " + path.string());
    }

    MainlineVisitor visitor(game);
    chess::pgn::StreamParser parser(stream);
    const auto parser_error = parser.readGames(visitor);
    if (parser_error != chess::pgn::StreamParserError::None &&
        parser_error != chess::pgn::StreamParserError::NotEnoughData) {
        throw InputError("invalid PGN " + path.string() + ": " + parser_error.message());
    }
    if (visitor.games_seen() < game || !visitor.found()) {
        throw InputError("game " + std::to_string(game) + " not found in " + path.string());
    }

    const auto& positions = visitor.positions();
    const auto selected_ply = ply.value_or(static_cast<int>(positions.size()));
    if (static_cast<std::size_t>(selected_ply) > positions.size()) {
        throw InputError("ply " + std::to_string(selected_ply) + " is out of range for game " +
                         std::to_string(game));
    }

    PgnPosition result;
    result.game = game;
    result.ply = selected_ply;
    result.board = selected_ply == 0 ? visitor.start_board()
                                     : positions[static_cast<std::size_t>(selected_ply - 1)];
    result.mainline.assign(positions.begin(), positions.begin() + selected_ply);
    return result;
}

} // namespace chesswhy::input
