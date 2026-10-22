#!/usr/bin/env python3
"""Validate ChessWhy report JSON files against schema and semantic relations."""

from __future__ import annotations

import argparse
import json
import sys
from collections.abc import Iterable, Sequence
from pathlib import Path
from typing import Protocol, TypeAlias, cast

import chess
from jsonschema import Draft202012Validator
from jsonschema.exceptions import SchemaError, ValidationError

JsonValue: TypeAlias = (
    None | bool | int | float | str | list["JsonValue"] | dict[str, "JsonValue"]
)
JsonSchema: TypeAlias = bool | dict[str, JsonValue]
JsonObject: TypeAlias = dict[str, JsonValue]
Failure: TypeAlias = tuple[str, str]

RESTRICTED_TAGS = frozenset(
    {"pin_absolute", "pin_relative", "overloaded_defender", "pin_tactical"}
)
VERDICT_THRESHOLDS = (60, 260, 540)


class Arguments(argparse.Namespace):
    def __init__(self) -> None:
        super().__init__()
        self.schema: Path = Path()
        self.reports: list[Path] = []


class ErrorValidator(Protocol):
    def iter_errors(self, instance: JsonValue) -> Iterable[ValidationError]: ...


def read_json(filename: Path) -> JsonValue:
    contents = filename.read_text(encoding="utf-8")
    return cast(JsonValue, json.loads(contents))


def json_pointer(parts: Iterable[object]) -> str:
    """Format a location as a JSON Pointer (the empty string denotes the root)."""
    return "".join(
        "/" + str(part).replace("~", "~0").replace("/", "~1") for part in parts
    )


def report_failure(filename: Path, location: str, message: str) -> None:
    single_line = " ".join(message.splitlines())
    print(f"{filename}: {location}: {single_line}", file=sys.stderr)


def error_key(error: ValidationError) -> tuple[tuple[str, ...], str]:
    path = cast(Iterable[object], error.absolute_path)
    return tuple(str(part) for part in path), error.message


def validation_errors(
    validator: Draft202012Validator, instance: JsonValue
) -> list[ValidationError]:
    typed_validator = cast(ErrorValidator, validator)
    errors = list(typed_validator.iter_errors(instance))
    errors.sort(key=error_key)
    return errors


def nested_validation_errors(
    errors: Iterable[ValidationError],
) -> list[ValidationError]:
    expanded: list[ValidationError] = []

    def add_tree(error: ValidationError) -> None:
        if schema_error_message(error) != error.message or not error.context:
            expanded.append(error)
            return
        for child in error.context:
            add_tree(child)

    for error in errors:
        add_tree(error)
    expanded.sort(key=error_key)
    return expanded


def as_object(value: JsonValue) -> JsonObject | None:
    return value if isinstance(value, dict) else None


def as_array(value: JsonValue) -> list[JsonValue] | None:
    return value if isinstance(value, list) else None


def get_object(value: JsonValue, key: str) -> JsonObject | None:
    object_value = as_object(value)
    return None if object_value is None else as_object(object_value.get(key))


def fail(
    failures: list[Failure], pointer: str, relation: str, condition: bool
) -> None:
    if not condition:
        failures.append((pointer, relation))


def score_units(value: JsonValue) -> int | None:
    eval_object = as_object(value)
    if eval_object is None:
        return None
    raw_score = eval_object.get("es")
    if isinstance(raw_score, bool) or not isinstance(raw_score, (int, float)):
        return None
    return round(float(raw_score) * 2000)


def number_units(value: JsonValue) -> int | None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    return round(float(value) * 2000)


def verdict_for_loss(loss_units: int) -> str:
    if loss_units <= VERDICT_THRESHOLDS[0]:
        return "fine"
    if loss_units < VERDICT_THRESHOLDS[1]:
        return "inaccuracy"
    if loss_units < VERDICT_THRESHOLDS[2]:
        return "mistake"
    return "blunder"


def side_name(color: chess.Color) -> str:
    return "white" if color else "black"


def board_from_fen(
    fen: JsonValue, pointer: str, failures: list[Failure]
) -> chess.Board | None:
    if not isinstance(fen, str):
        fail(failures, pointer, "FEN must be parseable by python-chess", False)
        return None
    try:
        return chess.Board(fen)
    except ValueError:
        fail(failures, pointer, f"FEN {fen!r} must be parseable by python-chess", False)
        return None


def legal_san_move(board: chess.Board, san: JsonValue) -> chess.Move | None:
    if not isinstance(san, str):
        return None
    try:
        return board.parse_san(san)
    except ValueError:
        return None


