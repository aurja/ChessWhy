#include "chesswhy/report.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace chesswhy {
namespace {

using Json = nlohmann::ordered_json;

template <typename Enum>
[[nodiscard]] std::string invalid_enum(const char* type, Enum value) {
    return "invalid " + std::string(type) + " value " +
           std::to_string(static_cast<unsigned int>(value));
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
    if (feature.file) {
        value["file"] = std::string(1, static_cast<char>('a' + *feature.file));
    } else {
        value["file"] = nullptr;
    }
    value["value"] = feature.value ? Json(*feature.value) : Json(nullptr);
}

[[nodiscard]] Json serialize_positional_feature(const PositionalFeature& feature) {
    Json value = Json::object();
    value["source"] = "rule";
    serialize_positional_feature_fields(value, feature);
    return value;
}

[[nodiscard]] Json serialize_positional_features(const std::vector<PositionalFeature>& features) {
    Json values = Json::array();
    for (const auto& feature : features) {
        values.push_back(serialize_positional_feature(feature));
    }
    return values;
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

[[nodiscard]] Json serialize_positional_changes(const std::vector<PositionalChange>& changes) {
    Json values = Json::array();
    for (const auto& change : changes) {
        values.push_back(serialize_positional_change(change));
    }
    return values;
}

[[nodiscard]] Json serialize_game_source(const GameSourceCounts& source) {
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
    Json moves = Json::array();
    for (const auto& move : source.moves) {
        Json item = Json::object();
        item["san"] = move.san;
        item["uci"] = move.uci;
        item["games"] = move.games;
        moves.push_back(std::move(item));
    }
    value["moves"] = std::move(moves);
    return value;
}

[[nodiscard]] Json serialize_games(const GamesInfo& games) {
    Json value = Json::object();
    value["source"] = "games";
    Json by_source = Json::array();
    for (const auto& source : games.by_source) {
        by_source.push_back(serialize_game_source(source));
    }
    value["by_source"] = std::move(by_source);
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
[[nodiscard]] Json serialize_tactic_labels(const std::vector<TacticLabel>& values) {
    Json labels = Json::array();
    for (const auto& label : values) {
        labels.push_back(serialize_tactic_label(label));
    }
    return labels;
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

[[nodiscard]] Json serialize_opening_name(const OpeningName& opening) {
    Json value = Json::object();
    value["eco"] = opening.eco;
    value["name"] = opening.name;
    return value;
}

[[nodiscard]] Json serialize_move(const MoveAssessment& move) {
    Json value = Json::object();
    value["source"] = "engine";
    value["san"] = move.san;
    value["uci"] = move.uci;
    value["eval"] = serialize_eval(move.eval);
    value["depth"] = move.depth;
    value["loss"] = static_cast<double>(move.loss_units) / 2000.0;
    value["verdict"] = to_string(move.verdict);
    value["in_multipv"] = move.in_multipv;
    value["gives_check"] = move.gives_check;
    value["line"] = move.line_san;
    value["walked_plies"] = move.walked_plies;
    value["consequence"] = serialize_consequence(move.consequence);
    value["is_tactic"] = move.is_tactic;
    value["allows_tactic"] = move.allows_tactic;
    value["detailed"] = move.detailed;
    if (move.parries_threat) {
        Json parry = Json::object();
        parry["source"] = parry_source(move.parries_threat->test);
        parry["value"] = move.parries_threat->parries;
        parry["test"] = to_string(move.parries_threat->test);
        value["parries_threat"] = std::move(parry);
    } else {
        value["parries_threat"] = nullptr;
    }
    value["creates_threat"] =
        move.creates_threat ? serialize_threat(*move.creates_threat) : Json(nullptr);
    value["creates_threat_skipped"] =
        move.creates_threat_skipped ? Json(to_string(*move.creates_threat_skipped)) : Json(nullptr);
    value["refutation"] = move.refutation ? serialize_threat(*move.refutation) : Json(nullptr);
    value["motifs"] = serialize_motifs(move.motifs);
    value["opportunity"] = serialize_optional_opportunity(move.opportunity);
    value["tactic_labels"] = serialize_tactic_labels(move.tactic_labels);
    value["root_motifs"] = serialize_motifs(move.root_motifs);
    value["positional"] = serialize_positional_changes(move.positional);
    return value;
}

} // namespace

std::string to_string(Side side) {
    switch (side) {
    case Side::White:
        return "white";
    case Side::Black:
        return "black";
    }
    throw std::invalid_argument(invalid_enum("Side", side));
}

std::string to_string(Budget budget) {
    switch (budget) {
    case Budget::Quick:
        return "quick";
    case Budget::Standard:
        return "standard";
    case Budget::Deep:
        return "deep";
    }
    throw std::invalid_argument(invalid_enum("Budget", budget));
}

std::string to_string(Level level) {
    switch (level) {
    case Level::Beginner:
        return "beginner";
    case Level::Club:
        return "club";
    case Level::Advanced:
        return "advanced";
    }
    throw std::invalid_argument(invalid_enum("Level", level));
}

std::string to_string(InputSource source) {
    switch (source) {
    case InputSource::Fen:
        return "fen";
    case InputSource::Pgn:
        return "pgn";
    }
    throw std::invalid_argument(invalid_enum("InputSource", source));
}

std::string to_string(Terminal terminal) {
    switch (terminal) {
    case Terminal::Checkmate:
        return "checkmate";
    case Terminal::Stalemate:
        return "stalemate";
    }
    throw std::invalid_argument(invalid_enum("Terminal", terminal));
}

std::string to_string(ConsequenceClass cls) {
    switch (cls) {
    case ConsequenceClass::Mate:
        return "mate";
    case ConsequenceClass::Material:
        return "material";
    case ConsequenceClass::Promotion:
        return "promotion";
    case ConsequenceClass::Positional:
        return "positional";
    }
    throw std::invalid_argument(invalid_enum("ConsequenceClass", cls));
}

std::string to_string(Verdict verdict) {
    switch (verdict) {
    case Verdict::Best:
        return "best";
    case Verdict::Okay:
        return "okay";
    case Verdict::Inaccuracy:
        return "inaccuracy";
    case Verdict::Mistake:
        return "mistake";
    case Verdict::Blunder:
        return "blunder";
    }
    throw std::invalid_argument(invalid_enum("Verdict", verdict));
}

std::string to_string(UrgencyRule rule) {
    switch (rule) {
    case UrgencyRule::InCheck:
        return "in_check";
    case UrgencyRule::Quiet:
        return "quiet";
    case UrgencyRule::ConsequenceClass:
        return "consequence_class";
    case UrgencyRule::Counterattack:
        return "counterattack";
    case UrgencyRule::Parry:
        return "parry";
    }
    throw std::invalid_argument(invalid_enum("UrgencyRule", rule));
}

std::string to_string(ParryTest test) {
    switch (test) {
    case ParryTest::Searched:
        return "searched";
    case ParryTest::ThreatMoveIllegal:
        return "threat_move_illegal";
    case ParryTest::DeferredByCheck:
        return "deferred_by_check";
    }
    throw std::invalid_argument(invalid_enum("ParryTest", test));
}

std::string to_string(CreatesThreatSkip skip) {
    switch (skip) {
    case CreatesThreatSkip::GivesCheck:
        return "gives_check";
    case CreatesThreatSkip::GameOver:
        return "game_over";
    case CreatesThreatSkip::NoLegalMove:
        return "no_legal_move";
    }
    throw std::invalid_argument(invalid_enum("CreatesThreatSkip", skip));
}

std::string to_string(HeldTo::Kind kind) {
    switch (kind) {
    case HeldTo::Kind::File:
        return "file";
    case HeldTo::Kind::Rank:
        return "rank";
    case HeldTo::Kind::Diagonal:
        return "diagonal";
    case HeldTo::Kind::Square:
        return "square";
    case HeldTo::Kind::Squares:
        return "squares";
    }
    throw std::invalid_argument(invalid_enum("HeldTo::Kind", kind));
}

std::string to_string(MotifTag tag) {
    switch (tag) {
    case MotifTag::HangingPiece:
        return "hanging_piece";
    case MotifTag::PinAbsolute:
        return "pin_absolute";
    case MotifTag::PinRelative:
        return "pin_relative";
    case MotifTag::BackRankWeakness:
        return "back_rank_weakness";
    case MotifTag::OverloadedDefender:
        return "overloaded_defender";
    case MotifTag::TrappedPiece:
        return "trapped_piece";
    case MotifTag::PinTactical:
        return "pin_tactical";
    case MotifTag::Fork:
        return "fork";
    case MotifTag::Skewer:
        return "skewer";
    case MotifTag::DiscoveredCheck:
        return "discovered_check";
    case MotifTag::DiscoveredAttack:
        return "discovered_attack";
    case MotifTag::DoubleCheck:
        return "double_check";
    case MotifTag::BackRankMate:
        return "back_rank_mate";
    case MotifTag::Promotion:
        return "promotion";
    }
    throw std::invalid_argument(invalid_enum("MotifTag", tag));
}

std::string to_string(MotifContribution contribution) {
    switch (contribution) {
    case MotifContribution::Direct:
        return "direct";
    case MotifContribution::Indirect:
        return "indirect";
    case MotifContribution::None:
        return "none";
    }
    throw std::invalid_argument(invalid_enum("MotifContribution", contribution));
}

std::string to_string(ThreatKind kind) {
    switch (kind) {
    case ThreatKind::Fork:
        return "fork";
    case ThreatKind::Skewer:
        return "skewer";
    case ThreatKind::DiscoveredCheck:
        return "discovered_check";
    case ThreatKind::DiscoveredAttack:
        return "discovered_attack";
    case ThreatKind::DoubleCheck:
        return "double_check";
    case ThreatKind::BackRankMate:
        return "back_rank_mate";
    case ThreatKind::Promotion:
        return "promotion";
    case ThreatKind::Checkmate:
        return "checkmate";
    case ThreatKind::Capture:
        return "capture";
    }
    throw std::invalid_argument(invalid_enum("ThreatKind", kind));
}

std::string to_string(TacticLabelTag tag) {
    switch (tag) {
    case TacticLabelTag::RemovalOfDefender:
        return "removal_of_defender";
    }
    throw std::invalid_argument(invalid_enum("TacticLabelTag", tag));
}

std::string to_string(PositionalTag tag) {
    switch (tag) {
    case PositionalTag::IsolatedPawn:
        return "isolated_pawn";
    case PositionalTag::DoubledPawns:
        return "doubled_pawns";
    case PositionalTag::TripledPawns:
        return "tripled_pawns";
    case PositionalTag::BackwardPawn:
        return "backward_pawn";
    case PositionalTag::PassedPawn:
        return "passed_pawn";
    case PositionalTag::ConnectedPassedPawns:
        return "connected_passed_pawns";
    case PositionalTag::PawnIslands:
        return "pawn_islands";
    case PositionalTag::PawnChain:
        return "pawn_chain";
    case PositionalTag::CastlingKingside:
        return "castling_kingside";
    case PositionalTag::CastlingQueenside:
        return "castling_queenside";
    case PositionalTag::KingShieldGap:
        return "king_shield_gap";
    case PositionalTag::OpenFileNearKing:
        return "open_file_near_king";
    case PositionalTag::HalfOpenFileNearKing:
        return "half_open_file_near_king";
    case PositionalTag::KingZoneAttacker:
        return "king_zone_attacker";
    case PositionalTag::BishopPair:
        return "bishop_pair";
    case PositionalTag::RookOpenFile:
        return "rook_open_file";
    case PositionalTag::RookHalfOpenFile:
        return "rook_half_open_file";
    case PositionalTag::RookSeventh:
        return "rook_seventh";
    case PositionalTag::Outpost:
        return "outpost";
    case PositionalTag::BadBishop:
        return "bad_bishop";
    case PositionalTag::MobilityLead:
        return "mobility_lead";
    case PositionalTag::SpaceLead:
        return "space_lead";
    case PositionalTag::DevelopmentLead:
        return "development_lead";
    case PositionalTag::MaterialAhead:
        return "material_ahead";
    case PositionalTag::ExchangeUp:
        return "exchange_up";
    }
    throw std::invalid_argument(invalid_enum("PositionalTag", tag));
}

std::string to_string(PositionalChangeKind kind) {
    switch (kind) {
    case PositionalChangeKind::Appears:
        return "appears";
    case PositionalChangeKind::Disappears:
        return "disappears";
    }
    throw std::invalid_argument(invalid_enum("PositionalChangeKind", kind));
}

const char* parry_source(ParryTest test) {
    return test == ParryTest::Searched ? "engine" : "rule";
}

nlohmann::ordered_json to_json(const Report& report) {
    Json value = Json::object();
    value["schema_version"] = report.schema_version;
    value["document"] = "full";

    Json input = Json::object();
    input["source"] = to_string(report.input.source);
    input["fen"] = report.input.fen;
    input["pgn_game"] = report.input.pgn_game ? Json(*report.input.pgn_game) : Json(nullptr);
    input["pgn_ply"] = report.input.pgn_ply ? Json(*report.input.pgn_ply) : Json(nullptr);
    input["moves"] = report.input.moves ? Json(*report.input.moves) : Json(nullptr);
    Json selection = Json::object();
    Json keywords = Json::array();
    for (const auto keyword : report.input.selection.keywords) {
        switch (keyword) {
        case Selection::Keyword::Best:
            keywords.push_back("best");
            break;
        case Selection::Keyword::Playable:
            keywords.push_back("playable");
            break;
        case Selection::Keyword::Inaccuracies:
            keywords.push_back("inaccuracies");
            break;
        case Selection::Keyword::Mistakes:
            keywords.push_back("mistakes");
            break;
        case Selection::Keyword::Blunders:
            keywords.push_back("blunders");
            break;
        case Selection::Keyword::Unplayable:
            keywords.push_back("unplayable");
            break;
        case Selection::Keyword::All:
            keywords.push_back("all");
            break;
        }
    }
    selection["keywords"] = std::move(keywords);
    selection["top"] =
        report.input.selection.top ? Json(*report.input.selection.top) : Json(nullptr);
    selection["moves"] = report.input.selection.moves;
    input["selection"] = std::move(selection);
    input["level"] = to_string(report.input.level);
    input["budget"] = to_string(report.input.budget);
    value["input"] = std::move(input);
    Json engine = Json::object();
    engine["name"] = report.engine.name;
    engine["threads"] = report.engine.threads;
    engine["hash_mb"] = report.engine.hash_mb;
    engine["multipv"] = report.engine.multipv;
    engine["reproducible"] = report.engine.reproducible;
    Json nodes = Json::object();
    nodes["A"] = report.engine.nodes.a;
    nodes["B"] = report.engine.nodes.b;
    nodes["C"] = report.engine.nodes.c;
    nodes["D"] = report.engine.nodes.d;
    nodes["E"] = report.engine.nodes.e;
    nodes["R"] = report.engine.nodes.r;
    engine["nodes"] = std::move(nodes);
    engine["nodes_searched"] = report.engine.nodes_searched;
    value["engine"] = std::move(engine);

    Json position = Json::object();
    position["fen"] = report.position.fen;
    position["side_to_move"] = to_string(report.position.side_to_move);
    position["in_check"] = report.position.in_check;
    position["legal_moves"] = report.position.legal_moves;
    position["terminal"] =
        report.position.terminal ? Json(to_string(*report.position.terminal)) : Json(nullptr);
    position["eval_white"] =
        report.position.eval_white ? serialize_eval(*report.position.eval_white) : Json(nullptr);
    position["motifs"] = serialize_motifs(report.position.motifs);
    position["positional"] = serialize_positional_features(report.position.positional);
    value["position"] = std::move(position);

    if (report.threats) {
        Json threats = Json::object();
        threats["white"] =
            report.threats->white ? serialize_threat(*report.threats->white) : Json(nullptr);
        threats["black"] =
            report.threats->black ? serialize_threat(*report.threats->black) : Json(nullptr);
        Json null_move_reliable = Json::object();
        null_move_reliable["source"] = "rule";
        null_move_reliable["value"] = report.threats->null_move_reliable
                                          ? Json(*report.threats->null_move_reliable)
                                          : Json(nullptr);
        threats["null_move_reliable"] = std::move(null_move_reliable);
        Json urgency = Json::object();
        urgency["source"] = "rule";
        urgency["more_urgent"] = report.threats->urgency.more_urgent
                                     ? Json(to_string(*report.threats->urgency.more_urgent))
                                     : Json(nullptr);
        urgency["rule"] = to_string(report.threats->urgency.rule);
        threats["urgency"] = std::move(urgency);
        value["threats"] = std::move(threats);
    } else {
        value["threats"] = nullptr;
    }

    Json candidates = Json::array();
    for (const auto& candidate : report.candidates) {
        candidates.push_back(serialize_move(candidate));
    }
    value["candidates"] = std::move(candidates);
    value["playable_truncated"] = report.playable_truncated;
    Json asked_moves = Json::array();
    for (const auto& asked_move : report.asked_moves) {
        asked_moves.push_back(serialize_move(asked_move));
    }
    value["asked_moves"] = std::move(asked_moves);

    Json opening = Json::object();
    opening["source"] = "book";
    opening["dataset"] = report.opening.dataset;
    opening["current"] =
        report.opening.current ? serialize_opening_name(*report.opening.current) : Json(nullptr);
    if (report.opening.last_named) {
        Json last_named = serialize_opening_name(report.opening.last_named->opening);
        last_named["ply"] = report.opening.last_named->ply;
        opening["last_named"] = std::move(last_named);
    } else {
        opening["last_named"] = nullptr;
    }
    value["opening"] = std::move(opening);
    value["games"] = serialize_games(report.games);
    return value;
}

} // namespace chesswhy
