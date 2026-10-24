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
PLAYABLE_UNITS = 100
INACCURACY_UNITS = 200
OPPORTUNITY_MARGIN_UNITS = 260
SELECTION_KEYWORDS = (
    "best",
    "playable",
    "inaccuracies",
    "mistakes",
    "blunders",
    "unplayable",
    "all",
)


class Arguments(argparse.Namespace):
    def __init__(self) -> None:
        super().__init__()
        self.schema: Path = Path()
        self.reports: list[Path] = []
        self.projection: list[Path] | None = None


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


def fail(failures: list[Failure], pointer: str, relation: str, condition: bool) -> None:
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


def same_engine_score(left: JsonValue, right: JsonValue) -> bool:
    left_eval = as_object(left)
    right_eval = as_object(right)
    if left_eval is None or right_eval is None:
        return False
    left_cp, right_cp = left_eval.get("cp"), right_eval.get("cp")
    left_mate, right_mate = left_eval.get("mate"), right_eval.get("mate")
    if isinstance(left_cp, int) and not isinstance(left_cp, bool):
        return left_cp == right_cp and left_mate is None and right_mate is None
    if isinstance(left_mate, int) and not isinstance(left_mate, bool):
        return left_mate == right_mate and left_cp is None and right_cp is None
    return False