def check_move_pair(
    board: chess.Board,
    value: JsonValue,
    pointer: str,
    failures: list[Failure],
) -> chess.Move | None:
    move_value = as_object(value)
    if move_value is None:
        return None
    san = move_value.get("san")
    uci = move_value.get("uci")
    san_move = legal_san_move(board, san)
    fail(
        failures,
        f"{pointer}/san",
        "SAN must name a legal move from this position",
        san_move is not None,
    )
    uci_move: chess.Move | None = None
    if isinstance(uci, str):
        try:
            uci_move = chess.Move.from_uci(uci)
        except ValueError:
            uci_move = None
    fail(
        failures,
        f"{pointer}/uci",
        "UCI must name a legal move from this position",
        uci_move is not None and board.is_legal(uci_move),
    )
    if san_move is not None and uci_move is not None:
        fail(
            failures,
            f"{pointer}/uci",
            "SAN and UCI must identify the same legal move",
            san_move == uci_move,
        )
    if uci_move is not None and board.is_legal(uci_move):
        return uci_move
    return san_move


def replay_san_line(
    board: chess.Board,
    line: JsonValue,
    pointer: str,
    failures: list[Failure],
) -> list[chess.Move] | None:
    line_values = as_array(line)
    if line_values is None:
        return None
    current = board.copy()
    moves: list[chess.Move] = []
    for index, san in enumerate(line_values):
        move = legal_san_move(current, san)
        fail(
            failures,
            f"{pointer}/{index}",
            "SAN must be legal from the position reached by the preceding line moves",
            move is not None,
        )
        if move is None:
            return None
        moves.append(move)
        current.push(move)
    return moves


def check_threat_legality(
    threat: JsonValue, pointer: str, failures: list[Failure]
) -> tuple[chess.Board | None, list[chess.Move] | None]:
    threat_object = as_object(threat)
    if threat_object is None:
        return None, None
    board = board_from_fen(threat_object.get("root_fen"), f"{pointer}/root_fen", failures)
    if board is None:
        return None, None
    moves = replay_san_line(board, threat_object.get("line"), f"{pointer}/line", failures)
    tactic = as_object(threat_object.get("tactic"))
    if tactic is not None:
        _ = check_move_pair(
            board,
            tactic.get("alternative"),
            f"{pointer}/tactic/alternative",
            failures,
        )
    return board, moves


def check_move_legality(
    move: JsonValue,
    pointer: str,
    root: chess.Board,
    failures: list[Failure],
) -> chess.Move | None:
    move_object = as_object(move)
    if move_object is None:
        return None
    first_move = check_move_pair(root, move, pointer, failures)
    line_moves = replay_san_line(root, move_object.get("line"), f"{pointer}/line", failures)
    if first_move is not None and line_moves:
        fail(
            failures,
            f"{pointer}/line/0",
            "the first line move must equal the assessed SAN/UCI move",
            first_move == line_moves[0],
        )
    tactic = as_object(move_object.get("tactic"))
    if tactic is not None:
        _ = check_move_pair(
            root,
            tactic.get("alternative"),
            f"{pointer}/tactic/alternative",
            failures,
        )
    for field in ("creates_threat", "refutation"):
        nested_threat = move_object.get(field)
        if isinstance(nested_threat, dict):
            _ = check_threat_legality(nested_threat, f"{pointer}/{field}", failures)
    return first_move


def check_score_relations(report: JsonObject, failures: list[Failure]) -> None:
    candidates = as_array(report.get("candidates")) or []
    asked_move = report.get("asked_move")
    assessed: list[tuple[str, JsonValue]] = [
        (f"/candidates/{index}", candidate)
        for index, candidate in enumerate(candidates)
    ]
    if isinstance(asked_move, dict):
        assessed.append(("/asked_move", asked_move))
    if not candidates:
        return
    best_units = score_units(get_object(candidates[0], "eval"))
    if best_units is None:
        return
    for pointer, value in assessed:
        move_object = as_object(value)
        if move_object is None:
            continue
        move_units = score_units(move_object.get("eval"))
        actual_loss = number_units(move_object.get("loss"))
        if move_units is None or actual_loss is None:
            continue
        expected_loss = max(0, best_units - move_units)
        fail(
            failures,
            f"{pointer}/loss",
            f"loss must equal max(0, candidate[0] ES units - move ES units) = {expected_loss}",
            actual_loss == expected_loss,
        )
        expected_verdict = verdict_for_loss(expected_loss)
        fail(
            failures,
            f"{pointer}/verdict",
            f"verdict must be {expected_verdict!r} for {expected_loss} loss units "
            + "(thresholds 60/260/540)",
            move_object.get("verdict") == expected_verdict,
        )


def check_position_relations(
    report: JsonObject, root: chess.Board | None, failures: list[Failure]
) -> None:
    position = get_object(report, "position")
    if position is None or root is None:
        return
    fail(
        failures,
        "/position/side_to_move",
        f"side_to_move must be {side_name(root.turn)!r} from position.fen",
        position.get("side_to_move") == side_name(root.turn),
    )
    fail(
        failures,
        "/position/in_check",
        f"in_check must be {root.is_check()} from position.fen",
        position.get("in_check") == root.is_check(),
    )
    fail(
        failures,
        "/position/legal_moves",
        f"legal_moves must be {root.legal_moves.count()} from position.fen",
        position.get("legal_moves") == root.legal_moves.count(),
    )


