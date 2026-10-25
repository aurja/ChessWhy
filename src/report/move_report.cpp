#include "chesswhy/move_report.hpp"

#include "chesswhy/analysis.hpp"
#include "chesswhy/positional.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace chesswhy {
namespace {

using Json = nlohmann::ordered_json;

[[nodiscard]] std::string invalid_enum(const char* type, unsigned int value) {
    return "invalid " + std::string(type) + " value " + std::to_string(value);
}

[[nodiscard]] std::string selection_keyword(Selection::Keyword keyword) {
    switch (keyword) {
    case Selection::Keyword::Best:
        return "best";
    case Selection::Keyword::Playable:
        return "playable";
    case Selection::Keyword::Inaccuracies:
        return "inaccuracies";
    case Selection::Keyword::Mistakes:
        return "mistakes";
    case Selection::Keyword::Blunders:
        return "blunders";
    case Selection::Keyword::Unplayable:
        return "unplayable";
    case Selection::Keyword::All:
        return "all";
    }
    throw std::invalid_argument(
        invalid_enum("Selection::Keyword", static_cast<unsigned int>(keyword)));
}

[[nodiscard]] bool playable(Verdict verdict) {
    return verdict == Verdict::Best || verdict == Verdict::Okay;
}

[[nodiscard]] bool selected_by_keyword(Selection::Keyword keyword, const MoveAssessment& move,
                                       std::size_t rank) {
    switch (keyword) {
    case Selection::Keyword::Best:
        return rank == 0;
    case Selection::Keyword::Playable:
        return playable(move.verdict);
    case Selection::Keyword::Inaccuracies:
        return move.verdict == Verdict::Inaccuracy;
    case Selection::Keyword::Mistakes:
        return move.verdict == Verdict::Mistake;
    case Selection::Keyword::Blunders:
        return move.verdict == Verdict::Blunder;
    case Selection::Keyword::Unplayable:
        return !playable(move.verdict);
    case Selection::Keyword::All:
        return true;
    }
    throw std::invalid_argument(
        invalid_enum("Selection::Keyword", static_cast<unsigned int>(keyword)));
}

[[nodiscard]] bool selected_from_a(const Selection& selection, const MoveAssessment& move,
                                   std::size_t index) {
    if (std::ranges::find(selection.moves, move.uci) != selection.moves.end() ||
        (selection.top && index < static_cast<std::size_t>(*selection.top))) {
        return true;
    }
    return std::ranges::any_of(selection.keywords, [&move, index](Selection::Keyword keyword) {
        return selected_by_keyword(keyword, move, index);
    });
}

[[nodiscard]] std::vector<Motif> relevant_motifs(const std::vector<Motif>& motifs) {
    const auto relevant = [](const Motif& motif) {
        return motif.contribution && *motif.contribution != MotifContribution::None;
    };
    const auto restricted_movement = [](MotifTag tag) {
        switch (tag) {
        case MotifTag::PinAbsolute:
        case MotifTag::PinRelative:
        case MotifTag::OverloadedDefender:
        case MotifTag::PinTactical:
            return true;
        default:
            return false;
        }
    };
    const auto repeats = [&restricted_movement](const Motif& first, const Motif& later) {
        return restricted_movement(first.tag) && restricted_movement(later.tag) &&
               first.side == later.side && first.ply == later.ply && first.actor == later.actor &&
               first.targets == later.targets && first.restriction == later.restriction &&
               first.contribution == later.contribution;
    };
    std::vector<Motif> result;
    result.reserve(static_cast<std::size_t>(std::ranges::count_if(motifs, relevant)));
    for (const auto& motif : motifs) {
        if (!relevant(motif)) {
            continue;
        }
        Motif projected = motif;
        projected.also.clear();
        if (restricted_movement(projected.tag)) {
            const auto first =
                std::ranges::find_if(result, [&projected, &repeats](const Motif& entry) {
                    return repeats(entry, projected);
                });
            if (first != result.end()) {
                first->also.push_back(MotifLabel{.tag = projected.tag, .threat = projected.threat});
                continue;
            }
        }
        result.push_back(std::move(projected));
    }
    return result;
}

[[nodiscard]] bool has_games_for_move(const GamesInfo& games, std::string_view uci, int& result) {
    if (games.by_source.empty() || !games.by_source.front().found) {
        return false;
    }
    const auto& moves = games.by_source.front().moves;
    const auto found = std::ranges::find(moves, uci, &GameMoveCount::uci);
    if (found == moves.end()) {
        return false;
    }
    result = found->games;
    return true;
}

[[nodiscard]] bool has_created_threat_consequence(ConsequenceClass cls) {
    return cls == ConsequenceClass::Mate || cls == ConsequenceClass::Material ||
           cls == ConsequenceClass::Promotion;
}

[[nodiscard]] Threat projected_threat(const Threat& threat, bool keep_tactic_facts) {
    Threat result;
    result.side = threat.side;
    result.root_fen = threat.root_fen;
    result.line_san = threat.line_san;
    result.consequence = threat.consequence;
    if (keep_tactic_facts) {
        result.motifs = relevant_motifs(threat.motifs);
        result.is_tactic = threat.is_tactic;
        result.opportunity = threat.opportunity;
        result.tactic_labels = threat.tactic_labels;
    }
    return result;
}

[[nodiscard]] Json serialize_tempo_value(const TempoValue& tempo) {
    Json value = Json::object();
    value["side"] = to_string(tempo.side);
    value["moves"] = static_cast<double>(tempo.moves_units) / 2000.0;
    value["passes"] = static_cast<double>(tempo.passes_units) / 2000.0;
    value["value"] = static_cast<double>(tempo.value_units) / 2000.0;
    value["label"] = tempo.label ? Json(to_string(*tempo.label)) : Json(nullptr);
    value["source"] = "engine";
    return value;
}

[[nodiscard]] Json serialize_eval(const Eval& eval) {
    Json value = Json::object();
    value["source"] = "engine";
    value["es"] = static_cast<double>(eval.es_units) / 2000.0;
    value["cp"] = eval.cp ? Json(*eval.cp) : Json(nullptr);
    value["mate"] = eval.mate ? Json(*eval.mate) : Json(nullptr);
    return value;
}

[[nodiscard]] Json serialize_piece_on_square(const PieceOnSquare& piece) {
    Json value = Json::object();
    std::string square(2, ' ');
    square[0] = static_cast<char>('a' + (piece.square % 8));
    square[1] = static_cast<char>('1' + (piece.square / 8));
    value["square"] = std::move(square);
    value["piece"] = std::string(1, piece.piece);
    return value;
}

[[nodiscard]] Json serialize_motif_threat(const MotifThreat& threat) {
    Json value = Json::object();
    value["kind"] = to_string(threat.kind);
    std::string square(2, ' ');
    square[0] = static_cast<char>('a' + (threat.square % 8));
    square[1] = static_cast<char>('1' + (threat.square / 8));
    value["square"] = std::move(square);
    return value;
}

[[nodiscard]] Json serialize_motif_restriction(const MotifRestriction& restriction) {
    Json value = Json::object();
    value["piece"] = serialize_piece_on_square(restriction.piece);
    value["restricted"] = restriction.restricted;
    value["free"] = restriction.free;
    Json held_to = Json::object();
    held_to["kind"] = to_string(restriction.held_to.kind);
    held_to["name"] = restriction.held_to.name;
    value["held_to"] = std::move(held_to);
    return value;
}

[[nodiscard]] Json serialize_motif(const Motif& motif) {
    Json value = Json::object();
    value["source"] = "rule";
    value["tag"] = to_string(motif.tag);
    value["side"] = to_string(motif.side);
    value["ply"] = motif.ply;
    value["actor"] = serialize_piece_on_square(motif.actor);
    Json targets = Json::array();
    for (const auto& target : motif.targets) {
        targets.push_back(serialize_piece_on_square(target));
    }
    value["targets"] = std::move(targets);
    value["gain"] = motif.gain ? Json(*motif.gain) : Json(nullptr);
    value["threat"] = motif.threat ? serialize_motif_threat(*motif.threat) : Json(nullptr);
    value["restriction"] =
        motif.restriction ? serialize_motif_restriction(*motif.restriction) : Json(nullptr);
    value["contribution"] =
        motif.contribution ? Json(to_string(*motif.contribution)) : Json(nullptr);
    Json also = Json::array();
    for (const MotifLabel& label : motif.also) {
        Json item = Json::object();
        item["tag"] = to_string(label.tag);
        item["threat"] = label.threat ? serialize_motif_threat(*label.threat) : Json(nullptr);
        also.push_back(std::move(item));
    }
    value["also"] = std::move(also);
    return value;
}

[[nodiscard]] Json serialize_motifs(const std::vector<Motif>& motifs) {
    Json values = Json::array();
    for (const auto& motif : motifs) {
        values.push_back(serialize_motif(motif));
    }
    return values;
}

void serialize_positional_feature_fields(Json& value, const PositionalFeature& feature) {
    value["tag"] = to_string(feature.tag);
    value["side"] = to_string(feature.side);
    Json pieces = Json::array();
    for (const auto& piece : feature.pieces) {
        pieces.push_back(serialize_piece_on_square(piece));
    }
    value["pieces"] = std::move(pieces);
    value["file"] =
        feature.file ? Json(std::string(1, static_cast<char>('a' + *feature.file))) : Json(nullptr);
    value["value"] = feature.value ? Json(*feature.value) : Json(nullptr);
}

[[nodiscard]] Json serialize_positional_change(const PositionalChange& change) {
    Json value = Json::object();
    value["source"] = "rule";
    value["ply"] = change.ply;
    value["change"] = to_string(change.change);
    value["holds_at_end"] = change.holds_at_end;
    value["net_change"] = change.net_change;
    value["captured"] = change.captured;
    value["in_use"] = change.in_use ? Json(*change.in_use) : Json(nullptr);
    serialize_positional_feature_fields(value, change.feature);
    return value;
}

[[nodiscard]] Json serialize_tactic_label(const TacticLabel& label) {
    Json value = Json::object();
    value["source"] = "rule";
    value["tag"] = to_string(label.tag);
    value["side"] = to_string(label.side);
    value["ply"] = label.ply;
    value["actor"] = serialize_piece_on_square(label.actor);
    Json targets = Json::array();
    for (const auto& target : label.targets) {
        targets.push_back(serialize_piece_on_square(target));
    }
    value["targets"] = std::move(targets);
    return value;
}

[[nodiscard]] Json serialize_tactic_labels(const std::vector<TacticLabel>& labels) {
    Json result = Json::array();
    for (const auto& label : labels) {
        result.push_back(serialize_tactic_label(label));
    }
    return result;
}

[[nodiscard]] Json serialize_tempo_gained(const TempoGained& tempo) {
    Json value = Json::object();
    value["ply"] = tempo.ply;
    value["san"] = tempo.san;
    value["side"] = to_string(tempo.side);
    value["source"] = "engine";
    return value;
}

[[nodiscard]] Json serialize_initiative_run(const InitiativeRun& run) {
    Json value = Json::object();
    value["holder"] = to_string(run.holder);
    value["start_ply"] = run.start_ply;
    value["end_ply"] = run.end_ply;
    value["tempi"] = run.tempi;
    value["ending"] = to_string(run.ending);
    value["outcome"] = to_string(run.outcome);
    value["source"] = "rule";
    return value;
}

[[nodiscard]] std::string to_string(ImpactGroup group) {
    switch (group) {
    case ImpactGroup::ParriedThreat:
        return "parried_threat";
    case ImpactGroup::CreatedThreat:
        return "created_threat";
    case ImpactGroup::Motifs:
        return "motifs";
    case ImpactGroup::Tempo:
        return "tempo";
    case ImpactGroup::PositionalDynamic:
        return "positional_dynamic";
    case ImpactGroup::PositionalStatic:
        return "positional_static";
    }
    throw std::invalid_argument(invalid_enum("ImpactGroup", static_cast<unsigned int>(group)));
}

[[nodiscard]] Json serialize_opportunity(const OpportunityJudgement& opportunity) {
    Json value = Json::object();
    value["source"] = opportunity.forced ? "rule" : "engine";
    value["forced"] = opportunity.forced;
    if (opportunity.alternative) {
        Json alternative = Json::object();
        alternative["san"] = opportunity.alternative->san;
        alternative["uci"] = opportunity.alternative->uci;
        alternative["eval"] = serialize_eval(opportunity.alternative->eval);
        value["alternative"] = std::move(alternative);
    } else {
        value["alternative"] = nullptr;
    }
    value["margin"] = opportunity.margin_units
                          ? Json(static_cast<double>(*opportunity.margin_units) / 2000.0)
                          : Json(nullptr);
    value["is_opportunity"] = opportunity.is_opportunity;
    return value;
}

[[nodiscard]] Json
serialize_optional_opportunity(const std::optional<OpportunityJudgement>& opportunity) {
    return opportunity ? serialize_opportunity(*opportunity) : Json(nullptr);
}

[[nodiscard]] Json serialize_consequence(const Consequence& consequence) {
    Json value = Json::object();
    value["source"] = "rule";
    value["class"] = to_string(consequence.cls);
    value["mate_in"] = consequence.mate_in ? Json(*consequence.mate_in) : Json(nullptr);
    value["material"] = consequence.material;
    value["promotes"] = consequence.promotes;
    value["plies"] = consequence.plies;
    if (consequence.terminal_draw) {
        Json terminal_draw = Json::object();
        terminal_draw["kind"] = to_string(consequence.terminal_draw->kind);
        terminal_draw["ply"] = consequence.terminal_draw->ply;
        value["terminal_draw"] = std::move(terminal_draw);
    } else {
        value["terminal_draw"] = nullptr;
    }
    value["settled"] = consequence.settled;
    return value;
}

[[nodiscard]] Json serialize_threat(const Threat& threat) {
    Json value = Json::object();
    value["source"] = "engine";
    value["side"] = to_string(threat.side);
    value["root_fen"] = threat.root_fen;
    value["line"] = threat.line_san;
    value["consequence"] = serialize_consequence(threat.consequence);
    value["motifs"] = serialize_motifs(threat.motifs);
    value["is_tactic"] = threat.is_tactic ? Json(*threat.is_tactic) : Json(nullptr);
    value["opportunity"] = serialize_optional_opportunity(threat.opportunity);
    value["tactic_labels"] = serialize_tactic_labels(threat.tactic_labels);
    return value;
}

[[nodiscard]] Json serialize_move(const MoveReport& move) {
    Json value = Json::object();
    value["rank"] = move.rank ? Json(*move.rank) : Json(nullptr);
    value["san"] = move.san;
    value["uci"] = move.uci;
    value["eval"] = serialize_eval(move.eval);
    value["depth"] = move.depth;
    value["loss"] = static_cast<double>(move.loss_units) / 2000.0;
    value["verdict"] = to_string(move.verdict);
    value["gives_check"] = move.gives_check;
    value["line"] = move.line_san;
    value["walked_plies"] = move.walked_plies;
    value["line_fens"] = move.line_fens;
    value["consequence"] = serialize_consequence(move.consequence);
    value["is_tactic"] = move.is_tactic;
    value["allows_tactic"] = move.allows_tactic;
    value["opportunity"] = serialize_optional_opportunity(move.opportunity);
    value["motifs"] = serialize_motifs(move.motifs);
    value["root_motifs"] = serialize_motifs(move.root_motifs);
    value["tactic_labels"] = serialize_tactic_labels(move.tactic_labels);
    Json positional = Json::array();
    for (const auto& change : move.positional) {
        positional.push_back(serialize_positional_change(change));
    }
    value["positional"] = std::move(positional);
    value["tempo_value"] =
        move.tempo_value ? serialize_tempo_value(*move.tempo_value) : Json(nullptr);
    if (move.tempo_gained) {
        Json gained = Json::array();
        for (const auto& tempo : *move.tempo_gained) {
            gained.push_back(serialize_tempo_gained(tempo));
        }
        value["tempo_gained"] = std::move(gained);
    } else {
        value["tempo_gained"] = nullptr;
    }
    if (move.initiative) {
        Json runs = Json::array();
        for (const auto& run : *move.initiative) {
            runs.push_back(serialize_initiative_run(run));
        }
        value["initiative"] = std::move(runs);
    } else {
        value["initiative"] = nullptr;
    }
    Json impact_order = Json::array();
    for (const ImpactGroup group : move.impact_order) {
        impact_order.push_back(to_string(group));
    }
    value["impact_order"] = std::move(impact_order);
    if (move.parried_threat) {
        Json parried = Json::object();
        parried["threat"] = serialize_threat(move.parried_threat->threat);
        parried["test"] = to_string(move.parried_threat->test);
        value["parried_threat"] = std::move(parried);
    } else {
        value["parried_threat"] = nullptr;
    }
    value["created_threat"] =
        move.created_threat ? serialize_threat(*move.created_threat) : Json(nullptr);
    value["refutation"] = move.refutation ? serialize_threat(*move.refutation) : Json(nullptr);
    value["refutation_skipped"] =
        move.refutation_skipped ? Json(to_string(*move.refutation_skipped)) : Json(nullptr);
    value["games"] = move.games ? Json(*move.games) : Json(nullptr);
    return value;
}

[[nodiscard]] Json serialize_selection(const Selection& selection) {
    Json value = Json::object();
    Json keywords = Json::array();
    for (const auto keyword : selection.keywords) {
        keywords.push_back(selection_keyword(keyword));
    }
    value["keywords"] = std::move(keywords);
    value["top"] = selection.top ? Json(*selection.top) : Json(nullptr);
    value["moves"] = selection.moves;
    return value;
}

[[nodiscard]] Json serialize_input(const InputInfo& input) {
    Json value = Json::object();
    value["source"] = to_string(input.source);
    value["fen"] = input.fen;
    value["pgn_game"] = input.pgn_game ? Json(*input.pgn_game) : Json(nullptr);
    value["pgn_ply"] = input.pgn_ply ? Json(*input.pgn_ply) : Json(nullptr);
    value["moves"] = input.moves ? Json(*input.moves) : Json(nullptr);
    value["selection"] = serialize_selection(input.selection);
    value["level"] = to_string(input.level);
    value["budget"] = to_string(input.budget);
    return value;
}

[[nodiscard]] Json serialize_engine(const EngineInfo& engine) {
    Json value = Json::object();
    value["name"] = engine.name;
    value["threads"] = engine.threads;
    value["hash_mb"] = engine.hash_mb;
    value["multipv"] = engine.multipv;
    value["reproducible"] = engine.reproducible;
    Json nodes = Json::object();
    nodes["A"] = engine.nodes.a;
    nodes["B"] = engine.nodes.b;
    nodes["C"] = engine.nodes.c;
    nodes["D"] = engine.nodes.d;
    nodes["E"] = engine.nodes.e;
    nodes["L"] = engine.nodes.l;
    nodes["R"] = engine.nodes.r;
    value["nodes"] = std::move(nodes);
    value["nodes_searched"] = engine.nodes_searched;
    return value;
}

[[nodiscard]] Json serialize_position(const PositionSummary& position) {
    Json value = Json::object();
    value["fen"] = position.fen;
    value["side_to_move"] = to_string(position.side_to_move);
    value["in_check"] = position.in_check;
    value["legal_moves"] = position.legal_moves;
    value["terminal"] = position.terminal ? Json(to_string(*position.terminal)) : Json(nullptr);
    value["eval_white"] =
        position.eval_white ? serialize_eval(*position.eval_white) : Json(nullptr);
    value["tempo_value"] =
        position.tempo_value ? serialize_tempo_value(*position.tempo_value) : Json(nullptr);
    return value;
}

[[nodiscard]] Json serialize_opening_name(const OpeningName& opening) {
    Json value = Json::object();
    value["eco"] = opening.eco;
    value["name"] = opening.name;
    return value;
}

[[nodiscard]] Json serialize_opening(const OpeningInfo& opening) {
    Json value = Json::object();
    value["source"] = "book";
    value["dataset"] = opening.dataset;
    value["current"] = opening.current ? serialize_opening_name(*opening.current) : Json(nullptr);
    if (opening.last_named) {
        Json last_named = serialize_opening_name(opening.last_named->opening);
        last_named["ply"] = opening.last_named->ply;
        value["last_named"] = std::move(last_named);
    } else {
        value["last_named"] = nullptr;
    }
    return value;
}

[[nodiscard]] Json serialize_game_source(const GameSourceSummary& source) {
    Json value = Json::object();
    value["id"] = source.id;
    value["table"] = source.table;
    value["platform"] = source.platform;
    value["players"] = source.players;
    value["months"] = source.months;
    value["credit"] = source.credit;
    value["max_ply"] = source.max_ply;
    value["min_games"] = source.min_games;
    value["found"] = source.found;
    value["games"] = source.games ? Json(*source.games) : Json(nullptr);
    value["white"] = source.white ? Json(*source.white) : Json(nullptr);
    value["draws"] = source.draws ? Json(*source.draws) : Json(nullptr);
    value["black"] = source.black ? Json(*source.black) : Json(nullptr);
    return value;
}

[[nodiscard]] Json serialize_games(const std::vector<GameSourceSummary>& games) {
    Json value = Json::object();
    value["source"] = "games";
    Json by_source = Json::array();
    for (const auto& source : games) {
        by_source.push_back(serialize_game_source(source));
    }
    value["by_source"] = std::move(by_source);
    return value;
}

[[nodiscard]] std::vector<std::string> replay_line_fens(const Report& report,
                                                        const MoveAssessment& assessment) {
    chess::Board board(report.position.fen);
    std::vector<std::string> fens;
    fens.reserve(static_cast<std::size_t>(assessment.walked_plies));
    for (int ply = 0; ply < assessment.walked_plies; ++ply) {
        const chess::Move move =
            chess::uci::parseSan(board, assessment.line_san.at(static_cast<std::size_t>(ply)));
        if (move == chess::Move::NO_MOVE) {
            throw std::invalid_argument("cannot replay move report SAN line");
        }
        board.makeMove(move);
        fens.push_back(board.getFen());
    }
    return fens;
}

[[nodiscard]] MoveReport assess_move(const Report& report, const MoveAssessment& assessment,
                                     std::optional<int> rank) {
    MoveReport result;
    result.rank = rank;
    result.san = assessment.san;
    result.uci = assessment.uci;
    result.eval = assessment.eval;
    result.depth = assessment.depth;
    result.loss_units = assessment.loss_units;
    result.verdict = assessment.verdict;
    result.gives_check = assessment.gives_check;
    result.line_san = assessment.line_san;
    result.walked_plies = assessment.walked_plies;
    result.line_fens = replay_line_fens(report, assessment);
    result.consequence = assessment.consequence;
    result.is_tactic = assessment.is_tactic;
    result.allows_tactic = assessment.allows_tactic;
    result.opportunity = assessment.opportunity;
    result.motifs = relevant_motifs(assessment.motifs);
    result.root_motifs = relevant_motifs(assessment.root_motifs);
    result.tactic_labels = assessment.tactic_labels;
    result.tempo_value = assessment.tempo_value;

    const bool tempo_dependent = analysis::tempo_dependent(report.position.tempo_value);
    if (assessment.tempo_ledger) {
        std::vector<TempoGained> tempo_gained;
        tempo_gained.reserve(static_cast<std::size_t>(
            std::ranges::count_if(assessment.tempo_ledger->plies, &TempoPly::tempo_gained)));
        for (const TempoPly& ply : assessment.tempo_ledger->plies) {
            if (ply.tempo_gained) {
                tempo_gained.push_back(
                    TempoGained{.ply = ply.ply, .san = ply.san, .side = ply.side});
            }
        }
        result.tempo_gained = std::move(tempo_gained);
        result.initiative = assessment.tempo_ledger->runs;
    }

    const auto used_net_change = [](const PositionalChange& change) {
        return change.net_change && change.in_use == true;
    };
    result.positional.reserve(
        static_cast<std::size_t>(std::ranges::count_if(assessment.positional, used_net_change)));
    for (const auto& change : assessment.positional) {
        if (used_net_change(change)) {
            result.positional.push_back(change);
        }
    }
    positional::impact_order(result.positional, tempo_dependent);

    if (assessment.parries_threat && assessment.parries_threat->parries && report.threats) {
        const auto& opponent_threat = report.position.side_to_move == Side::White
                                          ? report.threats->black
                                          : report.threats->white;
        if (opponent_threat) {
            result.parried_threat = ParriedThreat{
                .threat = projected_threat(*opponent_threat, false),
                .test = assessment.parries_threat->test,
            };
        }
    }
    if (assessment.creates_threat &&
        has_created_threat_consequence(assessment.creates_threat->consequence.cls)) {
        result.created_threat = projected_threat(*assessment.creates_threat, false);
    }
    if (!playable(assessment.verdict) && assessment.refutation) {
        result.refutation = projected_threat(*assessment.refutation, true);
    }
    result.refutation_skipped = assessment.refutation_skipped;

    int games = 0;
    if (has_games_for_move(report.games, assessment.uci, games)) {
        result.games = games;
    }
    result.impact_order = impact_order(result, tempo_dependent);
    return result;
}

} // namespace

