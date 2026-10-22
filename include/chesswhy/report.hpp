// Report data model, schema version 5. Frozen by Planning/milestones/M1.md, M2.md,
// PostM2_EM1.md, M3a.md, M3b.md and PostM3_EM2.md. JSON shape:
// schema/report.v5.schema.json. Every chess fact in the report is a field here; renderers
// and (from M4) the language model may only verbalise these fields.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace chesswhy {

inline constexpr int kSchemaVersion = 5;

enum class Side : std::uint8_t { White, Black };
enum class Budget : std::uint8_t { Quick, Standard, Deep };
enum class Level : std::uint8_t { Beginner, Club, Advanced };
enum class InputSource : std::uint8_t { Fen, Pgn };
enum class Terminal : std::uint8_t { Checkmate, Stalemate };
enum class ConsequenceClass : std::uint8_t { Mate, Material, Promotion, Positional };
enum class Verdict : std::uint8_t { Fine, Inaccuracy, Mistake, Blunder };
enum class UrgencyRule : std::uint8_t { InCheck, Quiet, ConsequenceClass, Counterattack, Parry };
enum class ParryTest : std::uint8_t { Searched, ThreatMoveIllegal, DeferredByCheck };
// Why `MoveAssessment::creates_threat` is null (PostM3_EM2 §4.4), checked in this order.
enum class CreatesThreatSkip : std::uint8_t { GivesCheck, GameOver, NoLegalMove };
// M2 §4, in table order: state motifs (§4.1, then PostM2_EM1 §4.5 `pin_tactical`), then
// move motifs (§4.2). The order is the tie-break of M2 §5; do not reorder.
enum class MotifTag : std::uint8_t {
    HangingPiece,
    PinAbsolute,
    PinRelative,
    BackRankWeakness,
    OverloadedDefender,
    TrappedPiece,
    PinTactical,
    Fork,
    Skewer,
    DiscoveredCheck,
    DiscoveredAttack,
    DoubleCheck,
    BackRankMate,
    Promotion,
};

// A motif's role in the tactic of the line it is listed on (M2 D11, §5; PostM2_EM1 §4.3).
// Set only on the motif lists of tactic lines (PostM2_EM1 §4.2); null elsewhere.
enum class MotifContribution : std::uint8_t { Direct, Indirect, None };

// What a `pin_tactical` motif's restricted piece holds off (PostM2_EM1 §4.5), in the
// order of that table; also the motif order's tie-break after the actor and targets.
enum class ThreatKind : std::uint8_t {
    Fork,
    Skewer,
    DiscoveredCheck,
    DiscoveredAttack,
    DoubleCheck,
    BackRankMate,
    Promotion,
    Checkmate,
    Capture,
};

// Tactic-side labels (PostM2_EM1 §4.4): reported only on tactic lines; not motifs.
enum class TacticLabelTag : std::uint8_t { RemovalOfDefender };

// M3a §4, in table order (§4.6): pawn structure, king safety, pieces, space and
// development, material. The order is the feature order's first key; do not reorder.
enum class PositionalTag : std::uint8_t {
    IsolatedPawn,
    DoubledPawns,
    BackwardPawn,
    PassedPawn,
    ConnectedPassedPawns,
    PawnIslands,
    PawnChain,
    CastlingKingside,
    CastlingQueenside,
    KingShieldGap,
    OpenFileNearKing,
    HalfOpenFileNearKing,
    KingZoneAttacker,
    BishopPair,
    RookOpenFile,
    RookHalfOpenFile,
    RookSeventh,
    Outpost,
    BadBishop,
    MobilityLead,
    SpaceLead,
    DevelopmentLead,
    MaterialAhead,
    ExchangeUp,
};

// M3a §5: a feature that appears or disappears along a line.
enum class PositionalChangeKind : std::uint8_t { Appears, Disappears };