def check_terminal_relations(report: JsonObject, failures: list[Failure]) -> None:
    position = get_object(report, "position")
    if position is None or position.get("terminal") is None:
        return
    candidates = as_array(report.get("candidates")) or []
    fail(
        failures,
        "/candidates",
        "a terminal position must have no candidate moves",
        not candidates,
    )
    asked = as_object(report.get("asked_move"))
    if asked is not None:
        fail(
            failures,
            "/asked_move/refutation",
            "a terminal position must have no asked-move refutation search fields",
            asked.get("refutation") is None,
        )


def check_urgency(report: JsonObject, failures: list[Failure]) -> None:
    threats = get_object(report, "threats")
    urgency = get_object(report.get("threats"), "urgency")
    if threats is None or urgency is None:
        return
    quiet = urgency.get("rule") == "quiet"
    fail(
        failures,
        "/threats/urgency/more_urgent",
        "more_urgent must be null exactly when urgency.rule is quiet",
        (urgency.get("more_urgent") is None) == quiet,
    )


def check_parry_relations(
    report: JsonObject,
    root: chess.Board | None,
    failures: list[Failure],
) -> None:
    if root is None:
        return
    root_position = get_object(report, "position")
    threat_side = "black"
    if root_position is not None and root_position.get("side_to_move") == "black":
        threat_side = "white"
    opponent_threat = get_object(report.get("threats"), threat_side)
    moves = as_array(report.get("candidates")) or []
    move_pairs: list[tuple[str, JsonValue]] = [
        (f"/candidates/{index}", value) for index, value in enumerate(moves)
    ]
    if isinstance(report.get("asked_move"), dict):
        move_pairs.append(("/asked_move", report["asked_move"]))
    for pointer, value in move_pairs:
        move_object = as_object(value)
        if move_object is None:
            continue
        parry = as_object(move_object.get("parries_threat"))
        if parry is None:
            continue
        test = parry.get("test")
        expected_source = "engine" if test == "searched" else "rule"
        fail(
            failures,
            f"{pointer}/parries_threat/source",
            f"source must be {expected_source!r} for test {test!r}",
            parry.get("source") == expected_source,
        )
        if test not in ("threat_move_illegal", "searched"):
            continue
        opponent = as_object(opponent_threat)
        line = None if opponent is None else as_array(opponent.get("line"))
        threat_root = (
            None
            if opponent is None
            else board_from_fen(
                opponent.get("root_fen"),
                f"/threats/{threat_side}/root_fen",
                failures,
            )
        )
        first_threat_move = (
            None if not line or threat_root is None else legal_san_move(threat_root, line[0])
        )
        assessed_move = check_move_pair(root, value, pointer, failures)
        if first_threat_move is None or assessed_move is None:
            fail(
                failures,
                f"{pointer}/parries_threat/test",
                "parry legality tests require a resolvable first move of the opponent threat",
                False,
            )
            continue
        after_move = root.copy()
        after_move.push(assessed_move)
        threat_move_legal = after_move.is_legal(first_threat_move)
        if test == "threat_move_illegal":
            relation = "threat_move_illegal requires the threat's first move to be illegal"
            condition = not threat_move_legal
        else:
            relation = "searched requires the threat's first move to be legal"
            condition = threat_move_legal
        fail(
            failures,
            f"{pointer}/parries_threat/test",
            relation + " after the assessed move",
            condition,
        )


def check_threat_skip(
    move: JsonValue,
    pointer: str,
    root: chess.Board,
    failures: list[Failure],
) -> None:
    move_object = as_object(move)
    if move_object is None:
        return
    assessed_move = check_move_pair(root, move, pointer, failures)
    if assessed_move is None:
        return
    after_move = root.copy()
    after_move.push(assessed_move)
    if after_move.is_check():
        expected: str | None = "gives_check"
    elif not any(after_move.legal_moves):
        expected = "game_over"
    else:
        after_null = after_move.copy()
        after_null.push(chess.Move.null())
        expected = "no_legal_move" if not any(after_null.legal_moves) else None
    fail(
        failures,
        f"{pointer}/creates_threat_skipped",
        f"creates_threat_skipped must be {expected!r} from the board "
        + "(check, game over, then no legal move after the opponent's null move)",
        move_object.get("creates_threat_skipped") == expected,
    )


def piece_square(value: JsonValue) -> tuple[int, str] | None:
    piece_object = as_object(value)
    if piece_object is None:
        return None
    square = piece_object.get("square")
    piece = piece_object.get("piece")
    if not isinstance(square, str) or not isinstance(piece, str):
        return None
    try:
        return chess.parse_square(square), piece
    except ValueError:
        return None


