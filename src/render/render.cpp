#include "chesswhy/render.hpp"

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
        for (char character : held_to.name) {
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

void write_header(std::ostringstream& output, const Report& report) {
    output << "Engine: " << report.engine.name << " (Threads=" << report.engine.threads
           << ", Hash=" << report.engine.hash_mb << " MB) [rule]\n";
    const NodeBudget& nodes = report.engine.nodes;
    output << "Budget: " << to_string(report.input.budget) << ", nodes A=" << nodes.a
           << ", B=" << nodes.b << ", C=" << nodes.c << ", D=" << nodes.d << ", E=" << nodes.e
           << ", R=" << nodes.r << " [rule]\n";
}

void write_candidate_count(std::ostringstream& output, const Report& report) {
    const std::size_t count = report.candidates.size();
    output << "Candidates: " << count << " of " << report.position.legal_moves
           << " legal moves (engine's top " << count << ')';
    if (report.position.legal_moves > 0 &&
        count < static_cast<std::size_t>(report.position.legal_moves)) {
        output << "; unlisted moves are not judged and may also be fine";
    }
    output << " [rule]\n";
}

void write_candidate_eval(std::ostringstream& output, const MoveAssessment& move) {
    output << std::fixed << std::setprecision(1) << static_cast<double>(move.eval.es_units) / 20.0;
}

void write_walk(std::ostringstream& output, const std::string& label, const MoveAssessment& move) {
    output << label << " facts read to ply " << move.walked_plies << " of " << move.line_san.size()
           << " [rule]\n";
}

void write_motif_label(std::ostringstream& output, const Motif& motif) {
    switch (motif.tag) {
    case MotifTag::PinAbsolute:
        output << "restricted movement (absolute pin)";
        return;
    case MotifTag::PinRelative:
        output << "restricted movement (relative pin)";
        return;
    case MotifTag::OverloadedDefender:
        output << "restricted movement (overloaded defender)";
        return;
    case MotifTag::PinTactical: {
        if (!motif.threat.has_value()) {
            throw std::bad_optional_access{};
        }
        const MotifThreat& threat = *motif.threat;
        char actor_piece = motif.actor.piece;
        if (actor_piece >= 'a' && actor_piece <= 'z') {
            actor_piece = static_cast<char>(actor_piece - ('a' - 'A'));
        }
        std::string kind = to_string(threat.kind);
        for (char& character : kind) {
            if (character == '_') {
                character = ' ';
            }
        }
        output << "restricted movement (tactic: " << actor_piece;
        write_square(output, motif.actor.square);
        output << '-';
        write_square(output, threat.square);
        output << ' ' << kind << ')';
        return;
    }
    default:
        output << to_string(motif.tag);
        return;
    }
}

void write_motifs(std::ostringstream& output, const std::vector<Motif>& motifs) {
    for (const auto& motif : motifs) {
        write_motif_label(output, motif);
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
            write_restriction(output, *motif.restriction);
        }
        output << " (ply " << motif.ply;
        if (motif.contribution) {
            output << ", " << to_string(*motif.contribution);
        }
        output << ") [rule]\n";
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

void write_tactic(std::ostringstream& output, const std::optional<TacticJudgement>& tactic) {
    if (!tactic) {
        return;
    }
    if (tactic->forced) {
        output << "forced: only legal move [rule]\n";
        return;
    }
    if (!tactic->is_tactic) {
        return;
    }

    if (!tactic->alternative.has_value() || !tactic->margin_units.has_value()) {
        return;
    }
    const TacticAlternative& alternative = tactic->alternative.value();
    const int margin_units = tactic->margin_units.value();
    const auto flags = output.flags();
    const auto precision = output.precision();
    output << "tactic: next best " << alternative.san << " is " << std::fixed
           << std::setprecision(2) << static_cast<double>(margin_units) / 2000.0
           << " worse [engine]\n";
    output.flags(flags);
    output.precision(precision);
    write_tactic_labels(output, tactic->labels);
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
    write_motifs(output, threat->motifs);
    write_consequence(output, label + " consequence", threat->consequence);
}

void write_positional_changes(std::ostringstream& output,
                              const std::vector<PositionalChange>& changes) {
    for (const auto& change : changes) {
        const PositionalFeature& feature = change.feature;
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
        output << ") [rule]\n";
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

void write_move_details(std::ostringstream& output, const std::string& label,
                        const MoveAssessment& move, const GamesInfo& games, Side side_to_move) {
    output << label << " line: " << san_line(move.line_san) << " [engine]\n";
    write_walk(output, label, move);
    write_positional_changes(output, move.positional);
    write_game_move_counts(output, move, games);
    write_tactic(output, move.tactic);
    if (move.verdict == Verdict::Fine) {
        write_motifs(output, move.motifs);
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
    if (move.refutation) {
        output << label << " refutation line: " << san_line(move.refutation->line_san)
               << " [engine]\n";
        write_tactic(output, move.refutation->tactic);
        write_motifs(output, move.refutation->motifs);
        write_consequence(output, label + " refutation consequence", move.refutation->consequence);
    } else {
        output << label << " refutation: null [rule]\n";
    }
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

    write_candidate_count(output, report);
    for (std::size_t index = 0; index < report.candidates.size(); ++index) {
        const auto& candidate = report.candidates[index];
        const std::string label = "Candidate " + std::to_string(index + 1);
        output << label << ": " << candidate.san << " (" << candidate.uci << "), "
               << to_string(candidate.verdict) << ", es=";
        write_candidate_eval(output, candidate);
        output << "%, loss=" << std::fixed << std::setprecision(1)
               << static_cast<double>(candidate.loss_units) / 20.0 << "%, depth=" << candidate.depth
               << " [engine]\n";
        write_move_details(output, label, candidate, report.games, report.position.side_to_move);
    }

    if (report.asked_move) {
        output << "Asked move: " << report.asked_move->san << " (" << report.asked_move->uci
               << "), " << to_string(report.asked_move->verdict)
               << ", in_multipv: " << boolean(report.asked_move->in_multipv) << " [engine]\n";
        write_move_details(output, "Asked move", *report.asked_move, report.games,
                           report.position.side_to_move);
    } else {
        output << "Asked move: null [rule]\n";
    }

    output << "Opening dataset: " << report.opening.dataset << " [book]\n";
    if (report.opening.current) {
        output << "Current opening: " << report.opening.current->eco << ' '
               << report.opening.current->name << " [book]\n";
    } else {
        output << "Current opening: null [book]\n";
    }
    if (report.opening.last_named) {
        output << "Last named opening: " << report.opening.last_named->opening.eco << ' '
               << report.opening.last_named->opening.name
               << ", ply=" << report.opening.last_named->ply << " [book]\n";
    } else {
        output << "Last named opening: null [book]\n";
    }
    write_games_summary(output, report.games);
    return output.str();
}

} // namespace chesswhy::render
