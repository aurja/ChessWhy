#include "chesswhy/render.hpp"

#include <cstdint>
#include <iomanip>
#include <locale>
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

void write_motifs(std::ostringstream& output, const std::vector<Motif>& motifs) {
    for (const auto& motif : motifs) {
        output << to_string(motif.tag) << ": " << motif.actor.piece << ' ';
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
            output << ", gain " << *motif.gain;
        }
        output << " (ply " << motif.ply << ") [rule]\n";
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
    write_motifs(output, threat->motifs);
    write_consequence(output, label + " consequence", threat->consequence);
}

void write_move_details(std::ostringstream& output, const std::string& label,
                        const MoveAssessment& move) {
    output << label << " line: " << san_line(move.line_san) << " [engine]\n";
    if (move.verdict == Verdict::Fine) {
        write_motifs(output, move.motifs);
    }
    if (move.parries_threat) {
        output << label << " parries threat: " << boolean(move.parries_threat->parries) << " ("
               << to_string(move.parries_threat->test) << ") ["
               << parry_source(move.parries_threat->test) << "]\n";
    } else {
        output << label << " parries threat: null [rule]\n";
    }
    if (move.creates_threat) {
        output << label << " creates threat side: " << to_string(move.creates_threat->side)
               << " [engine]\n";
        output << label << " creates threat line: " << san_line(move.creates_threat->line_san)
               << " [engine]\n";
        write_motifs(output, move.creates_threat->motifs);
        write_consequence(output, label + " creates threat consequence",
                          move.creates_threat->consequence);
    } else {
        output << label << " creates threat: null [rule]\n";
    }
    if (move.refutation) {
        output << label << " refutation line: " << san_line(move.refutation->line_san)
               << " [engine]\n";
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

    for (std::size_t index = 0; index < report.candidates.size(); ++index) {
        const auto& candidate = report.candidates[index];
        const std::string label = "Candidate " + std::to_string(index + 1);
        output << label << ": " << candidate.san << " (" << candidate.uci << "), "
               << to_string(candidate.verdict) << ", loss=" << std::fixed << std::setprecision(1)
               << static_cast<double>(candidate.loss_units) / 20.0 << "% [engine]\n";
        write_move_details(output, label, candidate);
    }

    if (report.asked_move) {
        output << "Asked move: " << report.asked_move->san << " (" << report.asked_move->uci
               << "), " << to_string(report.asked_move->verdict)
               << ", in_multipv: " << boolean(report.asked_move->in_multipv) << " [engine]\n";
        write_move_details(output, "Asked move", *report.asked_move);
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
    return output.str();
}

} // namespace chesswhy::render