def held_to_value(
    held_square: chess.Square, free_moves: list[str]
) -> dict[str, str]:
    destinations = sorted(
        {chess.square_name(chess.Move.from_uci(uci).to_square) for uci in free_moves}
    )
    if not destinations:
        return {"kind": "square", "name": chess.square_name(held_square)}
    squares = [chess.parse_square(destination) for destination in destinations]
    held_file = chess.square_file(held_square)
    held_rank = chess.square_rank(held_square)
    if all(chess.square_file(square) == held_file for square in squares):
        return {"kind": "file", "name": chr(ord("a") + held_file)}
    if all(chess.square_rank(square) == held_rank for square in squares):
        return {"kind": "rank", "name": str(held_rank + 1)}
    file_rank_constant = held_file - held_rank
    file_rank_diagonal = all(
        chess.square_file(square) - chess.square_rank(square) == file_rank_constant
        for square in squares
    )
    if file_rank_diagonal:
        low_file = max(0, file_rank_constant)
        high_file = min(7, 7 + file_rank_constant)
        edge_squares = (
            chess.square(file_index, file_index - file_rank_constant)
            for file_index in range(low_file, high_file + 1)
        )
        edge_names = sorted(chess.square_name(square) for square in edge_squares)
        return {"kind": "diagonal", "name": f"{edge_names[0]}-{edge_names[-1]}"}
    file_rank_sum = held_file + held_rank
    sum_diagonal = all(
        chess.square_file(square) + chess.square_rank(square) == file_rank_sum
        for square in squares
    )
    if sum_diagonal:
        low_file = max(0, file_rank_sum - 7)
        high_file = min(7, file_rank_sum)
        edge_squares = (
            chess.square(file_index, file_rank_sum - file_index)
            for file_index in range(low_file, high_file + 1)
        )
        edge_names = sorted(chess.square_name(square) for square in edge_squares)
        return {"kind": "diagonal", "name": f"{edge_names[0]}-{edge_names[-1]}"}
    return {"kind": "squares", "name": ",".join(destinations)}


def check_restriction(
    motif: JsonObject,
    restriction: JsonObject,
    board: chess.Board,
    pointer: str,
    failures: list[Failure],
) -> None:
    tag = motif.get("tag")
    actor = motif.get("actor")
    targets = as_array(motif.get("targets")) or []
    expected_piece = actor if tag == "overloaded_defender" else (
        targets[0] if targets else None
    )
    held_piece = piece_square(expected_piece)
    actual_piece = piece_square(restriction.get("piece"))
    if held_piece is None:
        fail(
            failures,
            f"{pointer}/piece",
            "held piece must be actor for overloaded_defender, otherwise targets[0]",
            False,
        )
        return
    held_square, held_symbol = held_piece
    actual_square = None if actual_piece is None else actual_piece[0]
    actual_symbol = None if actual_piece is None else actual_piece[1]
    held_piece_on_board = board.piece_at(held_square)
    fail(
        failures,
        f"{pointer}/piece",
        "restriction.piece must identify the held motif piece on its motif board",
        actual_square == held_square
        and actual_symbol == held_symbol
        and held_piece_on_board is not None
        and held_piece_on_board.symbol() == held_symbol,
    )
    motif_side = motif.get("side")
    if motif_side not in ("white", "black"):
        return
    x_turn = motif_side == "white"
    by = board.copy()
    if by.turn == x_turn:
        by.push(chess.Move.null())

    legal_moves = sorted(
        move.uci() for move in by.legal_moves if move.from_square == held_square
    )
    pseudo_moves = sorted(
        move.uci() for move in by.pseudo_legal_moves if move.from_square == held_square
    )
    restricted = as_array(restriction.get("restricted")) or []
    free = as_array(restriction.get("free")) or []
    restricted_uci = [value for value in restricted if isinstance(value, str)]
    free_uci = [value for value in free if isinstance(value, str)]
    fail(
        failures,
        f"{pointer}/restricted",
        "restricted UCI moves must be sorted and unique",
        restricted_uci == sorted(set(restricted_uci)),
    )
    fail(
        failures,
        f"{pointer}/free",
        "free UCI moves must be sorted and unique",
        free_uci == sorted(set(free_uci)),
    )
    fail(
        failures,
        f"{pointer}/restricted",
        "restricted and free moves must be disjoint",
        set(restricted_uci).isdisjoint(free_uci),
    )
    pin_move_sets: tuple[set[str], set[str]] | None = None
    if tag == "pin_absolute":
        actor_square_value = piece_square(actor)
        king_square = by.king(not x_turn)
        if actor_square_value is not None and king_square is not None:
            actor_square = actor_square_value[0]
            segment = chess.SquareSet(
                chess.between(actor_square, king_square)
                | chess.BB_SQUARES[actor_square]
                | chess.BB_SQUARES[king_square]
            )
            expected_restricted = {
                uci
                for uci in pseudo_moves
                if chess.Move.from_uci(uci).to_square not in segment
            }
            expected_free = set(legal_moves) - expected_restricted
            pin_move_sets = expected_restricted, expected_free
            expected_moves = expected_restricted | expected_free
        else:
            expected_moves = set(legal_moves)
    else:
        expected_moves = set(legal_moves)
    actual_moves = set(restricted_uci) | set(free_uci)
    fail(
        failures,
        pointer,
        "restricted and free must partition the held piece's moves on By "
        + f"(missing {sorted(expected_moves - actual_moves)}, "
        + f"unexpected {sorted(actual_moves - expected_moves)})",
        actual_moves == expected_moves,
    )
    if pin_move_sets is not None:
        expected_restricted, expected_free = pin_move_sets
        fail(
            failures,
            f"{pointer}/restricted",
            "pin_absolute restricted moves must be pseudo-legal destinations "
            + "off the actor-to-king segment",
            set(restricted_uci) == expected_restricted,
        )
        fail(
            failures,
            f"{pointer}/free",
            "free moves must be the remaining legal moves of the held piece",
            set(free_uci) == expected_free,
        )
    expected_held_to = held_to_value(held_square, free_uci)
    actual_held_to = as_object(restriction.get("held_to"))
    fail(
        failures,
        f"{pointer}/held_to",
        f"held_to must be {expected_held_to!r}, derived from free destinations and "
        + "the held piece's square",
        actual_held_to == expected_held_to,
    )