std::vector<ImpactGroup> impact_order(const MoveReport& move, bool tempo_dependent) {
    std::vector<ImpactGroup> groups;
    groups.reserve(6);
    if (move.parried_threat) {
        groups.push_back(ImpactGroup::ParriedThreat);
    }
    if (move.created_threat) {
        groups.push_back(ImpactGroup::CreatedThreat);
    }
    if (!move.motifs.empty() || !move.root_motifs.empty()) {
        groups.push_back(ImpactGroup::Motifs);
    }
    const bool has_tempo = move.tempo_value || (move.tempo_gained && !move.tempo_gained->empty()) ||
                           (move.initiative && !move.initiative->empty());
    const bool has_dynamic =
        std::ranges::any_of(move.positional, [](const PositionalChange& change) {
            return positional::is_dynamic(change.feature.tag);
        });
    const bool has_static =
        std::ranges::any_of(move.positional, [](const PositionalChange& change) {
            return !positional::is_dynamic(change.feature.tag);
        });
    if (tempo_dependent) {
        if (has_tempo) {
            groups.push_back(ImpactGroup::Tempo);
        }
        if (has_dynamic) {
            groups.push_back(ImpactGroup::PositionalDynamic);
        }
        if (has_static) {
            groups.push_back(ImpactGroup::PositionalStatic);
        }
    } else {
        if (has_static) {
            groups.push_back(ImpactGroup::PositionalStatic);
        }
        if (has_dynamic) {
            groups.push_back(ImpactGroup::PositionalDynamic);
        }
        if (has_tempo) {
            groups.push_back(ImpactGroup::Tempo);
        }
    }

    const auto rank = [&move](ImpactGroup group) {
        switch (group) {
        case ImpactGroup::ParriedThreat:
            return analysis::consequence_rank(move.parried_threat->threat.consequence);
        case ImpactGroup::CreatedThreat:
            return analysis::consequence_rank(move.created_threat->consequence);
        case ImpactGroup::Motifs:
            return analysis::consequence_rank(move.consequence);
        case ImpactGroup::Tempo:
        case ImpactGroup::PositionalDynamic:
        case ImpactGroup::PositionalStatic:
            return 0;
        }
        return 0;
    };
    for (std::size_t index = 1; index < groups.size(); ++index) {
        const ImpactGroup group = groups[index];
        std::size_t position = index;
        while (position > 0 && rank(group) > rank(groups[position - 1])) {
            groups[position] = groups[position - 1];
            --position;
        }
        groups[position] = group;
    }
    return groups;
}