// Lower-case JSON spellings: "white", "standard", "consequence_class", ...
[[nodiscard]] std::string to_string(Side side);
[[nodiscard]] std::string to_string(Budget budget);
[[nodiscard]] std::string to_string(Level level);
[[nodiscard]] std::string to_string(InputSource source);
[[nodiscard]] std::string to_string(Terminal terminal);
[[nodiscard]] std::string to_string(ConsequenceClass cls);
[[nodiscard]] std::string to_string(Verdict verdict);
[[nodiscard]] std::string to_string(UrgencyRule rule);
[[nodiscard]] std::string to_string(ParryTest test);
[[nodiscard]] std::string
to_string(CreatesThreatSkip skip);                 // "gives_check", "game_over", "no_legal_move"
[[nodiscard]] std::string to_string(MotifTag tag); // "hanging_piece", "pin_absolute", ...
[[nodiscard]] std::string to_string(MotifContribution contribution); // "direct", "indirect", "none"
[[nodiscard]] std::string to_string(ThreatKind kind);    // "fork", ..., "checkmate", "capture"
[[nodiscard]] std::string to_string(TacticLabelTag tag); // "removal_of_defender"
[[nodiscard]] std::string to_string(PositionalTag tag);  // "isolated_pawn", ...
[[nodiscard]] std::string to_string(PositionalChangeKind kind); // "appears", "disappears"
// Provenance of a parry decision: "engine" for searched, "rule" for the legality tests.
[[nodiscard]] const char* parry_source(ParryTest test);

struct InputInfo {
    InputSource source = InputSource::Fen;
    std::string fen;                 // analysed position, full FEN as produced by chess-library
    std::optional<int> pgn_game;     // 1-based, set iff source == Pgn
    std::optional<int> pgn_ply;      // half-moves played from the game's start position, iff Pgn
    std::optional<std::string> move; // --move exactly as typed
    Level level = Level::Club;
    Budget budget = Budget::Standard;
};

// Node budget per search kind (M1 §Budgets).
struct NodeBudget {
    std::uint64_t a = 0; // MultiPV K from the current position
    std::uint64_t b = 0; // null move, opponent to move
    std::uint64_t c = 0; // searchmoves m, for an asked move outside A
    std::uint64_t d = 0; // after m, searchmoves t (opponent's threat move)
    std::uint64_t e = 0; // after m, null move
    std::uint64_t r = 0; // after m, searchmoves all but the refutation's first move
};

struct EngineInfo {
    std::string name; // from UCI "id name", e.g. "Stockfish 19"
    int threads = 1;
    int hash_mb = 64;
    int multipv = 5;
    bool reproducible = true;         // false iff threads != 1
    NodeBudget nodes;                 // budget per search kind
    std::uint64_t nodes_searched = 0; // sum of "nodes" reported by every search run
};

// Expected score W + D/2 in [0, 1], from the side named by the containing field.
// Stored as an integer in units of 1/2000 (2W + D with W, D in permille) so that
// thresholds compare exactly; JSON prints es_units / 2000.0.
struct Eval {
    int es_units = 1000;
    std::optional<int> cp;   // display only
    std::optional<int> mate; // moves; positive = this side mates
};

// A piece standing on a square, as printed in a motif: {"square": "e5", "piece": "N"}.
struct PieceOnSquare {
    std::uint8_t square = 0; // 0..63, a1 = 0, b1 = 1, ..., h8 = 63
    char piece = 'P';        // FEN letter; upper case White, lower case Black
    bool operator==(const PieceOnSquare&) const = default;
};

// The threat behind a `pin_tactical` motif (PostM2_EM1 §4.5): X's move from the motif's
// actor square to `square` would make a threat of `kind` once the restricted piece left.
struct MotifThreat {
    ThreatKind kind = ThreatKind::Fork;
    std::uint8_t square = 0; // b, 0 = a1 ... 63 = h8
    bool operator==(const MotifThreat&) const = default;
};

// How a restricted-movement motif holds its piece (PostM3_EM2 §4.1, K2), from the free
// moves' distinct destinations: none → Square (name: the held piece's square); all on one
// file, rank or diagonal through that square → File ("e"), Rank ("7") or Diagonal ("a4-e8",
// edge squares, lower file first); otherwise Squares ("a2,d1,g8", ascending).
struct HeldTo {
    enum class Kind : std::uint8_t { File, Rank, Diagonal, Square, Squares };
    Kind kind = Kind::Square;
    std::string name;
    bool operator==(const HeldTo&) const = default;
};
[[nodiscard]] std::string
to_string(HeldTo::Kind kind); // "file", "rank", "diagonal", "square", "squares"

// The held piece of a restricted-movement motif and its moves (PostM3_EM2 §4.1, including
// hypothetical captures for pawns), computed with the held side to move (the motif's board, or
// that board after a null move). Source: rule.
struct MotifRestriction {
    PieceOnSquare piece;                 // the held piece h, as on the motif's board
    std::vector<std::string> restricted; // h's denied moves, UCI, ascending; never empty
    std::vector<std::string> free;       // h's other legal moves, UCI ascending
    HeldTo held_to;
    bool operator==(const MotifRestriction&) const = default;
};