def check_motif_restrictions(
    motifs: JsonValue,
    root: chess.Board,
    line: JsonValue,
    pointer: str,
    failures: list[Failure],
) -> None:
    motif_values = as_array(motifs)
    if motif_values is None:
        return
    line_moves = replay_san_line(root, line, f"{pointer.rsplit('/motifs', 1)[0]}/line", failures)
    if line_moves is None:
        return
    for index, value in enumerate(motif_values):
        motif = as_object(value)
        if motif is None:
            continue
        motif_pointer = f"{pointer}/{index}"
        tag = motif.get("tag")
        restriction_value = motif.get("restriction")
        is_restricted_tag = tag in RESTRICTED_TAGS
        fail(
            failures,
            f"{motif_pointer}/restriction",
            "restriction must be set iff tag is pin_absolute, pin_relative, "
            + "overloaded_defender, or pin_tactical",
            (isinstance(restriction_value, dict)) == is_restricted_tag,
        )
        if not is_restricted_tag or not isinstance(restriction_value, dict):
            continue
        ply_value = motif.get("ply")
        if not isinstance(ply_value, int) or isinstance(ply_value, bool):
            continue
        fail(
            failures,
            f"{motif_pointer}/ply",
            "motif ply must have a corresponding board in its line",
            0 <= ply_value <= len(line_moves),
        )
        if ply_value < 0 or ply_value > len(line_moves):
            continue
        board = root.copy()
        for move in line_moves[:ply_value]:
            board.push(move)
        check_restriction(
            motif,
            cast(JsonObject, restriction_value),
            board,
            f"{motif_pointer}/restriction",
            failures,
        )


def check_restrictions_everywhere(
    report: JsonObject, root: chess.Board | None, failures: list[Failure]
) -> None:
    position = get_object(report, "position")
    if root is None or position is None:
        return
    check_motif_restrictions(
        position.get("motifs"), root, [], "/position/motifs", failures
    )
    for key in ("candidates", "asked_move"):
        values = report.get(key)
        if key == "candidates":
            moves = as_array(values) or []
            move_pairs = [
                (f"/candidates/{index}", value)
                for index, value in enumerate(moves)
            ]
        elif isinstance(values, dict):
            move_pairs = [("/asked_move", values)]
        else:
            move_pairs = []
        for pointer, move_value in move_pairs:
            move = as_object(move_value)
            if move is None:
                continue
            check_motif_restrictions(
                move.get("motifs"), root, move.get("line"), f"{pointer}/motifs", failures
            )
            for field in ("creates_threat", "refutation"):
                threat = as_object(move.get(field))
                if threat is None:
                    continue
                threat_root = board_from_fen(
                    threat.get("root_fen"), f"{pointer}/{field}/root_fen", failures
                )
                if threat_root is not None:
                    check_motif_restrictions(
                        threat.get("motifs"),
                        threat_root,
                        threat.get("line"),
                        f"{pointer}/{field}/motifs",
                        failures,
                    )
    threats = as_object(report.get("threats"))
    if threats is not None:
        for color in ("white", "black"):
            threat = as_object(threats.get(color))
            if threat is None:
                continue
            threat_root = board_from_fen(
                threat.get("root_fen"), f"/threats/{color}/root_fen", failures
            )
            if threat_root is not None:
                check_motif_restrictions(
                    threat.get("motifs"),
                    threat_root,
                    threat.get("line"),
                    f"/threats/{color}/motifs",
                    failures,
                )


