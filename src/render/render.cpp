#include "chesswhy/render.hpp"

#include "chesswhy/move_report.hpp"
#include "chesswhy/positional.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace chesswhy::render {
namespace {

[[nodiscard]] std::string san_line(const std::vector<std::string>& line) {
    std::ostringstream output;
    for (std::size_t index = 0; index < line.size(); ++index) {
        if (index != 0) {
            output << ' ';
        }
        output << line[index];
    }
    return output.str();
}

[[nodiscard]] const char* boolean(bool value) {
    return value ? "true" : "false";
}

void write_square(std::ostringstream& output, std::uint8_t square) {
    output << static_cast<char>('a' + (square % 8)) << static_cast<char>('1' + (square / 8));
}
void write_uci_square(std::ostringstream& output, const std::string& uci, std::size_t offset) {
    output << uci[offset] << uci[offset + 1];
}

void write_restricted_moves(std::ostringstream& output, const MotifRestriction& restriction,
                            const std::vector<std::string>& moves) {
    if (moves.empty()) {
        output << "none";
        return;
    }
    char piece = restriction.piece.piece;
    if (piece >= 'a' && piece <= 'z') {
        piece = static_cast<char>(piece - ('a' - 'A'));
    }
    for (std::size_t index = 0; index < moves.size(); ++index) {
        if (index != 0) {
            output << ", ";
        }
        const std::string& uci = moves[index];
        if (piece != 'P') {
            output << piece;
        }
        write_uci_square(output, uci, 0);
        output << '-';
        write_uci_square(output, uci, 2);
        if (uci.size() > 4) {
            char promotion = uci[4];
            if (promotion >= 'a' && promotion <= 'z') {
                promotion = static_cast<char>(promotion - ('a' - 'A'));
            }
            output << '=' << promotion;
        }
    }
}

void write_held_to(std::ostringstream& output, const HeldTo& held_to) {
    switch (held_to.kind) {
    case HeldTo::Kind::File:
        output << "along file " << held_to.name;
        return;
    case HeldTo::Kind::Rank:
        output << "along rank " << held_to.name;
        return;
    case HeldTo::Kind::Diagonal:
        output << "along diagonal " << held_to.name;
        return;
    case HeldTo::Kind::Square:
        output << "at square " << held_to.name;
        return;
    case HeldTo::Kind::Squares: {
        const bool multiple = held_to.name.find(',') != std::string::npos;
        output << "only to square" << (multiple ? "s " : " ");
        for (const char character : held_to.name) {
            if (character == ',') {
                output << ", ";
            } else {
                output << character;
            }
        }
        return;
    }
    }
}

void write_restriction(std::ostringstream& output, const MotifRestriction& restriction) {
    output << "; ";
    write_held_to(output, restriction.held_to);
    output << "; restricted: ";
    write_restricted_moves(output, restriction, restriction.restricted);
    output << "; free: ";
    write_restricted_moves(output, restriction, restriction.free);
}

[[nodiscard]] const char* side_name(Side side) {
    return side == Side::White ? "White" : "Black";
}

[[nodiscard]] const char* tempo_label_text(TempoLabel label) {
    switch (label) {
    case TempoLabel::TempoDependent:
        return "tempo-dependent position";
    case TempoLabel::NonTempo:
        return "non-tempo position";
    case TempoLabel::Zugzwang:
        return "zugzwang";
    }
    return "";
}

void write_tempo_value(std::ostringstream& output, const std::optional<TempoValue>& tempo_value,
                       bool after_move = false) {
    if (!tempo_value) {
        return;
    }
    output << (after_move ? "After the move: tempo value " : "Tempo value: ") << std::fixed
           << std::setprecision(1) << static_cast<double>(tempo_value->value_units) / 20.0
           << "% for " << side_name(tempo_value->side);
    if (tempo_value->label) {
        output << " (" << tempo_label_text(*tempo_value->label) << ')';
    }
    output << " [engine]\n";
}

[[nodiscard]] const char* tempo_ply_label_text(TempoPlyLabel label) {
    switch (label) {
    case TempoPlyLabel::Threatening:
        return "threatening";
    case TempoPlyLabel::PureReply:
        return "pure reply";
    case TempoPlyLabel::CounterThreat:
        return "counter threat";
    case TempoPlyLabel::Neither:
        return "neither";
    }
    return "";
}

void write_initiative_runs(std::ostringstream& output, const std::vector<InitiativeRun>& runs) {
    for (const InitiativeRun& run : runs) {
        output << "Initiative: " << side_name(run.holder) << " from ply " << run.start_ply << ", "
               << run.tempi << (run.tempi == 1 ? " tempo" : " tempi") << "; "
               << to_string(run.ending) << " at ply " << run.end_ply << "; "
               << to_string(run.outcome) << " [rule]\n";
    }
}

void write_tempo_ledger(std::ostringstream& output, const MoveAssessment& move) {
    if (!move.tempo_ledger) {
        if (move.tempo_ledger_skipped) {
            output << "Tempo ledger: skipped (";
            switch (*move.tempo_ledger_skipped) {
            case TempoLedgerSkip::InCheck:
                output << "S in check";
                break;
            case TempoLedgerSkip::NullMoveUnreliable:
                output << "null move unreliable";
                break;
            case TempoLedgerSkip::NotDetailed:
                output << "line not detailed";
                break;
            }
            output << ") [engine]\n";
        }
        return;
    }

    output << "Tempo ledger: ";
    bool has_ply = false;
    for (const TempoPly& ply : move.tempo_ledger->plies) {
        if (has_ply) {
            output << "; ";
        }
        output << ply.ply << ". " << ply.san << ' ' << tempo_ply_label_text(ply.label);
        if (ply.tempo_gained) {
            output << ", tempo gained";
        }
        has_ply = true;
    }
    if (has_ply) {
        output << "; ";
    }
    const int white_tempi = move.tempo_ledger->tempi.white;
    output << "White " << white_tempi << (white_tempi == 1 ? " tempo" : " tempi") << ", Black "
           << move.tempo_ledger->tempi.black << " [engine]\n";
    write_initiative_runs(output, move.tempo_ledger->runs);
}

void write_move_tempo_rows(std::ostringstream& output, const MoveReport& move) {
    write_tempo_value(output, move.tempo_value, true);
    if (move.tempo_gained) {
        for (const TempoGained& gained : *move.tempo_gained) {
            output << "Tempo: " << side_name(gained.side) << " gains a tempo with " << gained.san
                   << " (ply " << gained.ply << ") [engine]\n";
        }
    }
    if (move.initiative) {
        write_initiative_runs(output, *move.initiative);
    }
}

[[nodiscard]] const char* creates_threat_skip_reason(CreatesThreatSkip skip) {
    switch (skip) {
    case CreatesThreatSkip::GivesCheck:
        return "gives check";
    case CreatesThreatSkip::GameOver:
        return "game over";
    case CreatesThreatSkip::NoLegalMove:
        return "no legal move";
    }
    return "";
}

void write_header(std::ostringstream& output, const EngineInfo& engine, Budget budget) {
    output << "Engine: " << engine.name << " (Threads=" << engine.threads
           << ", Hash=" << engine.hash_mb << " MB) [rule]\n";
    const NodeBudget& nodes = engine.nodes;
    output << "Budget: " << to_string(budget) << ", nodes A=" << nodes.a << ", B=" << nodes.b
           << ", C=" << nodes.c << ", D=" << nodes.d << ", E=" << nodes.e << ", L=" << nodes.l
           << ", R=" << nodes.r << " [rule]\n";
}

void write_header(std::ostringstream& output, const Report& report) {
    write_header(output, report.engine, report.input.budget);
}

void write_engine_line_count(std::ostringstream& output, const Report& report) {
    const std::size_t count = report.candidates.size();
    output << "Engine lines: " << count << " of " << report.position.legal_moves
           << " legal moves (engine's top " << count << ')';
    if (report.position.legal_moves > 0 &&
        count < static_cast<std::size_t>(report.position.legal_moves)) {
        output << "; unlisted moves are not judged and may also be playable";
    }
    output << " [rule]\n";
}

void write_move_eval(std::ostringstream& output, const MoveAssessment& move) {
    output << std::fixed << std::setprecision(1) << static_cast<double>(move.eval.es_units) / 20.0;
}

void write_move_eval(std::ostringstream& output, const MoveReport& move) {
    output << std::fixed << std::setprecision(1) << static_cast<double>(move.eval.es_units) / 20.0;
}

void write_walk(std::ostringstream& output, const std::string& label, const MoveAssessment& move) {
    output << label << " facts read to ply " << move.walked_plies << " of " << move.line_san.size()
           << " [rule]\n";
}

void write_walk(std::ostringstream& output, const MoveReport& move) {
    output << "Line: " << san_line(move.line_san) << "; facts read to ply " << move.walked_plies
           << " of " << move.line_san.size() << " [engine]\n";
}

[[nodiscard]] bool restricted_movement_tag(MotifTag tag) {
    switch (tag) {
    case MotifTag::PinAbsolute:
    case MotifTag::PinRelative:
    case MotifTag::OverloadedDefender:
    case MotifTag::PinTactical:
        return true;
    default:
        return false;
    }
}

void write_motif_inner_label(std::ostringstream& output, MotifTag tag,
                             const std::optional<MotifThreat>& threat, const PieceOnSquare& actor) {
    switch (tag) {
    case MotifTag::PinAbsolute:
        output << "absolute pin";
        return;
    case MotifTag::PinRelative:
        output << "relative pin";
        return;
    case MotifTag::OverloadedDefender:
        output << "overloaded defender";
        return;
    case MotifTag::PinTactical: {
        if (!threat.has_value()) {
            throw std::bad_optional_access{};
        }
        char actor_piece = actor.piece;
        if (actor_piece >= 'a' && actor_piece <= 'z') {
            actor_piece = static_cast<char>(actor_piece - ('a' - 'A'));
        }
        std::string kind = to_string(threat->kind);
        for (char& character : kind) {
            if (character == '_') {
                character = ' ';
            }
        }
        output << "tactic: " << actor_piece;
        write_square(output, actor.square);
        output << '-';
        write_square(output, threat->square);
        output << ' ' << kind;
        return;
    }
    default:
        output << to_string(tag);
        return;
    }
}

void write_motif_label(std::ostringstream& output, const Motif& motif) {
    const bool restricted = restricted_movement_tag(motif.tag);
    if (restricted) {
        output << "restricted movement (";
    }
    write_motif_inner_label(output, motif.tag, motif.threat, motif.actor);
    if (restricted) {
        output << ')';
    }
}

void write_compact_restricted_motif_label(std::ostringstream& output, const Motif& motif) {
    output << "restricted movement (";
    write_motif_inner_label(output, motif.tag, motif.threat, motif.actor);
    for (const MotifLabel& label : motif.also) {
        output << "; ";
        write_motif_inner_label(output, label.tag, label.threat, motif.actor);
    }
    output << ')';
}

void write_motif(std::ostringstream& output, const Motif& motif, bool use,
                 bool compact_restricted) {
    if (use) {
        output << "Uses: ";
    }
    const bool compact =
        compact_restricted && motif.restriction && restricted_movement_tag(motif.tag);
    if (compact) {
        write_compact_restricted_motif_label(output, motif);
    } else {
        write_motif_label(output, motif);
    }
    output << ": " << motif.actor.piece << ' ';
    write_square(output, motif.actor.square);
    if (!motif.targets.empty()) {
        output << " → ";
        for (std::size_t index = 0; index < motif.targets.size(); ++index) {
            if (index != 0) {
                output << ", ";
            }
            output << motif.targets[index].piece << ' ';
            write_square(output, motif.targets[index].square);
        }
    }
    if (motif.gain) {
        output << ", SEE " << *motif.gain;
    }
    if (motif.restriction) {
        if (compact) {
            output << "; ";
            write_held_to(output, motif.restriction->held_to);
        } else {
            write_restriction(output, *motif.restriction);
        }
    }
    output << " (ply " << motif.ply;
    if (motif.contribution) {
        output << ", " << to_string(*motif.contribution);
    }
    output << ") [rule]\n";
}

void write_motifs(std::ostringstream& output, const std::vector<Motif>& motifs, bool use = false,
                  bool compact_restricted = false) {
    for (const auto& motif : motifs) {
        write_motif(output, motif, use, compact_restricted);
    }
}

void write_tactic_labels(std::ostringstream& output, const std::vector<TacticLabel>& labels) {
    for (const auto& label : labels) {
        output << to_string(label.tag) << ": " << label.actor.piece << ' ';
        write_square(output, label.actor.square);
        if (!label.targets.empty()) {
            output << " → ";
            for (std::size_t index = 0; index < label.targets.size(); ++index) {
                if (index != 0) {
                    output << ", ";
                }
                output << label.targets[index].piece << ' ';
                write_square(output, label.targets[index].square);
            }
        }
        output << " (ply " << label.ply << ") [rule]\n";
    }
}

void write_opportunity(std::ostringstream& output,
                       const std::optional<OpportunityJudgement>& opportunity) {
    if (!opportunity) {
        return;
    }
    if (opportunity->forced) {
        output << "Forced: only legal move [rule]\n";
        return;
    }
    if (!opportunity->is_opportunity || !opportunity->alternative.has_value() ||
        !opportunity->margin_units.has_value()) {
        return;
    }

    const auto flags = output.flags();
    const auto precision = output.precision();
    output << "Opportunity: margin " << std::fixed << std::setprecision(2)
           << static_cast<double>(*opportunity->margin_units) / 2000.0 << " over "
           << opportunity->alternative->san << " [engine]\n";
    output.flags(flags);
    output.precision(precision);
}

void write_consequence_summary(std::ostringstream& output, const Consequence& consequence,
                               Side side, bool include_side) {
    if (consequence.cls == ConsequenceClass::Mate) {
        if (include_side) {
            output << side_name(side) << " mates in ";
        } else {
            output << "mate in ";
        }
        if (consequence.mate_in) {
            output << *consequence.mate_in;
        } else {
            output << "null";
        }
        return;
    }
    if (consequence.cls == ConsequenceClass::Material) {
        if (include_side) {
            output << side_name(side) << ' ';
        }
        const int material = consequence.material;
        const int absolute_material = material < 0 ? -material : material;
        output << (material < 0 ? "loses " : "wins ") << absolute_material
               << (absolute_material == 1 ? " pawn net by ply " : " pawns net by ply ")
               << consequence.plies;
    }
    if (consequence.cls == ConsequenceClass::Promotion) {
        if (include_side) {
            output << side_name(side) << ' ';
        }
        output << "promotes by ply " << consequence.plies;
    }
}

void write_tactic(std::ostringstream& output, const Consequence& consequence) {
    output << "Tactic: ";
    write_consequence_summary(output, consequence, Side::White, false);
    output << (consequence.cls == ConsequenceClass::Mate ? " [engine]\n" : " [rule]\n");
}

void write_refutation(std::ostringstream& output, const Threat& refutation) {
    output << "Refuted by: " << san_line(refutation.line_san);
    if (refutation.is_tactic.value_or(false)) {
        output << " ; a tactic: ";
        write_consequence_summary(output, refutation.consequence, refutation.side, true);
        output << (refutation.consequence.cls == ConsequenceClass::Mate ? " [engine]\n"
                                                                        : " [rule]\n");
    } else {
        output << " [engine]\n";
    }
}

void write_consequence(std::ostringstream& output, const std::string& label,
                       const Consequence& consequence) {
    output << label << ": class=" << to_string(consequence.cls) << ", mate_in=";
    if (consequence.mate_in) {
        output << *consequence.mate_in;
    } else {
        output << "null";
    }
    output << ", material=" << consequence.material
           << ", promotes=" << boolean(consequence.promotes) << ", plies=" << consequence.plies
           << " [rule]\n";
}

void write_threat(std::ostringstream& output, const std::string& label,
                  const std::optional<Threat>& threat) {
    if (!threat) {
        output << label << ": null [rule]\n";
        return;
    }
    output << label << " line: " << san_line(threat->line_san) << " [engine]\n";
    write_opportunity(output, threat->opportunity);
    write_tactic_labels(output, threat->tactic_labels);
    write_motifs(output, threat->motifs);
    write_consequence(output, label + " consequence", threat->consequence);
}
void write_positional_change(std::ostringstream& output, const PositionalChange& change,
                             std::optional<int> horizon = std::nullopt) {
    const PositionalFeature& feature = change.feature;
    if (horizon) {
        output << "By ply " << *horizon << ": ";
    }
    output << (change.change == PositionalChangeKind::Appears ? '+' : '-') << ' '
           << to_string(feature.tag) << ": " << to_string(feature.side);
    if (feature.value) {
        output << ' ' << *feature.value;
    }
    if (feature.file) {
        output << ", " << static_cast<char>('a' + *feature.file) << " file";
    }
    if (!feature.pieces.empty()) {
        output << ": ";
        for (std::size_t index = 0; index < feature.pieces.size(); ++index) {
            if (index != 0) {
                output << ", ";
            }
            output << feature.pieces[index].piece << ' ';
            write_square(output, feature.pieces[index].square);
        }
    }
    output << " (ply " << change.ply;
    if (!change.holds_at_end) {
        output << ", not at end";
    }
    output << ')';
    if (!change.net_change) {
        output << " (passing change)";
    }
    if (change.in_use && !*change.in_use) {
        output << " (fails the use test)";
    }
    output << " [rule]\n";
}

void write_positional_changes(std::ostringstream& output,
                              const std::vector<PositionalChange>& changes) {
    for (const auto& change : changes) {
        write_positional_change(output, change);
    }
}

void write_move_positional_group(std::ostringstream& output, const MoveReport& move,
                                 ImpactGroup group) {
    const bool dynamic = group == ImpactGroup::PositionalDynamic;
    for (const PositionalChange& change : move.positional) {
        if (change.net_change && change.in_use == true &&
            positional::is_dynamic(change.feature.tag) == dynamic) {
            write_positional_change(output, change, move.walked_plies);
        }
    }
}

void write_game_move_counts(std::ostringstream& output, const MoveAssessment& move,
                            const GamesInfo& games) {
    for (const auto& source : games.by_source) {
        if (!source.found) {
            continue;
        }
        const GameMoveCount* move_count = nullptr;
        for (const auto& candidate : source.moves) {
            if (candidate.uci == move.uci) {
                move_count = &candidate;
                break;
            }
        }
        if (move_count != nullptr) {
            if (!source.games.has_value()) {
                throw std::bad_optional_access{};
            }
            output << "games: played in " << move_count->games << " of " << *source.games
                   << " games [games]\n";
        } else {
            output << "games: not played in these games [games]\n";
        }
    }
}

void write_games_summary(std::ostringstream& output, const GamesInfo& games) {
    for (const auto& source : games.by_source) {
        if (!source.found) {
            output << "games: position not in the table (it keeps positions reached in at least "
                   << source.min_games << " games within the first " << source.max_ply
                   << " plies) [games]\n";
            continue;
        }
        if (!source.games.has_value() || !source.white.has_value() || !source.draws.has_value() ||
            !source.black.has_value()) {
            throw std::bad_optional_access{};
        }
        output << "games: " << *source.games << " games, " << source.players
               << " (platform: " << source.platform << "; source: " << source.credit
               << "; months: " << source.months << "): White won " << *source.white << ", draws "
               << *source.draws << ", Black won " << *source.black << " [games]\n";
    }
}
void write_opening(std::ostringstream& output, const OpeningInfo& opening) {
    output << "Opening dataset: " << opening.dataset << " [book]\n";
    if (opening.current) {
        output << "Current opening: " << opening.current->eco << ' ' << opening.current->name
               << " [book]\n";
    } else {
        output << "Current opening: null [book]\n";
    }
    if (opening.last_named) {
        output << "Last named opening: " << opening.last_named->opening.eco << ' '
               << opening.last_named->opening.name << ", ply=" << opening.last_named->ply
               << " [book]\n";
    } else {
        output << "Last named opening: null [book]\n";
    }
}

void write_move_position(std::ostringstream& output, const PositionSummary& position) {
    output << "Position: " << side_name(position.side_to_move) << " to move, "
           << position.legal_moves << " legal moves";
    if (position.terminal) {
        output << "; " << to_string(*position.terminal) << " [rule]\n";
        return;
    }
    if (!position.eval_white) {
        output << "; evaluation unavailable [rule]\n";
        return;
    }
    const Eval& eval = *position.eval_white;
    output << "; evaluation " << std::fixed << std::setprecision(1)
           << static_cast<double>(eval.es_units) / 20.0 << "% for White";
    if (eval.cp) {
        output << " (cp " << *eval.cp;
        if (eval.mate) {
            output << ", mate " << *eval.mate;
        }
        output << ')';
    } else if (eval.mate) {
        output << " (mate " << *eval.mate << ')';
    }
    output << " [engine]\n";
}

[[nodiscard]] bool is_playable(Verdict verdict) {
    return verdict == Verdict::Best || verdict == Verdict::Okay;
}

[[nodiscard]] bool matches_keyword(Verdict verdict, Selection::Keyword keyword) {
    switch (keyword) {
    case Selection::Keyword::Best:
        return verdict == Verdict::Best;
    case Selection::Keyword::Playable:
        return is_playable(verdict);
    case Selection::Keyword::Inaccuracies:
        return verdict == Verdict::Inaccuracy;
    case Selection::Keyword::Mistakes:
        return verdict == Verdict::Mistake;
    case Selection::Keyword::Blunders:
        return verdict == Verdict::Blunder;
    case Selection::Keyword::Unplayable:
        return verdict == Verdict::Inaccuracy || verdict == Verdict::Mistake ||
               verdict == Verdict::Blunder;
    case Selection::Keyword::All:
        return true;
    }
    return false;
}

[[nodiscard]] const char* selection_phrase(Selection::Keyword keyword) {
    switch (keyword) {
    case Selection::Keyword::Best:
        return "the best move";
    case Selection::Keyword::Playable:
        return "every playable move";
    case Selection::Keyword::Inaccuracies:
        return "every inaccuracy";
    case Selection::Keyword::Mistakes:
        return "every mistake";
    case Selection::Keyword::Blunders:
        return "every blunder";
    case Selection::Keyword::Unplayable:
        return "every unplayable move";
    case Selection::Keyword::All:
        return "every legal move";
    }
    return "";
}

void write_selection_count(std::ostringstream& output, const MoveReportDocument& document,
                           Selection::Keyword keyword) {
    const int legal_moves = document.position.legal_moves;
    const auto count = static_cast<int>(std::count_if(
        document.moves.begin(), document.moves.end(),
        [keyword](const MoveReport& move) { return matches_keyword(move.verdict, keyword); }));
    if (keyword == Selection::Keyword::Playable || keyword == Selection::Keyword::All ||
        keyword == Selection::Keyword::Unplayable) {
        output << " (" << count << " of " << legal_moves << " legal moves)";
        return;
    }
    output << ": ";
    if (count == 0) {
        output << "none";
    } else {
        output << count;
    }
    output << " among " << legal_moves << " legal moves";
}

void write_selection(std::ostringstream& output, const MoveReportDocument& document) {
    const Selection& selection = document.selection.selection;
    const std::size_t item_count = selection.moves.size() + selection.keywords.size() +
                                   static_cast<std::size_t>(selection.top.has_value());
    output << "Showing ";
    if (item_count == 0) {
        output << "no moves [rule]\n";
        return;
    }

    bool has_item = false;
    for (const std::string& uci : selection.moves) {
        if (has_item) {
            output << " and ";
        }
        const auto move = std::ranges::find_if(
            document.moves, [&uci](const MoveReport& report) { return report.uci == uci; });
        output << (move == document.moves.end() ? uci : move->san);
        has_item = true;
    }
    for (const Selection::Keyword keyword : selection.keywords) {
        if (has_item) {
            output << " and ";
        }
        output << selection_phrase(keyword);
        has_item = true;
    }
    if (selection.top) {
        if (has_item) {
            output << " and ";
        }
        output << "the top " << *selection.top << " moves";
    }

    if (item_count == 1 && selection.moves.empty()) {
        const bool counts_playable =
            selection.top.has_value() || selection.keywords.front() == Selection::Keyword::Best;
        if (!counts_playable) {
            write_selection_count(output, document, selection.keywords.front());
        } else if (document.position.terminal) {
            output << "; none (terminal position)";
        } else {
            const int playable = document.selection.playable_count;
            output << "; ";
            if (playable == 0) {
                output << "none";
            } else {
                output << playable << (playable == 1 ? " playable move" : " playable moves");
            }
            output << " among the engine's top " << document.selection.lines_searched;
        }
    }
    output << " [rule]\n";
}

void write_move_games(std::ostringstream& output, const MoveReport& move,
                      const std::vector<GameSourceSummary>& games) {
    if (!move.games || games.empty() || !games.front().found) {
        return;
    }
    const GameSourceSummary& source = games.front();
    if (!source.games) {
        throw std::bad_optional_access{};
    }
    output << "Played in " << *move.games << " of " << *source.games
           << " games (platform: " << source.platform << "; source: " << source.credit
           << ") [games]\n";
}

void write_move_report_threat(std::ostringstream& output, const MoveReport& move, Side side_to_move,
                              ImpactGroup group) {
    if (group == ImpactGroup::ParriedThreat && move.parried_threat) {
        const ParriedThreat& parried = *move.parried_threat;
        output << "Parries " << side_name(parried.threat.side)
               << "'s threat: " << san_line(parried.threat.line_san) << " (";
        write_consequence_summary(output, parried.threat.consequence, parried.threat.side, true);
        output << ") [" << parry_source(parried.test) << "]\n";
    } else if (group == ImpactGroup::CreatedThreat && move.created_threat) {
        const Side passing_side = side_to_move == Side::White ? Side::Black : Side::White;
        output << "Threatens (if " << side_name(passing_side)
               << " passes): " << san_line(move.created_threat->line_san) << " (";
        write_consequence_summary(output, move.created_threat->consequence,
                                  move.created_threat->side, true);
        output << ") [engine]\n";
    }
}

void write_move_report_group(std::ostringstream& output, const MoveReportDocument& document,
                             const MoveReport& move, ImpactGroup group) {
    switch (group) {
    case ImpactGroup::ParriedThreat:
    case ImpactGroup::CreatedThreat:
        write_move_report_threat(output, move, document.position.side_to_move, group);
        return;
    case ImpactGroup::Motifs:
        write_motifs(output, move.motifs, true, true);
        write_motifs(output, move.root_motifs, true, true);
        return;
    case ImpactGroup::Tempo:
        write_move_tempo_rows(output, move);
        return;
    case ImpactGroup::PositionalDynamic:
    case ImpactGroup::PositionalStatic:
        write_move_positional_group(output, move, group);
        return;
    }
}

void write_move_report(std::ostringstream& output, const MoveReportDocument& document,
                       const MoveReport& move) {
    if (move.rank) {
        output << *move.rank << ". ";
    } else {
        output << "Asked: ";
    }
    output << move.san << " (" << move.uci << "): " << to_string(move.verdict) << ", es=";
    write_move_eval(output, move);
    output << "%, loss=" << std::fixed << std::setprecision(1)
           << static_cast<double>(move.loss_units) / 20.0 << "%, depth=" << move.depth
           << " [engine]\n";

    write_walk(output, move);
    if (move.is_tactic) {
        write_tactic(output, move.consequence);
    }
    write_opportunity(output, move.opportunity);
    write_tactic_labels(output, move.tactic_labels);
    if (move.refutation) {
        write_refutation(output, *move.refutation);
        write_opportunity(output, move.refutation->opportunity);
        write_tactic_labels(output, move.refutation->tactic_labels);
        write_motifs(output, move.refutation->motifs, true, true);
    }
    for (const ImpactGroup group : move.impact_order) {
        write_move_report_group(output, document, move, group);
    }
    write_move_games(output, move, document.games);
}

void write_move_refutation(std::ostringstream& output, const std::string& label,
                           const MoveAssessment& move, bool detailed) {
    if (move.refutation) {
        write_refutation(output, *move.refutation);
        if (detailed) {
            write_opportunity(output, move.refutation->opportunity);
        }
        write_tactic_labels(output, move.refutation->tactic_labels);
        write_motifs(output, move.refutation->motifs);
        write_consequence(output, label + " refutation consequence", move.refutation->consequence);
    } else if (detailed) {
        output << label << " refutation: null [rule]\n";
    }
}

void write_move_details(std::ostringstream& output, const std::string& label,
                        const MoveAssessment& move, const GamesInfo& games, Side side_to_move) {
    output << label << " line: " << san_line(move.line_san) << " [engine]\n";
    write_walk(output, label, move);
    write_tempo_value(output, move.tempo_value, true);
    write_tempo_ledger(output, move);
    if (move.is_tactic) {
        write_tactic(output, move.consequence);
    }
    write_positional_changes(output, move.positional);
    write_game_move_counts(output, move, games);
    write_opportunity(output, move.opportunity);
    write_tactic_labels(output, move.tactic_labels);
    write_motifs(output, move.motifs);
    write_motifs(output, move.root_motifs);

    if (!move.detailed) {
        write_move_refutation(output, label, move, false);
        return;
    }
    if (move.parries_threat) {
        output << label << " parries threat: " << boolean(move.parries_threat->parries) << " (";
        if (move.parries_threat->test == ParryTest::DeferredByCheck) {
            output << "deferred_by_check: the move checks, so the threat only waits";
        } else {
            output << to_string(move.parries_threat->test);
        }
        output << ") [" << parry_source(move.parries_threat->test) << "]\n";
    } else {
        output << label << " parries threat: null [rule]\n";
    }
    if (move.creates_threat) {
        output << label << " creates threat side: " << to_string(move.creates_threat->side)
               << " [engine]\n";
        const Side passing_side = side_to_move == Side::White ? Side::Black : Side::White;
        output << label << " creates threat line (if " << side_name(passing_side)
               << " passes after " << move.san << "): " << san_line(move.creates_threat->line_san)
               << " [engine]\n";
        write_motifs(output, move.creates_threat->motifs);
        write_consequence(output, label + " creates threat consequence",
                          move.creates_threat->consequence);
    } else {
        output << label << " creates threat: not searched";
        if (move.creates_threat_skipped) {
            output << " (" << creates_threat_skip_reason(*move.creates_threat_skipped) << ')';
        }
        output << " [rule]\n";
    }
    write_move_refutation(output, label, move, true);
}

void write_eval(std::ostringstream& output, const std::string& label, const Eval& eval) {
    output << label << ": " << std::fixed << std::setprecision(1)
           << static_cast<double>(eval.es_units) / 20.0 << "% ES";
    if (eval.cp) {
        output << ", cp=" << *eval.cp;
    }
    if (eval.mate) {
        output << ", mate=" << *eval.mate;
    }
    output << " [engine]\n";
}

} // namespace

