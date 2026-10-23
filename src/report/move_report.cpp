#include "chesswhy/move_report.hpp"

#include "chesswhy/positional.hpp"

#include <algorithm>
#include <ranges>
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
    std::vector<Motif> result;
    result.reserve(static_cast<std::size_t>(std::ranges::count_if(motifs, relevant)));
    for (const auto& motif : motifs) {
        if (relevant(motif)) {
            result.push_back(motif);
        }
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
    result.consequence = assessment.consequence;
    result.is_tactic = assessment.is_tactic;
    result.allows_tactic = assessment.allows_tactic;
    result.opportunity = assessment.opportunity;
    result.motifs = relevant_motifs(assessment.motifs);
    result.root_motifs = relevant_motifs(assessment.root_motifs);
    result.tactic_labels = assessment.tactic_labels;

    for (const auto& change : std::views::reverse(assessment.positional)) {
        if (!change.holds_at_end ||
            std::ranges::any_of(result.positional, [&change](const PositionalChange& later) {
                return positional::same_identity(later.feature, change.feature);
            })) {
            continue;
        }
        result.positional.push_back(change);
    }
    std::ranges::reverse(result.positional);

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

    int games = 0;
    if (has_games_for_move(report.games, assessment.uci, games)) {
        result.games = games;
    }
    return result;
}

} // namespace

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