def check_motif_contributions(
    motifs: JsonValue,
    expected: bool,
    pointer: str,
    failures: list[Failure],
) -> None:
    values = as_array(motifs)
    if values is None:
        return
    for index, value in enumerate(values):
        motif = as_object(value)
        if motif is None:
            continue
        contribution = motif.get("contribution")
        fail(
            failures,
            f"{pointer}/{index}/contribution",
            "contribution must be non-null exactly on a tactic line",
            (contribution is not None) == expected,
        )


def check_non_tactic_labels(
    tactic: JsonObject | None,
    expected_tactic: bool,
    pointer: str,
    failures: list[Failure],
) -> None:
    if tactic is None or expected_tactic:
        return
    labels = as_array(tactic.get("labels"))
    fail(
        failures,
        f"{pointer}/labels",
        "labels must be empty when is_tactic is false",
        labels is not None and not labels,
    )


def check_tactic_judgement(
    tactic_value: JsonValue,
    pointer: str,
    forced_expected: bool,
    expected_margin: int | None,
    expected_alternative: JsonObject | None,
    first_move_uci: str | None,
    failures: list[Failure],
) -> bool:
    tactic = as_object(tactic_value)
    fail(
        failures,
        pointer,
        "a judged tactic line must contain a judgement object",
        tactic is not None,
    )
    if tactic is None:
        return False
    expected_tactic = expected_margin is not None and expected_margin >= 260
    fail(
        failures,
        f"{pointer}/source",
        "tactic source must be rule iff the line is forced, otherwise engine",
        tactic.get("source") == ("rule" if forced_expected else "engine"),
    )
    fail(
        failures,
        f"{pointer}/forced",
        f"forced must be {forced_expected} from the number of legal root moves",
        tactic.get("forced") == forced_expected,
    )
    alternative = tactic.get("alternative")
    if forced_expected:
        fail(
            failures,
            f"{pointer}/alternative",
            "forced lines must have no alternative",
            alternative is None,
        )
        fail(
            failures,
            f"{pointer}/margin",
            "forced lines must have no margin",
            tactic.get("margin") is None,
        )
    else:
        actual_alternative = as_object(alternative)
        fail(
            failures,
            f"{pointer}/alternative",
            "non-forced lines must have an alternative object",
            actual_alternative is not None,
        )
        if actual_alternative is not None and expected_alternative is not None:
            for field in ("san", "uci", "eval"):
                fail(
                    failures,
                    f"{pointer}/alternative/{field}",
                    f"root alternative {field} must equal candidate[1].{field}",
                    actual_alternative.get(field) == expected_alternative.get(field),
                )
        if actual_alternative is not None and first_move_uci is not None:
            fail(
                failures,
                f"{pointer}/alternative/uci",
                "refutation alternative must differ from the refutation's first move",
                actual_alternative.get("uci") != first_move_uci,
            )
        actual_margin = number_units(tactic.get("margin"))
        fail(
            failures,
            f"{pointer}/margin",
            f"margin must equal {expected_margin} expected-score units",
            expected_margin is not None and actual_margin == expected_margin,
        )
    fail(
        failures,
        f"{pointer}/is_tactic",
        f"is_tactic must be {expected_tactic} for the computed margin "
        + "(threshold 260 units; forced lines are never tactics)",
        tactic.get("is_tactic") == expected_tactic,
    )
    check_non_tactic_labels(tactic, expected_tactic, pointer, failures)
    return expected_tactic


def check_refutation_tactic(
    move: JsonObject,
    pointer: str,
    failures: list[Failure],
) -> bool:
    refutation = as_object(move.get("refutation"))
    if refutation is None:
        return False
    line = as_array(refutation.get("line")) or []
    motifs_pointer = f"{pointer}/refutation/motifs"
    if not line:
        fail(
            failures,
            f"{pointer}/refutation/tactic",
            "a refutation with no plies must have no tactic judgement",
            refutation.get("tactic") is None,
        )
        check_motif_contributions(refutation.get("motifs"), False, motifs_pointer, failures)
        return False
    tactic_value = refutation.get("tactic")
    if tactic_value is None:
        check_motif_contributions(refutation.get("motifs"), False, motifs_pointer, failures)
        return False
    root = board_from_fen(
        refutation.get("root_fen"), f"{pointer}/refutation/root_fen", failures
    )
    if root is None:
        return False
    forced = root.legal_moves.count() == 1
    first_move = legal_san_move(root, line[0])
    first_uci = None if first_move is None else root.uci(first_move)
    tactic_object = as_object(tactic_value)
    if forced:
        margin = None
    else:
        move_units = score_units(move.get("eval"))
        alternative = (
            None if tactic_object is None else as_object(tactic_object.get("alternative"))
        )
        alternative_units = (
            None if alternative is None else score_units(alternative.get("eval"))
        )
        margin = (
            None
            if move_units is None or alternative_units is None
            else (2000 - move_units) - alternative_units
        )
    is_tactic = check_tactic_judgement(
        tactic_value,
        f"{pointer}/refutation/tactic",
        forced,
        margin,
        None,
        first_uci,
        failures,
    )
    check_motif_contributions(
        refutation.get("motifs"), is_tactic, motifs_pointer, failures
    )
    return is_tactic