std::string text(const Report& report) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    write_header(output, report);
    output << "Position side to move: " << to_string(report.position.side_to_move) << " [rule]\n";
    output << "Position in check: " << boolean(report.position.in_check) << " [rule]\n";
    output << "Position terminal: ";
    if (report.position.terminal) {
        output << to_string(*report.position.terminal);
    } else {
        output << "null";
    }
    output << " [rule]\n";
    if (report.position.eval_white) {
        write_eval(output, "Position evaluation", *report.position.eval_white);
    } else {
        output << "Position evaluation: null [rule]\n";
    }
    write_tempo_value(output, report.position.tempo_value);

    if (!report.position.motifs.empty()) {
        output << "Position motifs: [rule]\n";
        write_motifs(output, report.position.motifs);
    }

    if (report.threats) {
        write_threat(output, "White threat", report.threats->white);
        write_threat(output, "Black threat", report.threats->black);
        output << "Threat null-move reliable: ";
        if (report.threats->null_move_reliable) {
            output << boolean(*report.threats->null_move_reliable);
        } else {
            output << "null";
        }
        output << " [rule]\n";
        output << "Threat urgency: ";
        if (report.threats->urgency.more_urgent) {
            output << to_string(*report.threats->urgency.more_urgent);
        } else {
            output << "null";
        }
        output << ", rule=" << to_string(report.threats->urgency.rule) << " [rule]\n";
    } else {
        output << "Threats: null [rule]\n";
    }

    write_engine_line_count(output, report);
    for (std::size_t index = 0; index < report.candidates.size(); ++index) {
        const auto& engine_line = report.candidates[index];
        const std::string label = "Engine line " + std::to_string(index + 1);
        output << label << ": " << engine_line.san << " (" << engine_line.uci << "), "
               << to_string(engine_line.verdict) << ", es=";
        write_move_eval(output, engine_line);
        output << "%, loss=" << std::fixed << std::setprecision(1)
               << static_cast<double>(engine_line.loss_units) / 20.0
               << "%, depth=" << engine_line.depth << " [engine]\n";
        write_move_details(output, label, engine_line, report.games, report.position.side_to_move);
    }

    for (std::size_t index = 0; index < report.asked_moves.size(); ++index) {
        const auto& asked_move = report.asked_moves[index];
        const std::string label = "Asked move " + std::to_string(index + 1);
        output << label << ": " << asked_move.san << " (" << asked_move.uci << "), "
               << to_string(asked_move.verdict)
               << ", in_multipv: " << boolean(asked_move.in_multipv) << " [engine]\n";
        write_move_details(output, label, asked_move, report.games, report.position.side_to_move);
    }
    if (report.asked_moves.empty()) {
        output << "Asked move: null [rule]\n";
    }

    write_opening(output, report.opening);
    write_games_summary(output, report.games);
    return output.str();
}

std::string text(const MoveReportDocument& document) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    write_header(output, document.engine, document.input.budget);
    write_move_position(output, document.position);
    write_tempo_value(output, document.position.tempo_value);
    write_opening(output, document.opening);
    write_selection(output, document);
    for (const MoveReport& move : document.moves) {
        write_move_report(output, document, move);
    }
    return output.str();
}

} // namespace chesswhy::render
