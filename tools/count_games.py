#!/usr/bin/env python3
"""Build the deterministic M3b game-count table from PGN input."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass, field
from io import TextIOWrapper
from pathlib import Path
from typing import Literal, TextIO, cast, override

import chess
import chess.pgn

# The default python-chess lexer drops unknown movetext tokens. Keep its PGN delimiters
# but pass every other token to PrefixVisitor so unreadable moves in the counted prefix
# exclude their game instead of disappearing silently.
chess.pgn.MOVETEXT_REGEX = re.compile(
    r"""(\{.*|;.*|\(|\)|\$[0-9]+|\*|1-0|0-1|1/2-1/2|[?!]{1,2}|[^\s{}();]+)""",
    re.DOTALL | re.VERBOSE,
)

MOVE_NUMBER_PREFIX = re.compile(r"\A[0-9]+\.(?:\.\.)?")
ANNOTATION_SUFFIX = re.compile(r"[!?]+\Z")

SOURCE_ID = "lichess_broadcast"
PLATFORM = "over the board (may include online events relayed as broadcasts)"
PLAYERS = "both rated 2200 or more"
CREDIT = "Lichess broadcast database, database.lichess.org, CC BY-SA 4.0"
DEFAULT_MAX_PLY = 40
DEFAULT_MIN_GAMES = 5
DEFAULT_MONTHS = "2020-01..2026-09"
ExclusionReason = Literal["variant", "setup", "rating", "result", "illegal"]
EXCLUSION_REASONS: tuple[ExclusionReason, ...] = (
    "variant",
    "setup",
    "rating",
    "result",
    "illegal",
)
RATING_FLOOR = 2200
RATING_PATTERN = re.compile(r"[0-9]+\Z")


@dataclass(frozen=True, slots=True)
class Arguments:
    max_ply: int
    min_games: int
    months: str
    inputs: tuple[Path, ...]


@dataclass(frozen=True, slots=True)
class ParsedGame:
    headers: chess.pgn.Headers
    moves: list[tuple[str, str]]
    illegal_prefix: bool


@dataclass(slots=True)
class PositionCounts:
    games: int = 0
    white: int = 0
    draws: int = 0
    black: int = 0
    moves: dict[str, int] = field(default_factory=dict)


class PrefixVisitor(chess.pgn.BaseVisitor[ParsedGame]):
    """Collect only mainline positions up to the configured ply limit."""

    def __init__(self, max_ply: int) -> None:
        self.max_ply: int = max_ply
        self.headers: chess.pgn.Headers = chess.pgn.Headers()
        self.moves: list[tuple[str, str]] = []
        self.mainline_plies: int = 0
        self.illegal_prefix: bool = False

    @override
    def begin_headers(self) -> chess.pgn.Headers:
        self.headers = chess.pgn.Headers()
        return self.headers

    @override
    def visit_header(self, tagname: str, tagvalue: str) -> None:
        self.headers[tagname] = tagvalue

    @override
    def begin_parse_san(
        self, board: chess.Board, san: str
    ) -> chess.pgn.SkipType | None:
        normalized = MOVE_NUMBER_PREFIX.sub("", san, count=1)
        if (
            not normalized
            or re.fullmatch(r"[+#]+", normalized) is not None
            or normalized in ("1-0", "0-1", "1/2-1/2", "*")
            or self.mainline_plies >= self.max_ply
        ):
            return chess.pgn.SKIP
        return None

    @override
    def parse_san(self, board: chess.Board, san: str) -> chess.Move:
        normalized = MOVE_NUMBER_PREFIX.sub("", san, count=1)
        normalized = ANNOTATION_SUFFIX.sub("", normalized)
        return board.parse_san(normalized)

    @override
    def visit_move(self, board: chess.Board, move: chess.Move) -> None:
        if move == chess.Move.null():
            self.illegal_prefix = True
        self.moves.append((position_key(board), move.uci()))
        self.mainline_plies += 1

    @override
    def begin_variation(self) -> chess.pgn.SkipType:
        return chess.pgn.SKIP

    @override
    def handle_error(self, error: Exception) -> None:
        if self.mainline_plies < self.max_ply:
            self.illegal_prefix = True

    @override
    def result(self) -> ParsedGame:
        return ParsedGame(self.headers, self.moves, self.illegal_prefix)


class GameCounter:
    def __init__(self, max_ply: int, min_games: int) -> None:
        self.max_ply: int = max_ply
        self.min_games: int = min_games
        self.games_read: int = 0
        self.games_kept: int = 0
        self.excluded: dict[ExclusionReason, int] = {
            reason: 0 for reason in EXCLUSION_REASONS
        }
        self.positions: dict[str, PositionCounts] = {}

    def add_game(self, game: ParsedGame) -> None:
        self.games_read += 1
        reason = exclusion_reason(game)
        if reason is not None:
            self.excluded[reason] += 1
            return

        self.games_kept += 1
        result = game.headers["Result"]
        seen: set[str] = set()
        for key, uci in game.moves:
            if key in seen:
                continue
            seen.add(key)
            counts = self.positions.get(key)
            if counts is None:
                counts = PositionCounts()
                self.positions[key] = counts
            counts.games += 1
            if result == "1-0":
                counts.white += 1
            elif result == "1/2-1/2":
                counts.draws += 1
            else:
                counts.black += 1
            counts.moves[uci] = counts.moves.get(uci, 0) + 1


def position_key(board: chess.Board) -> str:
    return " ".join(board.fen(en_passant="legal").split()[:4])


def is_rating_at_least(value: str | None, floor: int) -> bool:
    if value is None or RATING_PATTERN.fullmatch(value) is None:
        return False
    normalized = value.lstrip("0") or "0"
    floor_text = str(floor)
    return len(normalized) > len(floor_text) or (
        len(normalized) == len(floor_text) and normalized >= floor_text
    )


def exclusion_reason(game: ParsedGame) -> ExclusionReason | None:
    headers = game.headers
    if headers.get("Variant", "Standard") != "Standard":
        return "variant"
    if "FEN" in headers or "SetUp" in headers:
        return "setup"
    if not is_rating_at_least(
        headers.get("WhiteElo"), RATING_FLOOR
    ) or not is_rating_at_least(headers.get("BlackElo"), RATING_FLOOR):
        return "rating"
    if headers.get("Result") not in ("1-0", "0-1", "1/2-1/2"):
        return "result"
    if game.illegal_prefix:
        return "illegal"
    return None


def parse_args() -> Arguments:
    parser = argparse.ArgumentParser(description=__doc__)
    _ = parser.add_argument("--max-ply", type=int, default=DEFAULT_MAX_PLY)
    _ = parser.add_argument("--min-games", type=int, default=DEFAULT_MIN_GAMES)
    _ = parser.add_argument("--months", default=DEFAULT_MONTHS)
    _ = parser.add_argument(
        "inputs",
        metavar="PGN",
        type=Path,
        nargs="*",
        help="PGN or .pgn.zst input files (default: standard input)",
    )
    args = parser.parse_args()
    max_ply = cast(int, args.max_ply)
    min_games = cast(int, args.min_games)
    months = cast(str, args.months)
    inputs = cast(list[Path], args.inputs)
    if max_ply < 0:
        parser.error("--max-ply must be nonnegative")
    if min_games < 1:
        parser.error("--min-games must be positive")
    return Arguments(
        max_ply=max_ply,
        min_games=min_games,
        months=months,
        inputs=tuple(inputs),
    )


def read_games(source: TextIO, counter: GameCounter) -> None:
    while True:
        game = chess.pgn.read_game(
            source, Visitor=lambda: PrefixVisitor(counter.max_ply)
        )
        if game is None:
            return
        counter.add_game(game)


def read_path(path: Path, counter: GameCounter) -> None:
    if path.suffix == ".zst":
        with subprocess.Popen(
            ["zstd", "-dc", "--", str(path)],
            stdout=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
        ) as process:
            if process.stdout is None:
                raise OSError(f"zstd did not provide input for {path}")
            read_games(cast(TextIO, process.stdout), counter)
        if process.returncode != 0:
            raise OSError(f"zstd failed for {path} with status {process.returncode}")
    else:
        with path.open("r", encoding="utf-8", errors="replace") as source:
            read_games(cast(TextIO, source), counter)


def write_table(counter: GameCounter, months: str, output: TextIO) -> int:
    kept_keys = sorted(
        key
        for key, counts in counter.positions.items()
        if counts.games >= counter.min_games
    )
    excluded = " ".join(
        f"{name}={counter.excluded[name]}" for name in EXCLUSION_REASONS
    )
    header = (
        f"# source {SOURCE_ID}\n"
        f"# platform {PLATFORM}\n"
        f"# players {PLAYERS}\n"
        f"# months {months}\n"
        f"# credit {CREDIT}\n"
        f"# max_ply {counter.max_ply}\n"
        f"# min_games {counter.min_games}\n"
        f"# games_read {counter.games_read}\n"
        f"# excluded {excluded}\n"
        f"# games_kept {counter.games_kept}\n"
        f"# positions {len(kept_keys)}\n"
    )
    _ = output.write(header)
    for key in kept_keys:
        counts = counter.positions[key]
        moves = sorted(counts.moves.items(), key=lambda item: (-item[1], item[0]))
        move_text = ",".join(f"{uci}:{count}" for uci, count in moves)
        row = (
            f"{key}\t{counts.games}\t{counts.white}\t{counts.draws}\t"
            f"{counts.black}\t{move_text}\n"
        )
        _ = output.write(row)
    return len(kept_keys)


def main() -> int:
    args = parse_args()
    counter = GameCounter(args.max_ply, args.min_games)
    try:
        if args.inputs:
            for path in args.inputs:
                read_path(path, counter)
        else:
            cast(TextIOWrapper, sys.stdin).reconfigure(
                encoding="utf-8", errors="replace"
            )
            read_games(sys.stdin, counter)
        cast(TextIOWrapper, sys.stdout).reconfigure(encoding="utf-8", newline="\n")
        positions = write_table(counter, args.months, sys.stdout)
    except (OSError, UnicodeError) as error:
        print(f"count_games.py: {error}", file=sys.stderr)
        return 2

    excluded = " ".join(
        f"{name}={counter.excluded[name]}" for name in EXCLUSION_REASONS
    )
    print(f"games read: {counter.games_read}", file=sys.stderr)
    print(f"excluded: {excluded}", file=sys.stderr)
    print(f"games kept: {counter.games_kept}", file=sys.stderr)
    print(f"positions: {positions}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