def check_move_tactic_annotations(
    move_value: JsonValue,
    pointer: str,
    root_line: bool,
    root_tactic: bool,
    failures: list[Failure],
) -> None:
    move = as_object(move_value)
    if move is None:
        return
    check_motif_contributions(
        move.get("motifs"),
        root_line and root_tactic,
        f"{pointer}/motifs",
        failures,
    )
    created = as_object(move.get("creates_threat"))
    if created is not None:
        check_motif_contributions(
            created.get("motifs"), False, f"{pointer}/creates_threat/motifs", failures
        )
        fail(
            failures,
            f"{pointer}/creates_threat/tactic",
            "created-threat lines are not tactic-judged",
            created.get("tactic") is None,
        )
    _ = check_refutation_tactic(move, pointer, failures)


def check_tactic_invariants(
    report: JsonObject, root: chess.Board | None, failures: list[Failure]
) -> None:
    position = get_object(report, "position")
    if position is None:
        return
    terminal = position.get("terminal") is not None
    candidates = as_array(report.get("candidates")) or []
    root_tactic = False
    if not terminal:
        legal_moves = root.legal_moves.count() if root is not None else 0
        first = as_object(candidates[0]) if candidates else None
        first_tactic = None if first is None else as_object(first.get("tactic"))
        if first is None:
            fail(
                failures,
                "/candidates/0/tactic",
                "a non-terminal root must have a best candidate with a tactic judgement",
                False,
            )
        elif first_tactic is None:
            fail(
                failures,
                "/candidates/0/tactic",
                "a non-terminal root must have a tactic judgement object",
                False,
            )
        elif legal_moves == 1:
            root_tactic = check_tactic_judgement(
                first_tactic,
                "/candidates/0/tactic",
                True,
                None,
                None,
                None,
                failures,
            )
        else:
            second = as_object(candidates[1]) if len(candidates) >= 2 else None
            expected_margin: int | None = None
            expected_alternative: JsonObject | None = None
            if second is None:
                fail(
                    failures,
                    "/candidates/1",
                    "a non-forced root must have a second candidate",
                    False,
                )
            else:
                expected_alternative = {
                    key: second[key]
                    for key in ("san", "uci", "eval")
                    if key in second
                }
                first_units = score_units(first.get("eval"))
                second_units = score_units(second.get("eval"))
                if first_units is not None and second_units is not None:
                    expected_margin = first_units - second_units
            root_tactic = check_tactic_judgement(
                first_tactic,
                "/candidates/0/tactic",
                False,
                expected_margin,
                expected_alternative,
                None,
                failures,
            )
    for index, candidate_value in enumerate(candidates):
        candidate = as_object(candidate_value)
        if candidate is None:
            continue
        if index > 0:
            fail(
                failures,
                f"/candidates/{index}/tactic",
                "only candidates[0] has a root tactic judgement",
                candidate.get("tactic") is None,
            )
        check_move_tactic_annotations(
            candidate,
            f"/candidates/{index}",
            index == 0,
            root_tactic,
            failures,
        )
    check_motif_contributions(
        position.get("motifs"), root_tactic, "/position/motifs", failures
    )
    threats = as_object(report.get("threats"))
    if threats is not None:
        own_side = position.get("side_to_move")
        for color in ("white", "black"):
            threat = as_object(threats.get(color))
            if threat is None:
                continue
            check_motif_contributions(
                threat.get("motifs"),
                color == own_side and root_tactic,
                f"/threats/{color}/motifs",
                failures,
            )
            fail(
                failures,
                f"/threats/{color}/tactic",
                "root threat lines are not tactic-judged",
                threat.get("tactic") is None,
            )
    asked = as_object(report.get("asked_move"))
    if asked is not None:
        matching_index: int | None = None
        matching_candidate: JsonObject | None = None
        asked_uci = asked.get("uci")
        for index, candidate_value in enumerate(candidates):
            candidate = as_object(candidate_value)
            if candidate is not None and candidate.get("uci") == asked_uci:
                matching_index = index
                matching_candidate = candidate
                break
        expected_tactic = (
            None if matching_candidate is None else matching_candidate.get("tactic")
        )
        fail(
            failures,
            "/asked_move/tactic",
            "asked_move tactic must copy its candidate judgement, or be null outside MultiPV",
            asked.get("tactic") == expected_tactic,
        )
        check_move_tactic_annotations(
            asked,
            "/asked_move",
            matching_index == 0,
            root_tactic,
            failures,
        )