MoveReportDocument move_reports(const Report& report) {
    MoveReportDocument document;
    document.schema_version = kSchemaVersion;
    document.input = report.input;
    document.engine = report.engine;
    document.position = PositionSummary{
        .fen = report.position.fen,
        .side_to_move = report.position.side_to_move,
        .in_check = report.position.in_check,
        .legal_moves = report.position.legal_moves,
        .terminal = report.position.terminal,
        .eval_white = report.position.eval_white,
        .tempo_value = report.position.tempo_value,
    };
    document.selection.selection = report.input.selection;
    document.selection.lines_searched = report.position.terminal ? 0 : report.engine.multipv;
    document.selection.playable_count = static_cast<int>(std::ranges::count_if(
        report.candidates, [](const MoveAssessment& move) { return playable(move.verdict); }));
    document.selection.playable_truncated = report.playable_truncated;
    document.opening = report.opening;
    document.games.reserve(report.games.by_source.size());
    for (const auto& source : report.games.by_source) {
        document.games.push_back(GameSourceSummary{
            .id = source.id,
            .table = source.table,
            .platform = source.platform,
            .players = source.players,
            .months = source.months,
            .credit = source.credit,
            .max_ply = source.max_ply,
            .min_games = source.min_games,
            .found = source.found,
            .games = source.games,
            .white = source.white,
            .draws = source.draws,
            .black = source.black,
        });
    }

    if (report.position.terminal) {
        return document;
    }

    document.moves.reserve(report.candidates.size() + report.asked_moves.size());
    for (std::size_t index = 0; index < report.candidates.size(); ++index) {
        const auto& candidate = report.candidates[index];
        if (selected_from_a(report.input.selection, candidate, index) &&
            std::ranges::find(document.moves, candidate.uci, &MoveReport::uci) ==
                document.moves.end()) {
            document.moves.push_back(assess_move(report, candidate, static_cast<int>(index + 1)));
        }
    }
    for (const auto& uci : report.input.selection.moves) {
        if (std::ranges::find(document.moves, uci, &MoveReport::uci) != document.moves.end()) {
            continue;
        }
        const auto found = std::ranges::find(report.asked_moves, uci, &MoveAssessment::uci);
        if (found == report.asked_moves.end()) {
            continue;
        }
        document.moves.push_back(assess_move(report, *found, std::nullopt));
    }
    return document;
}

nlohmann::ordered_json to_json(const MoveReportDocument& document) {
    Json value = Json::object();
    value["schema_version"] = document.schema_version;
    value["document"] = "moves";
    value["input"] = serialize_input(document.input);
    value["engine"] = serialize_engine(document.engine);
    value["position"] = serialize_position(document.position);

    Json selection = serialize_selection(document.selection.selection);
    selection["lines_searched"] = document.selection.lines_searched;
    selection["playable_count"] = document.selection.playable_count;
    selection["playable_truncated"] = document.selection.playable_truncated;
    value["selection"] = std::move(selection);
    value["opening"] = serialize_opening(document.opening);
    value["games"] = serialize_games(document.games);
    Json moves = Json::array();
    for (const auto& move : document.moves) {
        moves.push_back(serialize_move(move));
    }
    value["moves"] = std::move(moves);
    return value;
}

} // namespace chesswhy
