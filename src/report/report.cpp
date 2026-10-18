#include "chesswhy/report.hpp"

#include <stdexcept>
#include <string>
#include <utility>

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
    value["loss"] = static_cast<double>(move.loss_units) / 2000.0;
    value["verdict"] = to_string(move.verdict);
    value["in_multipv"] = move.in_multipv;
    value["gives_check"] = move.gives_check;
    value["line"] = move.line_san;
    if (move.parries_threat) {
        Json parry = Json::object();
        parry["source"] = "engine";
        parry["value"] = move.parries_threat->parries;
        parry["test"] = to_string(move.parries_threat->test);
        value["parries_threat"] = std::move(parry);
    } else {
        value["parries_threat"] = nullptr;
    }
    value["creates_threat"] =
        move.creates_threat ? serialize_threat(*move.creates_threat) : Json(nullptr);
    value["refutation"] = move.refutation ? serialize_threat(*move.refutation) : Json(nullptr);
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
    case Verdict::Fine:
        return "fine";
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

nlohmann::ordered_json to_json(const Report& report) {
    Json value = Json::object();
    value["schema_version"] = report.schema_version;

    Json input = Json::object();
    input["source"] = to_string(report.input.source);
    input["fen"] = report.input.fen;
    input["pgn_game"] = report.input.pgn_game ? Json(*report.input.pgn_game) : Json(nullptr);
    input["pgn_ply"] = report.input.pgn_ply ? Json(*report.input.pgn_ply) : Json(nullptr);
    input["move"] = report.input.move ? Json(*report.input.move) : Json(nullptr);
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
    engine["nodes"] = std::move(nodes);
    engine["nodes_searched"] = report.engine.nodes_searched;
    value["engine"] = std::move(engine);

    Json position = Json::object();
    position["fen"] = report.position.fen;
    position["side_to_move"] = to_string(report.position.side_to_move);
    position["in_check"] = report.position.in_check;
    position["terminal"] =
        report.position.terminal ? Json(to_string(*report.position.terminal)) : Json(nullptr);
    position["eval_white"] =
        report.position.eval_white ? serialize_eval(*report.position.eval_white) : Json(nullptr);
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
    value["fine_truncated"] = report.fine_truncated;
    value["asked_move"] = report.asked_move ? serialize_move(*report.asked_move) : Json(nullptr);

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
    return value;
}

} // namespace chesswhy