def check_all_semantics(report_value: JsonValue, failures: list[Failure]) -> None:
    report = as_object(report_value)
    if report is None:
        return
    position = get_object(report, "position")
    root = (
        None
        if position is None
        else board_from_fen(position.get("fen"), "/position/fen", failures)
    )
    check_score_relations(report, failures)
    check_position_relations(report, root, failures)
    check_terminal_relations(report, failures)
    check_urgency(report, failures)
    check_parry_relations(report, root, failures)
    if root is not None:
        for index, value in enumerate(as_array(report.get("candidates")) or []):
            check_threat_skip(value, f"/candidates/{index}", root, failures)
        asked = report.get("asked_move")
        if isinstance(asked, dict):
            check_threat_skip(asked, "/asked_move", root, failures)
        for index, value in enumerate(as_array(report.get("candidates")) or []):
            _ = check_move_legality(value, f"/candidates/{index}", root, failures)
        asked = report.get("asked_move")
        if isinstance(asked, dict):
            _ = check_move_legality(asked, "/asked_move", root, failures)
        candidates = as_array(report.get("candidates")) or []
        if isinstance(asked, dict):
            asked_uci = asked.get("uci")
            for index, candidate_value in enumerate(candidates):
                candidate = as_object(candidate_value)
                if candidate is None or candidate.get("uci") != asked_uci:
                    continue
                fields = sorted(set(candidate) | set(asked))
                for field in fields:
                    fail(
                        failures,
                        f"/asked_move/{field.replace('~', '~0').replace('/', '~1')}",
                        f"asked_move field {field!r} must equal candidates[{index}]",
                        asked.get(field) == candidate.get(field),
                    )
                break
        games = get_object(report, "games")
        sources = [] if games is None else as_array(games.get("by_source")) or []
        for source_index, source_value in enumerate(sources):
            source = as_object(source_value)
            if source is None:
                continue
            for move_index, game_move in enumerate(as_array(source.get("moves")) or []):
                _ = check_move_pair(
                    root,
                    game_move,
                    f"/games/by_source/{source_index}/moves/{move_index}",
                    failures,
                )
    for color in ("white", "black"):
        threats = as_object(report.get("threats"))
        threat = None if threats is None else threats.get(color)
        if isinstance(threat, dict):
            _ = check_threat_legality(threat, f"/threats/{color}", failures)
    check_tactic_invariants(report, root, failures)
    check_restrictions_everywhere(report, root, failures)


def schema_error_message(error: ValidationError) -> str:
    path = tuple(str(part) for part in cast(Iterable[object], error.absolute_path))
    instance = error.instance
    if (
        error.validator == "oneOf"
        and isinstance(instance, dict)
        and {"cp", "mate"} <= cast(dict[str, object], instance).keys()
    ):
        return "eval must set exactly one of cp and mate"
    if path and path[-1] == "mate_in" and "consequence" in path:
        return "consequence.mate_in must be set iff consequence.class is mate"
    if path and path[-1] == "refutation":
        return "refutation must be set iff verdict is not fine"
    if path and path[-1] == "more_urgent" and "urgency" in path:
        return "more_urgent must be null iff urgency.rule is quiet"
    if path and path[-1] == "source" and "parries_threat" in path:
        return "parries_threat.source must be engine iff test is searched, otherwise rule"
    return error.message


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="validate report JSON files against a Draft 2020-12 schema"
    )
    _ = parser.add_argument("schema", type=Path)
    _ = parser.add_argument("reports", type=Path, nargs="+")
    arguments = parser.parse_args(argv, namespace=Arguments())

    try:
        schema_value = read_json(arguments.schema)
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        report_failure(arguments.schema, "", f"cannot read schema: {error}")
        return 1

    schema = cast(JsonSchema, schema_value)
    try:
        Draft202012Validator.check_schema(schema)
    except SchemaError as error:
        report_failure(arguments.schema, json_pointer(error.absolute_path), error.message)
        return 1

    validator = Draft202012Validator(schema)
    failures = False
    for filename in arguments.reports:
        try:
            instance = read_json(filename)
        except (OSError, UnicodeError, json.JSONDecodeError) as error:
            report_failure(filename, "", f"cannot read report: {error}")
            failures = True
            continue

        errors = validation_errors(validator, instance)
        if errors:
            for error in nested_validation_errors(errors):
                report_failure(
                    filename,
                    json_pointer(error.absolute_path),
                    schema_error_message(error),
                )
            failures = True
            continue
        semantic_failures: list[Failure] = []
        check_all_semantics(instance, semantic_failures)
        for pointer, relation in semantic_failures:
            report_failure(filename, pointer, relation)
        failures = failures or bool(semantic_failures)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
