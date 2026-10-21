#!/usr/bin/env python3
"""Render one SVG board per fixture position for Planning/definitions/FIXTURES.md.

Development-only. Uses python-chess (GPL-3.0-or-later) from the git-ignored .venv/;
ChessWhy never links or ships it. The piece drawings in the SVGs are Colin M.L.
Burnett's, used under the BSD option of their GFDL/BSD/GPL triple license.

    .venv/bin/python tools/render_fixture_boards.py

Writes Planning/definitions/boards/<ID>.svg for every F, U, T and C fixture, White at
the bottom, with an arrow for the fixture's asked move (F) or first line move (T, C).
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import cast

import chess
import chess.pgn
import chess.svg

ROOT = Path(__file__).resolve().parent.parent
FIXTURES = ROOT / "tests" / "fixtures"
OUT = ROOT / "Planning" / "definitions" / "boards"
BOARD_PX = 240

Json = dict[str, object]


def load_list(filename: str, key: str) -> list[Json]:
    document = cast(Json, json.loads((FIXTURES / filename).read_text(encoding="utf-8")))
    return cast(list[Json], document[key])


def arg_value(args: list[str], flag: str) -> str | None:
    return args[args.index(flag) + 1] if flag in args else None


def pgn_board(filename: str, game_number: int, ply: int | None) -> chess.Board:
    with (FIXTURES / filename).open(encoding="utf-8") as handle:
        for _ in range(game_number - 1):
            if chess.pgn.skip_game(handle) is False:
                raise ValueError(f"{filename}: no game {game_number}")
        game = chess.pgn.read_game(handle)
    if game is None:
        raise ValueError(f"{filename}: no game {game_number}")
    board = game.board()
    for index, move in enumerate(game.mainline_moves()):
        if ply is not None and index >= ply:
            break
        board.push(move)
    return board


def engine_fixture(fixture: Json) -> tuple[chess.Board, chess.Move | None]:
    args = cast(list[str], fixture["args"])
    fen = arg_value(args, "--fen")
    if fen is not None:
        board = chess.Board(fen)
    else:
        pgn = arg_value(args, "--pgn")
        game = arg_value(args, "--game")
        ply = arg_value(args, "--ply")
        if pgn is None:
            raise ValueError(f"{fixture['id']}: neither --fen nor --pgn")
        board = pgn_board(pgn, int(game or "1"), None if ply is None else int(ply))
    san = arg_value(args, "--move")
    return board, None if san is None else board.parse_san(san)


def line_fixture(fixture: Json) -> tuple[chess.Board, chess.Move | None]:
    board = chess.Board(cast(str, fixture["fen"]))
    line = cast(list[str], fixture.get("line_uci", []))
    return board, chess.Move.from_uci(line[0]) if line else None


def write_board(fixture_id: str, board: chess.Board, move: chess.Move | None) -> None:
    arrows = [] if move is None else [chess.svg.Arrow(move.from_square, move.to_square)]
    check = board.king(board.turn) if board.is_check() else None
    svg = chess.svg.board(board, size=BOARD_PX, arrows=arrows, check=check)
    _ = (OUT / f"{fixture_id}.svg").write_text(svg + "\n", encoding="utf-8")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for stale in OUT.glob("*.svg"):
        stale.unlink()
    for fixture in load_list("positions.json", "engine_fixtures"):
        write_board(cast(str, fixture["id"]), *engine_fixture(fixture))
    for fixture in load_list("positions.json", "unit_fixtures"):
        board = chess.Board(cast(str, fixture["fen"]))
        write_board(cast(str, fixture["id"]), board, None)
    for fixture in load_list("motifs.json", "motif_fixtures"):
        write_board(cast(str, fixture["id"]), *line_fixture(fixture))
    for fixture in load_list("tactics.json", "fixtures"):
        write_board(cast(str, fixture["id"]), *line_fixture(fixture))


if __name__ == "__main__":
    main()