// A tactical motif (M2 §4, §5): a pattern of one position. Source: rule. Identity for
// "newly present" (M2 §5) is (tag, side, actor, targets, threat); `ply`, `gain`,
// `restriction` and `contribution` are not part of it.
struct Motif {
    MotifTag tag = MotifTag::HangingPiece;
    Side side = Side::White; // X: the side the motif benefits
    int ply = 0;             // 0: the line's root; k: the position after the k-th move of the line
    PieceOnSquare actor;
    std::vector<PieceOnSquare> targets; // order per M2 §5 (PostM2_EM1 §4.5 for PinTactical)
    std::optional<int> gain;            // pawn units, set iff tag is HangingPiece or TrappedPiece
    std::optional<MotifThreat> threat;  // set iff tag is PinTactical
    std::optional<MotifRestriction> restriction;   // set iff tag is PinAbsolute, PinRelative,
                                                   // OverloadedDefender or PinTactical
    std::optional<MotifContribution> contribution; // null: not judged (PostM2_EM1 §4.2)
};

// A tactic-side label (PostM2_EM1 §4.4). Source: rule. Set only on tactic lines.
struct TacticLabel {
    TacticLabelTag tag = TacticLabelTag::RemovalOfDefender;
    Side side = Side::White;            // X: the tactic side
    int ply = 0;                        // k >= 1: the capturing move of the line
    PieceOnSquare actor;                // the capturing piece on its destination square, as on P_k
    std::vector<PieceOnSquare> targets; // d (the captured defender), then t (the defended piece)
};

// First move of the best alternative to a judged line (PostM2_EM1 §4.1). Source: engine.
struct TacticAlternative {
    std::string san;
    std::string uci;
    Eval eval; // the tactic side X's point of view
};

// Tactic judgement of a line (PostM2_EM1 §4.1). Source: engine, or rule iff forced.
struct TacticJudgement {
    bool forced = false;                          // the line's root has exactly one legal move
    std::optional<TacticAlternative> alternative; // null iff forced
    std::optional<int> margin_units;              // line ES - alternative ES; null iff forced
    bool is_tactic = false;                       // margin_units >= analysis::kTacticMarginUnits
    std::vector<TacticLabel> labels;              // empty unless is_tactic
};

// A positional feature of one board (M3a §4). Source: rule. Identity per the tag's
// Identity column (M3a §4), always including tag and side.
struct PositionalFeature {
    PositionalTag tag = PositionalTag::IsolatedPawn;
    Side side = Side::White;           // owner X (M3a §4 section headings)
    std::vector<PieceOnSquare> pieces; // order per the tag's pieces column
    std::optional<std::uint8_t> file;  // 0 = a ... 7 = h
    std::optional<int> value;          // set iff the tag has a value column entry
};

// A feature appearing or disappearing along a line (M3a §5). Source: rule.
struct PositionalChange {
    int ply = 0; // 1..walked plies; appears: as on P_k, disappears: as on P_k-1
    PositionalChangeKind change = PositionalChangeKind::Appears;
    bool holds_at_end = false;
    PositionalFeature feature;
};

struct PositionInfo {
    std::string fen;
    Side side_to_move = Side::White;
    bool in_check = false;
    int legal_moves = 0;              // legal moves of `fen` (PostM3_EM2 G8); source rule
    std::optional<Terminal> terminal; // set: no searches ran
    std::optional<Eval> eval_white;   // A's best line, White's point of view; null iff terminal
    std::vector<Motif> motifs;        // state motifs at ply 0, both sides; empty if terminal
    std::vector<PositionalFeature> positional; // M3a §4 features at ply 0, both sides, §4.6
                                               // order; also set for terminal positions
};

struct Consequence {
    ConsequenceClass cls = ConsequenceClass::Positional;
    std::optional<int> mate_in; // set iff cls == Mate
    int material = 0;           // net gain in pawn units for the threatening side over `plies`
    bool promotes = false;      // threatening side promotes within `plies`
    int plies = 0;              // plies of `line` the rule consumed
};

// A line played by `side` from `root_fen` (whose side to move is `side`), with its
// consequence for `side`. Source: engine line, rule consequence.
struct Threat {
    Side side = Side::White;
    std::string root_fen;
    std::vector<std::string> line_san; // at most 10 plies
    Consequence consequence;
    std::vector<Motif> motifs;             // plies 1..consequence.plies of the line (M2 §5)
    std::optional<TacticJudgement> tactic; // set only on judged refutations (PostM2_EM1 §4.1)
};