def expected_verdict(
    loss_units: int, is_candidate: bool, same_best_score: bool, allows_tactic: bool
) -> str:
    if loss_units <= PLAYABLE_UNITS:
        return "best" if is_candidate and same_best_score else "okay"
    if loss_units <= INACCURACY_UNITS:
        return "inaccuracy"
    return "blunder" if allows_tactic else "mistake"


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
    board = board_from_fen(
        threat_object.get("root_fen"), f"{pointer}/root_fen", failures
    )
    if board is None:
        return None, None
    moves = replay_san_line(
        board, threat_object.get("line"), f"{pointer}/line", failures
    )
    opportunity = as_object(threat_object.get("opportunity"))
    if opportunity is not None:
        _ = check_move_pair(
            board,
            opportunity.get("alternative"),
            f"{pointer}/opportunity/alternative",
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
    line_moves = replay_san_line(
        root, move_object.get("line"), f"{pointer}/line", failures
    )
    if first_move is not None and line_moves:
        fail(
            failures,
            f"{pointer}/line/0",
            "the first line move must equal the assessed SAN/UCI move",
            first_move == line_moves[0],
        )
    opportunity = as_object(move_object.get("opportunity"))
    if opportunity is not None:
        _ = check_move_pair(
            root,
            opportunity.get("alternative"),
            f"{pointer}/opportunity/alternative",
            failures,
        )
    for field in ("creates_threat", "refutation"):
        nested_threat = move_object.get(field)
        if isinstance(nested_threat, dict):
            _ = check_threat_legality(nested_threat, f"{pointer}/{field}", failures)
    return first_move


def check_score_relations(report: JsonObject, failures: list[Failure]) -> None:
    candidates = as_array(report.get("candidates")) or []
    asked_moves = as_array(report.get("asked_moves")) or []
    assessed: list[tuple[str, JsonValue, bool]] = [
        (f"/candidates/{index}", candidate, True)
        for index, candidate in enumerate(candidates)
    ]
    assessed.extend(
        (f"/asked_moves/{index}", move, False) for index, move in enumerate(asked_moves)
    )
    if not candidates:
        return
    best = as_object(candidates[0])
    if best is None:
        return
    best_eval = best.get("eval")
    best_units = score_units(best_eval)
    if best_units is None:
        return
    for pointer, value, is_candidate in assessed:
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
        expected = expected_verdict(
            expected_loss,
            is_candidate and move_object.get("in_multipv") is True,
            same_engine_score(move_object.get("eval"), best_eval),
            move_object.get("allows_tactic") is True,
        )
        fail(
            failures,
            f"{pointer}/verdict",
            f"verdict must be {expected!r} for {expected_loss} loss units, the line-1 score and allows_tactic",
            move_object.get("verdict") == expected,
        )


def check_selection_relations(
    report: JsonObject, root: chess.Board | None, failures: list[Failure]
) -> None:
    input_metadata = as_object(report.get("input"))
    if input_metadata is None:
        return
    raw_moves_value = input_metadata.get("moves")
    raw_items = raw_moves_value.split(",") if isinstance(raw_moves_value, str) else []
    input_object = get_object(report.get("input"), "selection")
    if input_object is None:
        return
    requested_keywords: set[str] = set()
    requested_moves: list[str] = []
    requested_top: int | None = None
    for item in raw_items:
        if item in SELECTION_KEYWORDS:
            requested_keywords.add(item)
            continue
        if item.startswith("top") and item[3:].isdecimal():
            value = int(item[3:])
            if value > 0:
                requested_top = max(requested_top or value, value)
            continue
        if root is None:
            continue
        move = legal_san_move(root, item)
        if move is None:
            try:
                uci_move = chess.Move.from_uci(item)
            except ValueError:
                uci_move = None
            if uci_move is not None and root.is_legal(uci_move):
                move = uci_move
        if move is not None:
            uci = root.uci(move)
            if uci not in requested_moves:
                requested_moves.append(uci)
    if raw_moves_value is None:
        requested_keywords.add("best")
    expected_keywords = [
        keyword for keyword in SELECTION_KEYWORDS if keyword in requested_keywords
    ]
    actual_keywords = as_array(input_object.get("keywords")) or []
    fail(
        failures,
        "/input/selection/keywords",
        "keywords must be unique and in the fixed selection order",
        actual_keywords == expected_keywords,
    )
    actual_top = input_object.get("top")
    fail(
        failures,
        "/input/selection/top",
        "top must be the largest topN item, or null when none was requested",
        actual_top == requested_top,
    )
    actual_moves = as_array(input_object.get("moves")) or []
    fail(
        failures,
        "/input/selection/moves",
        "asked move UCIs must be deduplicated in typed order",
        actual_moves == requested_moves,
    )

    candidates = as_array(report.get("candidates")) or []
    candidate_ucis: list[str] = []
    for index, candidate_value in enumerate(candidates):
        candidate = as_object(candidate_value)
        if candidate is None:
            continue
        uci = candidate.get("uci")
        if isinstance(uci, str):
            candidate_ucis.append(uci)
        fail(
            failures,
            f"/candidates/{index}/in_multipv",
            "every candidate is a line from Search A",
            candidate.get("in_multipv") is True,
        )
    expected_asked_moves = [uci for uci in requested_moves if uci not in candidate_ucis]
    asked_moves = as_array(report.get("asked_moves")) or []
    actual_asked_moves = [
        move.get("uci")
        for value in asked_moves
        if (move := as_object(value)) is not None
    ]
    fail(
        failures,
        "/asked_moves",
        "asked-move assessments must be exactly the selected moves outside Search A, in typed order",
        actual_asked_moves == expected_asked_moves,
    )
    for index, value in enumerate(asked_moves):
        move = as_object(value)
        if move is not None:
            fail(
                failures,
                f"/asked_moves/{index}/in_multipv",
                "a Search C move is outside Search A",
                move.get("in_multipv") is False,
            )


def check_playable_truncated(report: JsonObject, failures: list[Failure]) -> None:
    engine = as_object(report.get("engine"))
    position = as_object(report.get("position"))
    if engine is None or position is None:
        return
    width = engine.get("multipv")
    legal_moves = position.get("legal_moves")
    candidates = as_array(report.get("candidates")) or []
    all_playable = True
    for value in candidates:
        candidate = as_object(value)
        loss = None if candidate is None else number_units(candidate.get("loss"))
        if loss is None or loss > PLAYABLE_UNITS:
            all_playable = False
            break
    expected = (
        isinstance(width, int)
        and not isinstance(width, bool)
        and isinstance(legal_moves, int)
        and not isinstance(legal_moves, bool)
        and len(candidates) == width
        and width < legal_moves
        and all_playable
    )
    fail(
        failures,
        "/playable_truncated",
        "playable_truncated must hold exactly when Search A returned all K playable lines and K is below the legal-move count",
        report.get("playable_truncated") is expected,
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
    asked_moves = as_array(report.get("asked_moves")) or []
    fail(
        failures,
        "/asked_moves",
        "a terminal position must have no asked-move assessments",
        not asked_moves,
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
    move_pairs.extend(
        (f"/asked_moves/{index}", value)
        for index, value in enumerate(as_array(report.get("asked_moves")) or [])
    )
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
        if test not in ("threat_move_illegal", "searched", "deferred_by_check"):
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
            None
            if not line or threat_root is None
            else legal_san_move(threat_root, line[0])
        )
        assessed_move = check_move_pair(root, value, pointer, failures)
        if first_threat_move is None or assessed_move is None:
            fail(
                failures,
                f"{pointer}/parries_threat/test",
                "parry tests require a resolvable first move of the opponent threat",
                False,
            )
            continue
        after_move = root.copy()
        after_move.push(assessed_move)
        threat_move_legal = after_move.is_legal(first_threat_move)
        deferred = after_move.is_checkmate() or (
            move_object.get("gives_check") is True
            and after_move.is_pseudo_legal(first_threat_move)
        )
        if test == "threat_move_illegal":
            relation = "threat_move_illegal requires an illegal threat move not deferred by check"
            condition = not threat_move_legal and not deferred
            fail(
                failures,
                f"{pointer}/parries_threat/value",
                "an illegal threat move must parry the threat",
                parry.get("value") is True,
            )
        elif test == "deferred_by_check":
            relation = (
                "deferred_by_check requires an illegal threat move deferred by check"
            )
            condition = not threat_move_legal and deferred
            fail(
                failures,
                f"{pointer}/parries_threat/value",
                "a test deferred by check must not claim the threat is parried",
                parry.get("value") is False,
            )
        else:
            relation = "searched requires a legal threat move"
            condition = threat_move_legal
        fail(
            failures,
            f"{pointer}/parries_threat/test",
            relation,
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
    if move_object.get("detailed") is False:
        return  # check_move_annotations requires the skip fact to be null here
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


def held_to_value(held_square: chess.Square, free_moves: list[str]) -> dict[str, str]:
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


def hypothetical_pawn_captures(
    board: chess.Board, held_square: chess.Square, pawn: chess.Piece
) -> tuple[set[str], set[str]]:
    pseudo_moves: set[str] = set()
    legal_moves: set[str] = set()
    direction = 1 if pawn.color == chess.WHITE else -1
    target_rank = chess.square_rank(held_square) + direction
    king_square = board.king(pawn.color)
    promotion_pieces: tuple[int | None, ...] = (
        (chess.QUEEN, chess.ROOK, chess.BISHOP, chess.KNIGHT)
        if target_rank == (7 if pawn.color == chess.WHITE else 0)
        else (None,)
    )
    for file_delta in (-1, 1):
        target_file = chess.square_file(held_square) + file_delta
        if not 0 <= target_file < 8 or not 0 <= target_rank < 8:
            continue
        target_square = chess.square(target_file, target_rank)
        target_piece = board.piece_at(target_square)
        if target_piece is not None and target_piece.piece_type == chess.KING:
            continue
        after = board.copy(stack=False)
        _ = after.remove_piece_at(held_square)
        _ = after.remove_piece_at(target_square)
        after.set_piece_at(target_square, pawn)
        after.turn = not pawn.color
        after.ep_square = None
        is_legal = king_square is None or not after.is_attacked_by(
            not pawn.color, king_square
        )
        for promotion in promotion_pieces:
            move = chess.Move(held_square, target_square, promotion=promotion)
            move_uci = move.uci()
            pseudo_moves.add(move_uci)
            if is_legal:
                legal_moves.add(move_uci)
    return pseudo_moves, legal_moves


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
    expected_piece = (
        actor if tag == "overloaded_defender" else (targets[0] if targets else None)
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

    # A move set never captures a king (the flipped board can leave X in check).
    def captures_king(move: chess.Move) -> bool:
        target = by.piece_at(move.to_square)
        return target is not None and target.piece_type == chess.KING

    legal_move_set = {
        move.uci()
        for move in by.legal_moves
        if move.from_square == held_square and not captures_king(move)
    }
    pseudo_move_set = {
        move.uci()
        for move in by.pseudo_legal_moves
        if move.from_square == held_square and not captures_king(move)
    }
    pawn = by.piece_at(held_square)
    if pawn is not None and pawn.piece_type == chess.PAWN:
        hypothetical_pseudo, hypothetical_legal = hypothetical_pawn_captures(
            by, held_square, pawn
        )
        pseudo_move_set.update(hypothetical_pseudo)
        legal_move_set.update(hypothetical_legal)
    legal_moves = sorted(legal_move_set)
    pseudo_moves = sorted(pseudo_move_set)
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
    line_moves = replay_san_line(
        root, line, f"{pointer.rsplit('/motifs', 1)[0]}/line", failures
    )
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
    for key in ("candidates", "asked_moves"):
        values = as_array(report.get(key)) or []
        move_pairs = [(f"/{key}/{index}", value) for index, value in enumerate(values)]
        for pointer, move_value in move_pairs:
            move = as_object(move_value)
            if move is None:
                continue
            check_motif_restrictions(
                move.get("motifs"),
                root,
                move.get("line"),
                f"{pointer}/motifs",
                failures,
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


def check_line_labels(
    labels_value: JsonValue,
    expected_tactic_line: bool,
    pointer: str,
    failures: list[Failure],
) -> None:
    labels = as_array(labels_value)
    if expected_tactic_line:
        return
    fail(
        failures,
        pointer,
        "tactic_labels must be empty outside a tactic line",
        labels is not None and not labels,
    )


def check_opportunity_judgement(
    opportunity_value: JsonValue,
    pointer: str,
    forced_expected: bool,
    expected_margin: int | None,
    expected_alternative: JsonObject | None,
    first_move_uci: str | None,
    failures: list[Failure],
) -> bool:
    opportunity = as_object(opportunity_value)
    fail(
        failures,
        pointer,
        "a judged line must contain an opportunity object",
        opportunity is not None,
    )
    if opportunity is None:
        return False
    expected_opportunity = (
        not forced_expected
        and expected_margin is not None
        and expected_margin >= OPPORTUNITY_MARGIN_UNITS
    )
    fail(
        failures,
        f"{pointer}/source",
        "opportunity source must be rule iff the line is forced, otherwise engine",
        opportunity.get("source") == ("rule" if forced_expected else "engine"),
    )
    fail(
        failures,
        f"{pointer}/forced",
        f"forced must be {forced_expected} from the number of legal root moves",
        opportunity.get("forced") == forced_expected,
    )
    alternative = opportunity.get("alternative")
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
            opportunity.get("margin") is None,
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
        actual_margin = number_units(opportunity.get("margin"))
        fail(
            failures,
            f"{pointer}/margin",
            f"margin must equal {expected_margin} expected-score units",
            expected_margin is not None and actual_margin == expected_margin,
        )
    fail(
        failures,
        f"{pointer}/is_opportunity",
        f"is_opportunity must be {expected_opportunity}; threshold 260 units, forced lines are never opportunities",
        opportunity.get("is_opportunity") == expected_opportunity,
    )
    return expected_opportunity


def check_refutation(move: JsonObject, pointer: str, failures: list[Failure]) -> bool:
    refutation = as_object(move.get("refutation"))
    if refutation is None:
        return False
    allows_tactic = move.get("allows_tactic") is True
    fail(
        failures,
        f"{pointer}/refutation/is_tactic",
        "a refutation is a tactic iff the refuted move allows a tactic",
        refutation.get("is_tactic") == allows_tactic,
    )
    line = as_array(refutation.get("line")) or []
    motifs_pointer = f"{pointer}/refutation/motifs"
    if move.get("detailed") is False:
        fail(
            failures,
            f"{pointer}/refutation/opportunity",
            "a non-detailed line's refutation must not have an opportunity judgement",
            refutation.get("opportunity") is None,
        )
        check_motif_contributions(
            refutation.get("motifs"), allows_tactic, motifs_pointer, failures
        )
        check_line_labels(
            refutation.get("tactic_labels"),
            allows_tactic,
            f"{pointer}/refutation/tactic_labels",
            failures,
        )
        return allows_tactic
    if not line:
        fail(
            failures,
            f"{pointer}/refutation/opportunity",
            "a refutation with no plies must have no opportunity judgement",
            refutation.get("opportunity") is None,
        )
        check_motif_contributions(
            refutation.get("motifs"), allows_tactic, motifs_pointer, failures
        )
        check_line_labels(
            refutation.get("tactic_labels"),
            allows_tactic,
            f"{pointer}/refutation/tactic_labels",
            failures,
        )
        return allows_tactic
    root = board_from_fen(
        refutation.get("root_fen"), f"{pointer}/refutation/root_fen", failures
    )
    if root is None:
        return False
    forced = root.legal_moves.count() == 1
    first_move = legal_san_move(root, line[0])
    first_uci = None if first_move is None else root.uci(first_move)
    opportunity = as_object(refutation.get("opportunity"))
    if forced:
        margin = None
    else:
        move_units = score_units(move.get("eval"))
        alternative = (
            None if opportunity is None else as_object(opportunity.get("alternative"))
        )
        alternative_units = (
            None if alternative is None else score_units(alternative.get("eval"))
        )
        margin = (
            None
            if move_units is None or alternative_units is None
            else (2000 - move_units) - alternative_units
        )
    is_opportunity = check_opportunity_judgement(
        refutation.get("opportunity"),
        f"{pointer}/refutation/opportunity",
        forced,
        margin,
        None,
        first_uci,
        failures,
    )
    tactic_line = allows_tactic or is_opportunity
    check_motif_contributions(
        refutation.get("motifs"), tactic_line, motifs_pointer, failures
    )
    check_line_labels(
        refutation.get("tactic_labels"),
        tactic_line,
        f"{pointer}/refutation/tactic_labels",
        failures,
    )
    return tactic_line


def check_move_annotations(
    move_value: JsonValue, pointer: str, failures: list[Failure]
) -> None:
    move = as_object(move_value)
    if move is None:
        return
    consequence = as_object(move.get("consequence"))
    consequence_class = None if consequence is None else consequence.get("class")
    expected_is_tactic = consequence_class in ("mate", "material")
    fail(
        failures,
        f"{pointer}/is_tactic",
        "is_tactic must be true exactly for mate or material consequences",
        move.get("is_tactic") is expected_is_tactic,
    )
    material = None if consequence is None else consequence.get("material")
    eval_object = as_object(move.get("eval"))
    mate = None if eval_object is None else eval_object.get("mate")
    expected_allows_tactic = (
        isinstance(material, int) and not isinstance(material, bool) and material <= -1
    ) or (isinstance(mate, int) and not isinstance(mate, bool) and mate < 0)
    fail(
        failures,
        f"{pointer}/allows_tactic",
        "allows_tactic must be true exactly when consequence material is at most -1 or eval.mate is negative",
        move.get("allows_tactic") is expected_allows_tactic,
    )
    if move.get("detailed") is False:
        for field in ("parries_threat", "creates_threat", "creates_threat_skipped"):
            fail(
                failures,
                f"{pointer}/{field}",
                "a non-detailed line must not have parry, created-threat or skip facts",
                move.get(field) is None,
            )
        refutation = as_object(move.get("refutation"))
        if refutation is not None:
            fail(
                failures,
                f"{pointer}/refutation/opportunity",
                "a non-detailed line's refutation must not have an opportunity judgement",
                refutation.get("opportunity") is None,
            )
    opportunity = as_object(move.get("opportunity"))
    is_opportunity = (
        opportunity is not None and opportunity.get("is_opportunity") is True
    )
    tactic_line = move.get("is_tactic") is True or is_opportunity
    check_motif_contributions(
        move.get("motifs"), tactic_line, f"{pointer}/motifs", failures
    )
    check_motif_contributions(
        move.get("root_motifs"), tactic_line, f"{pointer}/root_motifs", failures
    )
    if not tactic_line:
        fail(
            failures,
            f"{pointer}/root_motifs",
            "root_motifs must be empty outside a tactic line",
            move.get("root_motifs") == [],
        )
    check_line_labels(
        move.get("tactic_labels"), tactic_line, f"{pointer}/tactic_labels", failures
    )
    created = as_object(move.get("creates_threat"))
    if created is not None:
        check_motif_contributions(
            created.get("motifs"), False, f"{pointer}/creates_threat/motifs", failures
        )
        fail(
            failures,
            f"{pointer}/creates_threat/is_tactic",
            "created-threat lines do not carry a tactic flag",
            created.get("is_tactic") is None,
        )
        fail(
            failures,
            f"{pointer}/creates_threat/opportunity",
            "created-threat lines do not carry an opportunity judgement",
            created.get("opportunity") is None,
        )
        check_line_labels(
            created.get("tactic_labels"),
            False,
            f"{pointer}/creates_threat/tactic_labels",
            failures,
        )
    _ = check_refutation(move, pointer, failures)


def check_tactic_invariants(
    report: JsonObject, root: chess.Board | None, failures: list[Failure]
) -> None:
    position = get_object(report, "position")
    if position is None:
        return
    terminal = position.get("terminal") is not None
    candidates = as_array(report.get("candidates")) or []
    if not terminal:
        legal_moves = root.legal_moves.count() if root is not None else 0
        first = as_object(candidates[0]) if candidates else None
        opportunity = None if first is None else as_object(first.get("opportunity"))
        if first is None:
            fail(
                failures,
                "/candidates/0/opportunity",
                "a non-terminal root must have a best candidate with an opportunity judgement",
                False,
            )
        elif opportunity is None:
            fail(
                failures,
                "/candidates/0/opportunity",
                "a non-terminal root must have an opportunity judgement object",
                False,
            )
        elif legal_moves == 1:
            _ = check_opportunity_judgement(
                opportunity,
                "/candidates/0/opportunity",
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
                    key: second[key] for key in ("san", "uci", "eval") if key in second
                }
                first_units = score_units(first.get("eval"))
                second_units = score_units(second.get("eval"))
                if first_units is not None and second_units is not None:
                    expected_margin = first_units - second_units
            _ = check_opportunity_judgement(
                opportunity,
                "/candidates/0/opportunity",
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
                f"/candidates/{index}/opportunity",
                "only candidates[0] has a root opportunity judgement",
                candidate.get("opportunity") is None,
            )
        check_move_annotations(candidate, f"/candidates/{index}", failures)

    check_motif_contributions(
        position.get("motifs"), False, "/position/motifs", failures
    )
    threats = as_object(report.get("threats"))
    if threats is not None:
        for color in ("white", "black"):
            threat = as_object(threats.get(color))
            if threat is None:
                continue
            for field in ("is_tactic", "opportunity"):
                fail(
                    failures,
                    f"/threats/{color}/{field}",
                    "root threat lines do not carry tactic judgements",
                    threat.get(field) is None,
                )
            check_line_labels(
                threat.get("tactic_labels"),
                False,
                f"/threats/{color}/tactic_labels",
                failures,
            )

    for index, asked in enumerate(as_array(report.get("asked_moves")) or []):
        asked_object = as_object(asked)
        if asked_object is not None:
            fail(
                failures,
                f"/asked_moves/{index}/opportunity",
                "Search C lines do not have opportunity judgements",
                asked_object.get("opportunity") is None,
            )
        check_move_annotations(asked, f"/asked_moves/{index}", failures)


TEMPO_WEAK_PAWNS = frozenset(
    {"isolated_pawn", "doubled_pawns", "tripled_pawns", "backward_pawn"}
)
TEMPO_PASSERS = frozenset({"passed_pawn", "connected_passed_pawns"})
TEMPO_KING_FILES = frozenset(
    {"king_shield_gap", "open_file_near_king", "half_open_file_near_king"}
)
TEMPO_SHELTER = TEMPO_KING_FILES | frozenset(
    {"castling_kingside", "castling_queenside"}
)
TEMPO_ROOK_FILES = frozenset({"rook_open_file", "rook_half_open_file"})
TEMPO_WEAKNESS_TAGS = frozenset(
    {
        "isolated_pawn",
        "doubled_pawns",
        "tripled_pawns",
        "backward_pawn",
        "king_shield_gap",
        "open_file_near_king",
        "half_open_file_near_king",
        "king_zone_attacker",
        "bad_bishop",
    }
)
TEMPO_DYNAMIC_TAGS = frozenset(
    {
        "castling_kingside",
        "castling_queenside",
        "king_zone_attacker",
        "mobility_lead",
        "development_lead",
    }
)
TEMPO_STATIC_GROUPS = {
    "castling_kingside": 0,
    "castling_queenside": 0,
    "king_shield_gap": 0,
    "open_file_near_king": 0,
    "half_open_file_near_king": 0,
    "king_zone_attacker": 0,
    "material_ahead": 1,
    "exchange_up": 1,
    "bishop_pair": 1,
    "pawn_islands": 2,
    "pawn_chain": 2,
    "isolated_pawn": 2,
    "doubled_pawns": 2,
    "tripled_pawns": 2,
    "backward_pawn": 2,
    "passed_pawn": 2,
    "connected_passed_pawns": 2,
    "rook_open_file": 3,
    "rook_half_open_file": 3,
    "rook_seventh": 3,
    "outpost": 3,
    "bad_bishop": 3,
    "space_lead": 4,
    "mobility_lead": 4,
    "development_lead": 4,
}


def rule_game_over(board: chess.Board) -> bool:
    return board.is_checkmate() or board.is_stalemate()


def material_balance(board: chess.Board, side: chess.Color) -> int:
    values = {
        chess.PAWN: 1,
        chess.KNIGHT: 3,
        chess.BISHOP: 3,
        chess.ROOK: 5,
        chess.QUEEN: 9,
    }
    balance = 0
    for piece_type, value in values.items():
        balance += value * len(board.pieces(piece_type, side))
        balance -= value * len(board.pieces(piece_type, not side))
    return balance


def tempo_label(value_units: int) -> str | None:
    if value_units > 200:
        return "tempo_dependent"
    if value_units < 0:
        return "zugzwang"
    if value_units < 100:
        return "non_tempo"
    return None


def check_tempo_value(
    value: JsonValue,
    pointer: str,
    side: chess.Color | None,
    expected_moves: int | None,
    must_be_null: bool,
    failures: list[Failure],
) -> None:
    if must_be_null:
        fail(
            failures,
            pointer,
            "tempo value must be null when it cannot be measured",
            value is None,
        )
        return
    tempo = as_object(value)
    fail(
        failures,
        pointer,
        "tempo value must be set when both searches returned a line",
        tempo is not None,
    )
    if tempo is None:
        return
    if side is not None:
        fail(
            failures,
            f"{pointer}/side",
            f"tempo side must be {side_name(side)!r}",
            tempo.get("side") == side_name(side),
        )
    moves = number_units(tempo.get("moves"))
    passes = number_units(tempo.get("passes"))
    value_units = number_units(tempo.get("value"))
    if expected_moves is not None:
        fail(
            failures,
            f"{pointer}/moves",
            "tempo moves must equal the expected score when the side moves",
            moves == expected_moves,
        )
    fail(
        failures,
        f"{pointer}/value",
        "tempo value must equal moves minus passes in ES units",
        moves is not None and passes is not None and value_units == moves - passes,
    )
    expected_label = None if value_units is None else tempo_label(value_units)
    fail(
        failures,
        f"{pointer}/label",
        "tempo label must match the §6.1 ES-unit boundaries",
        tempo.get("label") == expected_label,
    )
    fail(
        failures,
        f"{pointer}/source",
        "tempo value provenance must be engine",
        tempo.get("source") == "engine",
    )


def consequence_rank(consequence_value: JsonValue) -> int | None:
    consequence = as_object(consequence_value)
    if consequence is None:
        return None
    consequence_class = consequence.get("class")
    if consequence_class == "mate":
        return 4
    if consequence_class == "material":
        material = consequence.get("material")
        if not isinstance(material, int) or isinstance(material, bool):
            return None
        return 3 if material >= 3 else 2 if material >= 1 else 0
    if consequence_class == "promotion":
        return 1
    if consequence_class == "positional":
        return 0
    return None


def replay_walk(
    root: chess.Board,
    assessment: JsonObject,
) -> tuple[list[chess.Board], list[chess.Move]] | None:
    line = as_array(assessment.get("line"))
    walk = assessment.get("walked_plies")
    if (
        line is None
        or not isinstance(walk, int)
        or isinstance(walk, bool)
        or walk < 0
        or walk > len(line)
    ):
        return None
    boards = [root.copy()]
    moves: list[chess.Move] = []
    current = root.copy()
    for san in line[:walk]:
        move = legal_san_move(current, san)
        if move is None:
            return None
        current.push(move)
        moves.append(move)
        boards.append(current.copy())
        if rule_game_over(current):
            break
    return boards, moves


def replay_line_fens(
    position_fen: JsonValue, line_value: JsonValue, walked_plies: JsonValue
) -> list[str] | None:
    line = as_array(line_value)
    if (
        not isinstance(position_fen, str)
        or line is None
        or not isinstance(walked_plies, int)
        or isinstance(walked_plies, bool)
        or walked_plies < 0
        or walked_plies > len(line)
    ):
        return None
    try:
        board = chess.Board(position_fen)
        line_fens: list[str] = []
        for san in line[:walked_plies]:
            if not isinstance(san, str):
                return None
            _ = board.push_san(san)
            line_fens.append(board.fen(en_passant="legal"))
        return line_fens
    except ValueError:
        return None


def null_move_has_no_legal_move(board: chess.Board) -> bool:
    passed = board.copy()
    passed.push(chess.Move.null())
    return not any(passed.legal_moves)


def favoured_side(change: JsonObject) -> str | None:
    tag = change.get("tag")
    if tag in ("pawn_islands", "pawn_chain"):
        return None
    side = change.get("side")
    if side not in ("white", "black"):
        return None
    favours_owner = (change.get("change") == "appears") != (tag in TEMPO_WEAKNESS_TAGS)
    if favours_owner:
        return side
    return "black" if side == "white" else "white"


def change_squares(change: JsonObject) -> set[chess.Square]:
    squares: set[chess.Square] = set()
    for piece_value in as_array(change.get("pieces")) or []:
        piece = as_object(piece_value)
        square = None if piece is None else piece.get("square")
        if isinstance(square, str):
            try:
                squares.add(chess.parse_square(square))
            except ValueError:
                pass
    return squares


def king_zone(board: chess.Board, side: chess.Color) -> chess.SquareSet:
    king = board.king(side)
    if king is None:
        return chess.SquareSet()
    return chess.SquareSet(chess.BB_KING_ATTACKS[king] | chess.BB_SQUARES[king])


def king_pressed(
    side: chess.Color,
    start_ply: int,
    boards: list[chess.Board],
    moves: list[chess.Move],
) -> bool:
    other = not side
    end_ply = len(moves)
    for ply in range(start_ply, end_ply + 1):
        before = boards[ply - 1]
        move = moves[ply - 1]
        if before.turn != other:
            continue
        after = boards[ply]
        zone = king_zone(before, side)
        if after.is_check() or (before.is_capture(move) and move.to_square in zone):
            return True
    end = boards[-1]
    zone = king_zone(end, side)
    attackers = chess.SquareSet()
    for square in zone:
        attackers |= end.attackers(other, square)
    non_pawns = attackers & (
        end.pieces_mask(chess.KNIGHT, other)
        | end.pieces_mask(chess.BISHOP, other)
        | end.pieces_mask(chess.ROOK, other)
        | end.pieces_mask(chess.QUEEN, other)
    )
    return len(non_pawns) >= 2


def use_test(
    change: JsonObject,
    boards: list[chess.Board],
    moves: list[chess.Move],
    tempo_dependent: bool,
) -> bool:
    tag = change.get("tag")
    ply = change.get("ply")
    side_name_value = change.get("side")
    if (
        not isinstance(ply, int)
        or isinstance(ply, bool)
        or ply < 1
        or ply > len(moves)
        or side_name_value not in ("white", "black")
    ):
        return False
    side = side_name_value == "white"
    other = not side
    move = moves[ply - 1]
    before = boards[ply - 1]
    end = boards[-1]
    squares = change_squares(change)
    appearing = change.get("change") == "appears"

    if end.is_checkmate():
        return False
    if (
        tag in ("castling_kingside", "castling_queenside")
        and not appearing
        and before.is_castling(move)
    ):
        return False
    if tag == "material_ahead" and not appearing and material_balance(end, side) != 0:
        return False
    if tag == "king_zone_attacker" and not appearing:
        return False
    if (
        tag in TEMPO_KING_FILES
        and not appearing
        and end.king(side) != boards[0].king(side)
    ):
        return False
    if tag in {
        "material_ahead",
        "exchange_up",
        "rook_seventh",
        "outpost",
        "bad_bishop",
        "bishop_pair",
    }:
        return True
    if tag in ("pawn_islands", "pawn_chain"):
        return False
    if tag in {"mobility_lead", "space_lead", "development_lead"}:
        return tempo_dependent
    if tag in TEMPO_KING_FILES and (
        end.has_kingside_castling_rights(side)
        or end.has_queenside_castling_rights(side)
    ):
        return king_pressed(side, ply, boards, moves)
    if ply <= 2 and tag in (
        TEMPO_WEAK_PAWNS | TEMPO_PASSERS | TEMPO_SHELTER | TEMPO_ROOK_FILES
    ):
        return True
    if tag in TEMPO_WEAK_PAWNS:
        if appearing:
            if any(
                boards[later_ply - 1].turn == other
                and boards[later_ply - 1].is_capture(moves[later_ply - 1])
                and moves[later_ply - 1].to_square in squares
                for later_ply in range(ply + 1, len(moves) + 1)
            ):
                return True
            return any(
                (piece := end.piece_at(square)) is not None
                and piece.color == side
                and piece.piece_type == chess.PAWN
                and end.is_attacked_by(other, square)
                for square in squares
            )
        return (
            before.turn == other
            and before.is_capture(move)
            and move.to_square in squares
        )
    if tag in TEMPO_PASSERS:
        if appearing:
            return any(
                moves[later_ply - 1].from_square in squares
                for later_ply in range(ply + 1, len(moves) + 1)
            )
        return move.from_square in squares or (
            before.turn == other and move.to_square in squares
        )
    if appearing and (tag in TEMPO_SHELTER or tag == "king_zone_attacker"):
        return king_pressed(side, ply, boards, moves)
    if tag in TEMPO_ROOK_FILES:
        if not appearing:
            return False
        return any(
            moves[later_ply - 1].from_square in squares
            and chess.square_file(moves[later_ply - 1].from_square)
            == chess.square_file(moves[later_ply - 1].to_square)
            for later_ply in range(ply + 1, len(moves) + 1)
        )
    return False


def expected_initiative_runs(
    plies: list[JsonValue],
    boards: list[chess.Board],
    changes_value: JsonValue,
) -> list[JsonObject]:
    n = len(plies)
    changes = as_array(changes_value) or []
    runs: list[JsonObject] = []
    scan = 1
    while scan <= n:
        first = as_object(plies[scan - 1])
        if first is None or first.get("tempo_gained") is not True:
            scan += 1
            continue
        start = scan
        holder = first.get("side")
        last_gain = start
        while last_gain + 2 <= n:
            following = as_object(plies[last_gain + 1])
            if following is None or following.get("tempo_gained") is not True:
                break
            last_gain += 2
        ending_ply = last_gain + 2
        if ending_ply > n:
            ending = "unfinished"
            end = n
        else:
            ending_move = as_object(plies[ending_ply - 1]) or {}
            if ending_move.get("threatening") is not True:
                ending = "spent"
                end = ending_ply
            elif boards[ending_ply].is_checkmate():
                ending = "runs"
                end = ending_ply
            elif ending_ply + 1 > n:
                ending = "unfinished"
                end = ending_ply
            else:
                reply = as_object(plies[ending_ply]) or {}
                if reply.get("label") == "counter_threat":
                    ending = "contested"
                    end = ending_ply + 1
                elif reply.get("stops_threat") is False:
                    ending = "runs"
                    end = ending_ply + 1
                else:
                    ending = "runs"
                    end = ending_ply + 1
        converted = False
        holder_color = holder == "white"
        if holder in ("white", "black") and (
            material_balance(boards[-1], holder_color)
            - material_balance(boards[start - 1], holder_color)
            >= 1
        ):
            converted = True

        def change_converts(
            change_value: JsonValue,
            run_start: int,
            run_end: int,
            run_holder: JsonValue,
        ) -> bool:
            change = as_object(change_value)
            if change is None:
                return False
            ply = change.get("ply")
            return (
                change.get("net_change") is True
                and change.get("in_use") is True
                and isinstance(ply, int)
                and not isinstance(ply, bool)
                and run_start <= ply <= run_end
                and favoured_side(change) == run_holder
            )

        if not converted:
            converted = any(
                change_converts(change_value, start, end, holder)
                for change_value in changes
            )
        runs.append(
            {
                "holder": holder,
                "start_ply": start,
                "end_ply": end,
                "tempi": (last_gain - start) // 2 + 1,
                "ending": ending,
                "outcome": "converted" if converted else "evaporated",
                "source": "rule",
            }
        )
        scan = max(start + 1, end)
    return runs


def check_tempo_ledger(
    assessment: JsonObject,
    pointer: str,
    boards: list[chess.Board],
    moves: list[chess.Move],
    failures: list[Failure],
) -> None:
    ledger = as_object(assessment.get("tempo_ledger"))
    plies_value = None if ledger is None else as_array(ledger.get("plies"))
    if ledger is None or plies_value is None:
        return
    count = len(moves)
    fail(
        failures,
        f"{pointer}/tempo_ledger/plies",
        "ledger plies must cover the walked line, cut at its first terminal board",
        len(plies_value) == count,
    )
    line = as_array(assessment.get("line")) or []
    expected_labels: list[str] = []
    for index, ply_value in enumerate(plies_value):
        ply_pointer = f"{pointer}/tempo_ledger/plies/{index}"
        ply = as_object(ply_value)
        if ply is None:
            expected_labels.append("neither")
            continue
        expected_ply = index + 1
        fail(
            failures,
            f"{ply_pointer}/ply",
            "ledger ply labels must be consecutive and start at one",
            ply.get("ply") == expected_ply,
        )
        expected_san = line[index] if index < len(line) else None
        fail(
            failures,
            f"{ply_pointer}/san",
            "ledger SAN must match the walked line",
            ply.get("san") == expected_san,
        )
        expected_side = side_name(boards[index].turn) if index < len(boards) else None
        fail(
            failures,
            f"{ply_pointer}/side",
            "ledger side must be the side to move before this ply",
            ply.get("side") == expected_side,
        )
        if index >= len(moves):
            expected_labels.append("neither")
            continue
        after = boards[index + 1]
        by_check = after.is_check()
        fail(
            failures,
            f"{ply_pointer}/by_check",
            "by_check must match the board after this ply",
            ply.get("by_check") is by_check,
        )
        threat_rank = ply.get("threat_rank")
        if by_check:
            expected_threatening = True
            expected_rank: int | None = None
        elif rule_game_over(after) or null_move_has_no_legal_move(after):
            expected_threatening = False
            expected_rank = None
        elif index == 0:
            created = as_object(assessment.get("creates_threat"))
            expected_rank = (
                None
                if created is None
                else consequence_rank(created.get("consequence"))
            )
            expected_threatening = expected_rank is not None and expected_rank >= 2
        else:
            expected_rank = (
                threat_rank
                if isinstance(threat_rank, int) and not isinstance(threat_rank, bool)
                else None
            )
            expected_threatening = expected_rank is not None and expected_rank >= 2
            fail(
                failures,
                f"{ply_pointer}/threat_rank",
                "a searched threat must report its consequence rank",
                expected_rank is not None,
            )
        fail(
            failures,
            f"{ply_pointer}/threat_rank",
            "threat rank must match the available E consequence or be null when no search applies",
            threat_rank == expected_rank,
        )
        fail(
            failures,
            f"{ply_pointer}/threatening",
            "a ply is threatening iff it gives check or its consequence rank is at least two",
            ply.get("threatening") is expected_threatening,
        )

        previous = as_object(plies_value[index - 1]) if index > 0 else None
        previous_threatening = (
            previous is not None and previous.get("threatening") is True
        )
        stops = ply.get("stops_threat")
        if not previous_threatening:
            fail(
                failures,
                f"{ply_pointer}/stops_threat",
                "stops_threat must be null unless the preceding ply is threatening",
                stops is None,
            )
        elif rule_game_over(after):
            fail(
                failures,
                f"{ply_pointer}/stops_threat",
                "a game-ending reply stops the preceding threat",
                stops is True,
            )
        else:
            fail(
                failures,
                f"{ply_pointer}/stops_threat",
                "a reply to a threat must report whether it stops that threat",
                isinstance(stops, bool),
            )
        if previous_threatening:
            if isinstance(stops, bool):
                label = (
                    "counter_threat"
                    if expected_threatening and stops
                    else "pure_reply"
                    if stops
                    else "threatening"
                    if expected_threatening
                    else "neither"
                )
            else:
                label = "neither"
        else:
            label = "threatening" if expected_threatening else "neither"
        expected_labels.append(label)
        fail(
            failures,
            f"{ply_pointer}/label",
            "ledger label must follow the preceding threat, stop test, and this ply's threat",
            ply.get("label") == label,
        )

    for index, ply_value in enumerate(plies_value):
        ply = as_object(ply_value)
        if ply is None:
            continue
        next_label = (
            expected_labels[index + 1] if index + 1 < len(expected_labels) else None
        )
        expected_gained = next_label == "pure_reply"
        fail(
            failures,
            f"{pointer}/tempo_ledger/plies/{index}/tempo_gained",
            "tempo_gained must be set exactly when the next ply is a pure reply",
            ply.get("tempo_gained") is expected_gained,
        )

    tempi = as_object(ledger.get("tempi"))
    if tempi is not None:
        for color in ("white", "black"):
            expected_count = sum(
                1
                for ply_value in plies_value
                if (ply := as_object(ply_value)) is not None
                and ply.get("side") == color
                and ply.get("tempo_gained") is True
            )
            fail(
                failures,
                f"{pointer}/tempo_ledger/tempi/{color}",
                f"{color} tempi must count that side's tempo_gained plies",
                tempi.get(color) == expected_count,
            )

    expected_runs = expected_initiative_runs(
        plies_value, boards, assessment.get("positional")
    )
    runs = as_array(ledger.get("runs"))
    fail(
        failures,
        f"{pointer}/tempo_ledger/runs",
        "initiative runs must match the §6.3 scan",
        runs is not None and len(runs) == len(expected_runs),
    )
    if runs is not None:
        for index, expected in enumerate(expected_runs):
            actual = as_object(runs[index]) if index < len(runs) else None
            run_pointer = f"{pointer}/tempo_ledger/runs/{index}"
            fail(
                failures,
                run_pointer,
                "initiative run must be an object",
                actual is not None,
            )
            if actual is None:
                continue
            for key, expected_value in expected.items():
                fail(
                    failures,
                    f"{run_pointer}/{key}",
                    f"initiative run {key} must match the §6.3 derivation",
                    type(actual.get(key)) is type(expected_value)
                    and actual.get(key) == expected_value,
                )


def check_positional_relations(
    report: JsonObject,
    assessment: JsonObject,
    pointer: str,
    boards: list[chess.Board],
    moves: list[chess.Move],
    tempo_dependent: bool,
    failures: list[Failure],
) -> None:
    position = get_object(report, "position")
    root_features = (
        [] if position is None else as_array(position.get("positional")) or []
    )
    root_identities = {
        positional_identity(feature)
        for feature_value in root_features
        if (feature := as_object(feature_value)) is not None
    }
    changes = as_array(assessment.get("positional")) or []
    latest: dict[str, int] = {}
    for index, change_value in enumerate(changes):
        change = as_object(change_value)
        if change is not None:
            latest[positional_identity(change)] = index
    for index, change_value in enumerate(changes):
        change = as_object(change_value)
        if change is None:
            continue
        change_pointer = f"{pointer}/positional/{index}"
        identity = positional_identity(change)
        expected_net = latest.get(identity) == index and (
            (change.get("change") == "appears") != (identity in root_identities)
        )
        fail(
            failures,
            f"{change_pointer}/net_change",
            "only the last change per identity is net, iff its final direction differs from root features",
            change.get("net_change") is expected_net,
        )
        if not expected_net:
            fail(
                failures,
                f"{change_pointer}/in_use",
                "in_use must be null for a non-net change",
                change.get("in_use") is None,
            )
            continue
        expected_use = use_test(change, boards, moves, tempo_dependent)
        fail(
            failures,
            f"{change_pointer}/in_use",
            "in_use must match the §6.6 use test",
            change.get("in_use") is expected_use,
        )


def check_tempo_and_positional_semantics(
    report: JsonObject, root: chess.Board | None, failures: list[Failure]
) -> None:
    position = get_object(report, "position")
    if position is None:
        return
    candidates = as_array(report.get("candidates")) or []
    first = as_object(candidates[0]) if candidates else None
    root_moves = None if first is None else score_units(first.get("eval"))
    threats = as_object(report.get("threats"))
    opponent_color = None if root is None else not root.turn
    opponent_threat = (
        None
        if threats is None or opponent_color is None
        else threats.get(side_name(opponent_color))
    )
    root_null = (
        root is None
        or root.is_check()
        or position.get("terminal") is not None
        or not isinstance(opponent_threat, dict)
    )
    check_tempo_value(
        position.get("tempo_value"),
        "/position/tempo_value",
        None if root is None else root.turn,
        root_moves,
        root_null,
        failures,
    )
    root_tempo = as_object(position.get("tempo_value"))
    tempo_dependent = (
        root_tempo is not None and root_tempo.get("label") == "tempo_dependent"
    )
    if root is None:
        return
    reliability = get_object(report.get("threats"), "null_move_reliable")
    reliable = None if reliability is None else reliability.get("value")
    for field in ("candidates", "asked_moves"):
        for index, assessment_value in enumerate(as_array(report.get(field)) or []):
            assessment = as_object(assessment_value)
            if assessment is None:
                continue
            pointer = f"/{field}/{index}"
            side = not root.turn
            move_eval = score_units(assessment.get("eval"))
            expected_moves = None if move_eval is None else 2000 - move_eval
            detailed = assessment.get("detailed") is True
            value_must_be_null = (
                not detailed or assessment.get("creates_threat_skipped") is not None
            )
            check_tempo_value(
                assessment.get("tempo_value"),
                f"{pointer}/tempo_value",
                side,
                expected_moves,
                value_must_be_null,
                failures,
            )

            if root.is_check():
                expected_skip = "in_check"
            elif reliable is False:
                expected_skip = "null_move_unreliable"
            elif not detailed:
                expected_skip = "not_detailed"
            else:
                expected_skip = None
            ledger = assessment.get("tempo_ledger")
            skip = assessment.get("tempo_ledger_skipped")
            fail(
                failures,
                f"{pointer}/tempo_ledger_skipped",
                "ledger skip reason must follow in-check, null-reliability, then detail priority",
                skip == expected_skip,
            )
            fail(
                failures,
                f"{pointer}/tempo_ledger",
                "tempo ledger must be null exactly when one of its skip rules applies",
                (ledger is None) == (expected_skip is not None),
            )
            walk = replay_walk(root, assessment)
            if walk is None:
                continue
            boards, moves = walk
            check_positional_relations(
                report, assessment, pointer, boards, moves, tempo_dependent, failures
            )
            if isinstance(ledger, dict):
                check_tempo_ledger(
                    assessment,
                    pointer,
                    boards,
                    moves,
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
    check_selection_relations(report, root, failures)
    check_playable_truncated(report, failures)
    check_position_relations(report, root, failures)
    check_tempo_and_positional_semantics(report, root, failures)
    check_terminal_relations(report, failures)
    check_urgency(report, failures)
    check_parry_relations(report, root, failures)
    if root is not None:
        for index, value in enumerate(as_array(report.get("candidates")) or []):
            check_threat_skip(value, f"/candidates/{index}", root, failures)
            _ = check_move_legality(value, f"/candidates/{index}", root, failures)
        for index, asked in enumerate(as_array(report.get("asked_moves")) or []):
            pointer = f"/asked_moves/{index}"
            check_threat_skip(asked, pointer, root, failures)
            _ = check_move_legality(asked, pointer, root, failures)
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


def relevant_motifs(motifs_value: JsonValue) -> list[JsonValue]:
    motifs = as_array(motifs_value) or []
    projected: list[JsonValue] = []
    restricted_motifs: list[tuple[tuple[JsonValue, ...], int]] = []
    identity_fields = (
        "side",
        "ply",
        "actor",
        "targets",
        "restriction",
        "contribution",
    )
    for motif_value in motifs:
        motif = as_object(motif_value)
        if motif is None or motif.get("contribution") not in ("direct", "indirect"):
            continue
        projected_motif = dict(motif)
        projected_motif["also"] = []
        tag = motif.get("tag")
        if tag in RESTRICTED_TAGS:
            identity = tuple(motif.get(field) for field in identity_fields)
            for earlier_identity, earlier_index in restricted_motifs:
                if identity == earlier_identity:
                    earlier = as_object(projected[earlier_index])
                    assert earlier is not None
                    also = as_array(earlier["also"])
                    assert also is not None
                    also.append({"tag": tag, "threat": motif.get("threat")})
                    break
            else:
                restricted_motifs.append((identity, len(projected)))
                projected.append(projected_motif)
            continue
        projected.append(projected_motif)
    return projected


def projected_threat(value: JsonValue, filter_motifs: bool) -> JsonValue:
    threat = as_object(value)
    if threat is None:
        return None
    projected = dict(threat)
    projected["motifs"] = (
        relevant_motifs(threat.get("motifs")) if filter_motifs else []
    )
    return projected


def positional_identity(change: JsonObject) -> str:
    tag = change.get("tag")
    side = change.get("side")
    pieces = as_array(change.get("pieces")) or []
    identity: list[JsonValue] = [tag, side]
    if tag in {
        "isolated_pawn",
        "doubled_pawns",
        "tripled_pawns",
        "backward_pawn",
        "passed_pawn",
        "king_shield_gap",
        "open_file_near_king",
        "half_open_file_near_king",
        "rook_open_file",
        "rook_half_open_file",
    }:
        identity.append(change.get("file"))
    elif tag == "connected_passed_pawns":
        identity.append(
            [
                square[0]
                for piece_value in pieces
                if (piece := as_object(piece_value)) is not None
                and isinstance((square := piece.get("square")), str)
                and len(square) == 2
            ]
        )
    elif tag == "pawn_islands" or tag == "material_ahead":
        identity.append(change.get("value"))
    elif tag == "pawn_chain":
        identity.append(
            [
                piece.get("square", "")
                for piece_value in pieces
                if (piece := as_object(piece_value)) is not None
            ]
        )
    elif tag == "king_zone_attacker" or tag == "outpost":
        first_piece = as_object(pieces[0]) if pieces else None
        identity.append(
            None
            if first_piece is None
            else [first_piece.get("square"), first_piece.get("piece")]
        )
    elif tag == "bad_bishop":
        first_piece = as_object(pieces[0]) if pieces else None
        square = None if first_piece is None else first_piece.get("square")
        color: int | None = None
        if isinstance(square, str) and len(square) == 2:
            color = (ord(square[0]) - ord("a") + int(square[1]) - 1) % 2
        identity.append(color)
    return json.dumps(
        identity, ensure_ascii=True, separators=(",", ":"), sort_keys=True
    )


def net_positional_changes(
    value: JsonValue, tempo_dependent: bool = False
) -> list[JsonValue]:
    changes = as_array(value) or []
    selected: list[tuple[int, JsonValue, JsonObject]] = []
    for index, change_value in enumerate(changes):
        change = as_object(change_value)
        if (
            change is not None
            and change.get("net_change") is True
            and change.get("in_use") is True
        ):
            selected.append((index, change_value, change))

    def order_key(item: tuple[int, JsonValue, JsonObject]) -> tuple[bool, int]:
        tag = item[2].get("tag")
        tag_name = tag if isinstance(tag, str) else ""
        return (
            (tag_name in TEMPO_DYNAMIC_TAGS) != tempo_dependent,
            TEMPO_STATIC_GROUPS.get(tag_name, 99),
        )

    selected.sort(key=order_key)
    return [change_value for _, change_value, _ in selected]


def projected_tempo_gained(assessment: JsonObject) -> JsonValue:
    ledger = as_object(assessment.get("tempo_ledger"))
    if ledger is None:
        return None
    gained: list[JsonValue] = []
    for ply_value in as_array(ledger.get("plies")) or []:
        ply = as_object(ply_value)
        if ply is not None and ply.get("tempo_gained") is True:
            gained.append(
                {
                    "ply": ply.get("ply"),
                    "san": ply.get("san"),
                    "side": ply.get("side"),
                    "source": "engine",
                }
            )
    return gained


def move_impact_order(
    assessment: JsonObject, projected: JsonObject, tempo_dependent: bool
) -> list[JsonValue]:
    ranks: dict[str, int] = {}
    parried = as_object(projected.get("parried_threat"))
    parried_threat = None if parried is None else as_object(parried.get("threat"))
    if parried_threat is not None:
        ranks["parried_threat"] = (
            consequence_rank(parried_threat.get("consequence")) or 0
        )
    created = as_object(projected.get("created_threat"))
    if created is not None:
        ranks["created_threat"] = consequence_rank(created.get("consequence")) or 0
    if as_array(projected.get("motifs")) or as_array(projected.get("root_motifs")):
        ranks["motifs"] = consequence_rank(assessment.get("consequence")) or 0
    tempo_gained = as_array(projected.get("tempo_gained")) or []
    initiative = as_array(projected.get("initiative")) or []
    if projected.get("tempo_value") is not None or tempo_gained or initiative:
        ranks["tempo"] = 0
    positional_dynamic = False
    positional_static = False
    for change_value in as_array(projected.get("positional")) or []:
        change = as_object(change_value)
        if change is None:
            continue
        if change.get("tag") in TEMPO_DYNAMIC_TAGS:
            positional_dynamic = True
        else:
            positional_static = True
    if positional_dynamic:
        ranks["positional_dynamic"] = 0
    if positional_static:
        ranks["positional_static"] = 0
    base_order = (
        [
            "parried_threat",
            "created_threat",
            "motifs",
            "tempo",
            "positional_dynamic",
            "positional_static",
        ]
        if tempo_dependent
        else [
            "parried_threat",
            "created_threat",
            "motifs",
            "positional_static",
            "positional_dynamic",
            "tempo",
        ]
    )
    ordered_groups: list[str] = sorted(
        ranks.keys(),
        key=lambda group: (-ranks[group], base_order.index(group)),
    )
    return [cast(JsonValue, group) for group in ordered_groups]


def game_count_for_move(report: JsonObject, uci: JsonValue) -> JsonValue:
    if not isinstance(uci, str):
        return None
    games_object = as_object(report.get("games"))
    if games_object is None:
        return None
    sources = as_array(games_object.get("by_source")) or []
    first = as_object(sources[0]) if sources else None
    if first is None:
        return None
    for game_move_value in as_array(first.get("moves")) or []:
        game_move = as_object(game_move_value)
        if game_move is not None and game_move.get("uci") == uci:
            return game_move.get("games")
    return None


def project_assessment(
    report: JsonObject,
    assessment: JsonObject,
    rank: int | None,
    opponent_threat: JsonValue,
) -> JsonObject:
    position = get_object(report, "position") or {}
    tempo_value = as_object(position.get("tempo_value"))
    tempo_dependent = (
        tempo_value is not None and tempo_value.get("label") == "tempo_dependent"
    )
    ledger = as_object(assessment.get("tempo_ledger"))
    line_fens = replay_line_fens(
        position.get("fen"), assessment.get("line"), assessment.get("walked_plies")
    )
    projected: JsonObject = {
        "rank": rank,
        "san": assessment.get("san"),
        "uci": assessment.get("uci"),
        "eval": assessment.get("eval"),
        "depth": assessment.get("depth"),
        "loss": assessment.get("loss"),
        "verdict": assessment.get("verdict"),
        "gives_check": assessment.get("gives_check"),
        "line": assessment.get("line"),
        "walked_plies": assessment.get("walked_plies"),
        **(
            {"line_fens": cast(JsonValue, line_fens)}
            if line_fens is not None
            else {}
        ),
        "consequence": assessment.get("consequence"),
        "is_tactic": assessment.get("is_tactic"),
        "allows_tactic": assessment.get("allows_tactic"),
        "opportunity": assessment.get("opportunity"),
        "motifs": relevant_motifs(assessment.get("motifs")),
        "root_motifs": relevant_motifs(assessment.get("root_motifs")),
        "tactic_labels": assessment.get("tactic_labels"),
        "positional": net_positional_changes(
            assessment.get("positional"), tempo_dependent
        ),
        "tempo_value": assessment.get("tempo_value"),
        "tempo_gained": projected_tempo_gained(assessment),
        "initiative": None if ledger is None else ledger.get("runs"),
        "impact_order": [],
        "parried_threat": None,
        "created_threat": None,
        "refutation": None,
        "games": game_count_for_move(report, assessment.get("uci")),
    }
    parry = as_object(assessment.get("parries_threat"))
    if parry is not None and parry.get("value") is True and opponent_threat is not None:
        projected["parried_threat"] = {
            "threat": projected_threat(opponent_threat, False),
            "test": parry.get("test"),
        }
    created = as_object(assessment.get("creates_threat"))
    consequence = None if created is None else as_object(created.get("consequence"))
    consequence_class = None if consequence is None else consequence.get("class")
    if created is not None and consequence_class in ("mate", "material", "promotion"):
        projected["created_threat"] = projected_threat(created, False)
    verdict = assessment.get("verdict")
    refutation = assessment.get("refutation")
    if verdict not in ("best", "okay") and refutation is not None:
        projected["refutation"] = projected_threat(refutation, True)
    projected["impact_order"] = move_impact_order(
        assessment, projected, tempo_dependent
    )
    return projected


def projected_move_document(report: JsonObject) -> JsonObject:
    input_object = as_object(report.get("input")) or {}
    input_selection = as_object(input_object.get("selection")) or {}
    engine = as_object(report.get("engine")) or {}
    position = as_object(report.get("position")) or {}
    selection: JsonObject = {
        "keywords": input_selection.get("keywords", []),
        "top": input_selection.get("top"),
        "moves": input_selection.get("moves", []),
        "lines_searched": (
            0 if position.get("terminal") is not None else engine.get("multipv", 0)
        ),
        "playable_count": sum(
            1
            for candidate_value in as_array(report.get("candidates")) or []
            if (candidate := as_object(candidate_value)) is not None
            and candidate.get("verdict") in ("best", "okay")
        ),
        "playable_truncated": report.get("playable_truncated"),
    }
    candidate_values = as_array(report.get("candidates")) or []
    asked_values = as_array(report.get("asked_moves")) or []
    keywords = as_array(input_selection.get("keywords")) or []
    top = input_selection.get("top")
    explicit_moves = as_array(input_selection.get("moves")) or []
    selected_ucis: set[str] = set()
    for index, candidate_value in enumerate(candidate_values):
        candidate = as_object(candidate_value)
        if candidate is None:
            continue
        verdict = candidate.get("verdict")
        selected = (
            ("best" in keywords and index == 0)
            or (isinstance(top, int) and not isinstance(top, bool) and index < top)
            or ("playable" in keywords and verdict in ("best", "okay"))
            or ("inaccuracies" in keywords and verdict == "inaccuracy")
            or ("mistakes" in keywords and verdict == "mistake")
            or ("blunders" in keywords and verdict == "blunder")
            or (
                "unplayable" in keywords
                and verdict in ("inaccuracy", "mistake", "blunder")
            )
            or ("all" in keywords)
            or candidate.get("uci") in explicit_moves
        )
        uci = candidate.get("uci")
        if selected and isinstance(uci, str):
            selected_ucis.add(uci)
    opponent_threat: JsonValue = None
    side_to_move = position.get("side_to_move")
    threat_color = "black" if side_to_move == "white" else "white"
    threats = as_object(report.get("threats"))
    if threats is not None:
        opponent_threat = threats.get(threat_color)
    projected_moves: list[JsonValue] = []
    for index, candidate_value in enumerate(candidate_values):
        candidate = as_object(candidate_value)
        if candidate is not None and candidate.get("uci") in selected_ucis:
            projected_moves.append(
                project_assessment(report, candidate, index + 1, opponent_threat)
            )
    for asked_value in asked_values:
        asked = as_object(asked_value)
        if asked is not None and asked.get("uci") in explicit_moves:
            projected_moves.append(
                project_assessment(report, asked, None, opponent_threat)
            )

    full_games = as_object(report.get("games")) or {}
    game_sources: list[JsonValue] = []
    for source_value in as_array(full_games.get("by_source")) or []:
        source = as_object(source_value)
        if source is not None:
            game_sources.append(
                {key: value for key, value in source.items() if key != "moves"}
            )
    full_position = as_object(report.get("position")) or {}
    position_summary: JsonObject = {
        field: full_position.get(field)
        for field in (
            "fen",
            "side_to_move",
            "in_check",
            "legal_moves",
            "terminal",
            "eval_white",
            "tempo_value",
        )
    }
    return {
        "schema_version": report.get("schema_version"),
        "document": "moves",
        "input": report.get("input"),
        "engine": report.get("engine"),
        "position": position_summary,
        "selection": selection,
        "opening": report.get("opening"),
        "games": {"source": full_games.get("source"), "by_source": game_sources},
        "moves": projected_moves,
    }


def compare_projected_values(
    expected: JsonValue,
    actual: JsonValue,
    pointer: str,
    failures: list[Failure],
) -> None:
    if isinstance(expected, dict):
        actual_object = as_object(actual)
        fail(
            failures,
            pointer,
            "projected object must match the full report",
            actual_object is not None,
        )
        if actual_object is None:
            return
        for key, value in expected.items():
            if key == "line_fens" and is_projected_move_pointer(pointer):
                continue
            child_pointer = f"{pointer}/{key}"
            if key not in actual_object:
                fail(
                    failures,
                    child_pointer,
                    "projected field is missing",
                    False,
                )
            else:
                compare_projected_values(
                    value, actual_object[key], child_pointer, failures
                )
        for key in actual_object.keys() - expected.keys():
            if key == "line_fens" and is_projected_move_pointer(pointer):
                continue
            fail(
                failures,
                f"{pointer}/{key}",
                "field is not part of the full-report projection",
                False,
            )
        return
    if isinstance(expected, list):
        actual_array = as_array(actual)
        fail(
            failures,
            pointer,
            "projected array must match the full report",
            actual_array is not None,
        )
        if actual_array is None:
            return
        fail(
            failures,
            pointer,
            f"projected array must contain {len(expected)} items",
            len(actual_array) == len(expected),
        )
        for index, (expected_item, actual_item) in enumerate(
            zip(expected, actual_array)
        ):
            compare_projected_values(
                expected_item, actual_item, f"{pointer}/{index}", failures
            )
        return
    fail(
        failures,
        pointer,
        "field must equal its §7 full-report projection",
        type(expected) is type(actual) and expected == actual,
    )


def is_projected_move_pointer(pointer: str) -> bool:
    parts = pointer.split("/")
    return len(parts) == 3 and parts[1] == "moves" and parts[2].isdigit()


def check_line_fens(moves_document: JsonValue, failures: list[Failure]) -> None:
    document = as_object(moves_document)
    if document is None:
        return
    position = as_object(document.get("position"))
    position_fen = None if position is None else position.get("fen")
    moves = as_array(document.get("moves"))
    if not isinstance(position_fen, str) or moves is None:
        return
    for move_index, move_value in enumerate(moves):
        move = as_object(move_value)
        if move is None:
            continue
        pointer = f"/moves/{move_index}"
        line = as_array(move.get("line"))
        walked_plies = move.get("walked_plies")
        if (
            line is None
            or not isinstance(walked_plies, int)
            or isinstance(walked_plies, bool)
            or walked_plies < 0
            or walked_plies > len(line)
        ):
            continue
        line_fens = as_array(move.get("line_fens"))
        fail(
            failures,
            f"{pointer}/line_fens",
            "line_fens must be an array with one FEN per walked ply",
            line_fens is not None,
        )
        if line_fens is None:
            continue
        expected_fens = replay_line_fens(position_fen, line, walked_plies)
        if expected_fens is None:
            fail(
                failures,
                f"{pointer}/line",
                "line SAN must be replayable from position.fen",
                False,
            )
            continue
        fail(
            failures,
            f"{pointer}/line_fens",
            f"line_fens must contain {walked_plies} FENs",
            len(line_fens) == walked_plies,
        )
        for ply, (expected_fen, actual_fen) in enumerate(
            zip(expected_fens, line_fens)
        ):
            fen_pointer = f"{pointer}/line_fens/{ply}"
            if not isinstance(actual_fen, str):
                fail(
                    failures,
                    fen_pointer,
                    "FEN must match the position after the corresponding SAN",
                    False,
                )
                continue
            try:
                actual_position = chess.Board(actual_fen).fen(en_passant="legal")
                expected_position = chess.Board(expected_fen).fen(
                    en_passant="legal"
                )
            except ValueError:
                fail(
                    failures,
                    fen_pointer,
                    "FEN must be parseable by python-chess",
                    False,
                )
                continue
            fail(
                failures,
                fen_pointer,
                "FEN must match the position after the corresponding SAN",
                actual_position == expected_position,
            )


def check_projection(
    full_report: JsonValue, moves_document: JsonValue, failures: list[Failure]
) -> None:
    report = as_object(full_report)
    if report is None:
        return
    compare_projected_values(
        projected_move_document(report),
        moves_document,
        "",
        failures,
    )
    check_line_fens(moves_document, failures)


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
        return "refutation must be set iff verdict is not playable"
    if path and path[-1] == "more_urgent" and "urgency" in path:
        return "more_urgent must be null iff urgency.rule is quiet"
    if path and path[-1] == "source" and "parries_threat" in path:
        return (
            "parries_threat.source must be engine iff test is searched, otherwise rule"
        )
    return error.message


def schema_for_document(base_schema: Path, document: JsonValue) -> Path:
    if document not in ("full", "moves"):
        return base_schema
    if base_schema.name not in ("report.v8.schema.json", "moves.v8.schema.json"):
        return base_schema
    schema_name = (
        "report.v8.schema.json" if document == "full" else "moves.v8.schema.json"
    )
    return base_schema.with_name(schema_name)


def validate_document_file(
    filename: Path, base_schema: Path
) -> tuple[JsonValue | None, bool]:
    try:
        instance = read_json(filename)
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        report_failure(filename, "", f"cannot read report: {error}")
        return None, False
    report = as_object(instance)
    document = None if report is None else report.get("document")
    schema_path = schema_for_document(base_schema, document)
    try:
        schema_value = read_json(schema_path)
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        report_failure(schema_path, "", f"cannot read schema: {error}")
        return instance, False
    schema = cast(JsonSchema, schema_value)
    try:
        Draft202012Validator.check_schema(schema)
    except SchemaError as error:
        report_failure(schema_path, json_pointer(error.absolute_path), error.message)
        return instance, False
    validator = Draft202012Validator(schema)
    errors = validation_errors(validator, instance)
    if errors:
        for error in nested_validation_errors(errors):
            report_failure(
                filename,
                json_pointer(error.absolute_path),
                schema_error_message(error),
            )
        return instance, False
    if document == "full":
        semantic_failures: list[Failure] = []
        check_all_semantics(instance, semantic_failures)
        for pointer, relation in semantic_failures:
            report_failure(filename, pointer, relation)
        return instance, not semantic_failures
    if document == "moves":
        return instance, True
    report_failure(filename, "/document", "document must be 'full' or 'moves'")
    return instance, False


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="validate ChessWhy full and moves documents against their v8 schemas"
    )
    _ = parser.add_argument("schema", type=Path)
    _ = parser.add_argument("reports", type=Path, nargs="*")
    _ = parser.add_argument(
        "--projection",
        type=Path,
        nargs=2,
        metavar=("FULL_REPORT", "MOVES_DOCUMENT"),
        help="validate a full report and moves document, then check their §7 projection",
    )
    arguments = parser.parse_args(argv, namespace=Arguments())
    if arguments.projection is None:
        if not arguments.reports:
            parser.error("provide at least one report or use --projection")
        failed = False
        for filename in arguments.reports:
            _, valid = validate_document_file(filename, arguments.schema)
            failed = failed or not valid
        return 1 if failed else 0
    if arguments.reports:
        parser.error("report paths cannot be combined with --projection")
    full_filename, moves_filename = arguments.projection
    full_report, full_valid = validate_document_file(full_filename, arguments.schema)
    moves_document, moves_valid = validate_document_file(
        moves_filename, arguments.schema
    )
    if not full_valid or not moves_valid:
        return 1
    full_object = as_object(full_report)
    moves_object = as_object(moves_document)
    if full_object is None or full_object.get("document") != "full":
        report_failure(
            full_filename, "/document", "projection input must be a full report"
        )
        return 1
    if moves_object is None or moves_object.get("document") != "moves":
        report_failure(
            moves_filename, "/document", "projection input must be a moves document"
        )
        return 1
    projection_failures: list[Failure] = []
    check_projection(full_report, moves_document, projection_failures)
    for pointer, relation in projection_failures:
        report_failure(moves_filename, pointer, relation)
    return 1 if projection_failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