// Null on MoveAssessment when the opponent has no threat of class mate/material, and when
// the side to move is in check.
struct ParryInfo {
    bool parries = false; // the opponent's threat no longer wins against the pass baseline
    ParryTest test = ParryTest::Searched;
};

struct MoveAssessment {
    std::string san;
    std::string uci;
    Eval eval;          // side to move's point of view
    int depth = 0;      // search depth of the line that gave `eval` (MultiPV line or search C)
    int loss_units = 0; // max(0, best.es_units - eval.es_units)
    Verdict verdict = Verdict::Fine;
    bool in_multipv = true; // false: evaluated by search C
    bool gives_check = false;
    std::vector<std::string>
        line_san;         // from the current position, starts with this move, <= 10 plies
    int walked_plies = 0; // plies of line_san that motifs and positional were read from (rule)
    std::optional<ParryInfo> parries_threat; // null: no opponent threat of class mate/material
    std::optional<Threat> creates_threat;    // search E
    std::optional<CreatesThreatSkip> creates_threat_skipped; // null iff creates_threat is set
    std::optional<Threat> refutation; // opponent's reply line; set iff verdict != Fine
    std::vector<Motif> motifs; // plies 1..walked_plies of `line_san` (M2 §5); ply 1 is this move
    std::optional<TacticJudgement>
        tactic; // set only on candidates[0] and the asked move copying it
    std::vector<PositionalChange> positional; // M3a §5 over plies 1..walked_plies of line_san
};

struct Urgency {
    std::optional<Side> more_urgent; // null iff rule == Quiet
    UrgencyRule rule = UrgencyRule::Quiet;
};

struct Threats {
    std::optional<Threat> white; // null for the opponent when the side to move is in check or
                                 // has no legal move after the side to move's null move
    std::optional<Threat> black;
    std::optional<bool> null_move_reliable; // null iff side to move is in check
    Urgency urgency;
};

struct OpeningName {
    std::string eco;
    std::string name;
};

struct NamedPly {
    OpeningName opening;
    int ply = 0; // half-moves from the game's start position
};

struct OpeningInfo {
    std::string dataset; // openings::kDataset, or "custom@<12 hex>" (PostM3_EM2 §4.5)
    std::optional<OpeningName> current;
    std::optional<NamedPly>
        last_named; // PGN input only: last named position on the mainline up to the ply
};

// M3b §5: game counts of one source for the analysed position. Source: games. Strings
// and limits come from the table's header lines; `table` is games::kTable, or
// "custom@<12 hex>" when the loaded table is not the pinned one (PostM3_EM2 §4.5).
struct GameMoveCount {
    std::string san; // from the analysed position
    std::string uci;
    int games = 0;
};

struct GameSourceCounts {
    std::string id;       // "lichess_broadcast"
    std::string table;    // games::kTable or "custom@<12 hex>"
    std::string platform; // "over the board (may include online events relayed as broadcasts)"
    std::string players;  // "both rated 2200 or more"
    std::string months;   // "2020-01..2026-09"
    std::string credit;   // "Lichess broadcast database, database.lichess.org, CC BY-SA 4.0"
    int max_ply = 0;
    int min_games = 0;
    bool found = false;
    std::optional<int> games;         // null iff !found
    std::optional<int> white;         // null iff !found
    std::optional<int> draws;         // null iff !found
    std::optional<int> black;         // null iff !found
    std::vector<GameMoveCount> moves; // table order; empty iff !found
};

struct GamesInfo {
    std::vector<GameSourceCounts> by_source; // one entry today
};

struct Report {
    int schema_version = kSchemaVersion;
    InputInfo input;
    EngineInfo engine;
    PositionInfo position;
    std::optional<Threats> threats;         // null iff terminal
    std::vector<MoveAssessment> candidates; // search A, MultiPV order; empty iff terminal
    bool fine_truncated = false; // every candidate is fine; false does not mean the unlisted
                                 // moves are not fine
    std::optional<MoveAssessment> asked_move;
    OpeningInfo opening;
    GamesInfo games; // after opening; always set, also for terminal positions
};

// Serialisation (src/report). Key order follows the schema (ordered_json keeps insertion
// order; plain nlohmann::json would sort keys); output is deterministic.
[[nodiscard]] nlohmann::ordered_json to_json(const Report& report);

} // namespace chesswhy
