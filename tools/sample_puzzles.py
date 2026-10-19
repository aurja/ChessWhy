#!/usr/bin/env python3
"""Select the deterministic M2 Lichess puzzle evaluation sample."""

from __future__ import annotations

import argparse
import csv
import hashlib
import heapq
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import TextIO, cast

SOURCE_COLUMNS = (
    "PuzzleId",
    "FEN",
    "Moves",
    "Rating",
    "RatingDeviation",
    "Popularity",
    "NbPlays",
    "Themes",
    "GameUrl",
    "OpeningTags",
    "DailyDate",
)
OUTPUT_COLUMNS = ("PuzzleId", "FEN", "Moves", "Themes", "Stratum")
MEASURED_TAGS = (
    "hangingPiece",
    "fork",
    "pin",
    "skewer",
    "discoveredAttack",
    "discoveredCheck",
    "doubleCheck",
    "backRankMate",
    "capturingDefender",
    "trappedPiece",
    "promotion",
)
BACKGROUND_SIZE = 6_000
STRATUM_SIZE = 300

# hash, source row number, PuzzleId, FEN, Moves, Themes
Record = tuple[int, int, str, str, str, str]
HeapItem = tuple[int, int, Record]


@dataclass(frozen=True)
class Arguments:
    split: str
    exclude: Path


def parse_args() -> Arguments:
    repo_root = Path(__file__).resolve().parents[1]
    default_exclude = repo_root / "tests/fixtures/puzzles/test.csv"
    parser = argparse.ArgumentParser(description=__doc__)
    _ = parser.add_argument("--split", choices=("test", "dev"), required=True)
    _ = parser.add_argument(
        "--exclude",
        type=Path,
        default=default_exclude,
        help=f"CSV whose PuzzleIds dev excludes (default: {default_exclude})",
    )
    args = parser.parse_args()
    return Arguments(split=cast(str, args.split), exclude=cast(Path, args.exclude))


def read_excluded_ids(path: Path) -> set[str]:
    with path.open(newline="", encoding="utf-8") as source:
        reader = csv.DictReader(source)
        if tuple(reader.fieldnames or ()) != OUTPUT_COLUMNS:
            raise ValueError(
                f"exclude file {path} has columns {reader.fieldnames!r}; expected {list(OUTPUT_COLUMNS)!r}"
            )
        return {row["PuzzleId"] for row in reader}


def keep_smallest(heap: list[HeapItem], record: Record, limit: int) -> None:
    # Negating both sort components makes the heap root the latest/highest hash.
    item: HeapItem = (-record[0], -record[1], record)
    if len(heap) < limit:
        heapq.heappush(heap, item)
    elif item[:2] > heap[0][:2]:
        _ = heapq.heapreplace(heap, item)


def ordered_records(heap: list[HeapItem]) -> list[Record]:
    return sorted((item[2] for item in heap), key=lambda record: record[:2])


def parse_int(row: dict[str, str | None], column: str, row_number: int) -> int:
    value = row[column]
    if value is None:
        raise ValueError(f"source row {row_number}: missing {column}")
    try:
        return int(value)
    except ValueError as error:
        raise ValueError(
            f"source row {row_number}: invalid integer in {column}: {value!r}"
        ) from error


def write_csv(records: list[tuple[Record, str]], output: TextIO) -> None:
    writer = csv.writer(output, lineterminator="\n")
    writer.writerow(OUTPUT_COLUMNS)
    for record, stratum in records:
        writer.writerow((*record[2:], stratum))


def main() -> int:
    args = parse_args()
    excluded: set[str] = set()
    if args.split == "dev":
        try:
            excluded = read_excluded_ids(args.exclude)
        except (OSError, ValueError, csv.Error) as error:
            print(f"sample_puzzles.py: {error}", file=sys.stderr)
            return 2

    reader = csv.DictReader(sys.stdin)
    if tuple(reader.fieldnames or ()) != SOURCE_COLUMNS:
        print(
            f"sample_puzzles.py: unexpected source columns {reader.fieldnames!r}; expected {list(SOURCE_COLUMNS)!r}",
            file=sys.stderr,
        )
        return 2

    background: list[HeapItem] = []
    strata: list[list[HeapItem]] = [[] for _ in MEASURED_TAGS]
    rows_read = 0
    eligible_count = 0
    excluded_count = 0

    try:
        for row_number, row in enumerate(reader, start=2):
            rows_read += 1
            if None in row or any(value is None for value in row.values()):
                raise ValueError(f"source row {row_number}: malformed CSV record")

            deviation = parse_int(row, "RatingDeviation", row_number)
            plays = parse_int(row, "NbPlays", row_number)
            popularity = parse_int(row, "Popularity", row_number)
            moves = row["Moves"]
            puzzle_id = row["PuzzleId"]
            if moves is None or puzzle_id is None:
                raise ValueError(f"source row {row_number}: missing Moves or PuzzleId")
            if (
                deviation > 80
                or plays < 1_000
                or popularity < 80
                or len(moves.split()) > 7
            ):
                continue

            eligible_count += 1
            if puzzle_id in excluded:
                excluded_count += 1
                continue

            fen = row["FEN"]
            themes = row["Themes"]
            if fen is None or themes is None:
                raise ValueError(f"source row {row_number}: missing FEN or Themes")
            digest = int.from_bytes(
                hashlib.sha256(
                    f"chesswhy-m2-{args.split}:{puzzle_id}".encode()
                ).digest(),
                "big",
            )
            record: Record = (digest, rows_read, puzzle_id, fen, moves, themes)
            keep_smallest(background, record, BACKGROUND_SIZE)

            theme_set = set(themes.split())
            for tag_index, tag in enumerate(MEASURED_TAGS):
                if tag in theme_set:
                    # All previously selected rows together number at most the first
                    # 6,000 plus 300 for each earlier stratum. Retaining that many
                    # candidates plus 300 guarantees the first 300 unchosen rows.
                    limit = BACKGROUND_SIZE + STRATUM_SIZE * (tag_index + 1)
                    keep_smallest(strata[tag_index], record, limit)

    except (csv.Error, ValueError) as error:
        print(f"sample_puzzles.py: {error}", file=sys.stderr)
        return 2

    chosen_ids = {record[2] for record in ordered_records(background)}
    selected: list[tuple[Record, str]] = [
        (record, "background") for record in ordered_records(background)
    ]
    row_counts: dict[str, int] = {"background": len(selected)}
    for tag_index, tag in enumerate(MEASURED_TAGS):
        added = 0
        for record in ordered_records(strata[tag_index]):
            if record[2] in chosen_ids:
                continue
            selected.append((record, tag))
            chosen_ids.add(record[2])
            added += 1
            if added == STRATUM_SIZE:
                break
        row_counts[tag] = added
        if added < STRATUM_SIZE:
            print(
                f"sample_puzzles.py: stratum {tag} has {added} eligible unchosen rows; expected {STRATUM_SIZE}",
                file=sys.stderr,
            )
            return 2

    background_tag_counts = {tag: 0 for tag in MEASURED_TAGS}
    for record, stratum in selected:
        if stratum != "background":
            continue
        themes = set(record[5].split())
        for tag in MEASURED_TAGS:
            if tag in themes:
                background_tag_counts[tag] += 1

    write_csv(selected, sys.stdout)
    print(f"rows read: {rows_read}", file=sys.stderr)
    print(f"eligible: {eligible_count}", file=sys.stderr)
    print(f"eligible excluded by --exclude: {excluded_count}", file=sys.stderr)
    print(
        "rows per stratum: "
        + ", ".join(f"{name}={count}" for name, count in row_counts.items()),
        file=sys.stderr,
    )
    print(
        "background tag counts: "
        + ", ".join(f"{tag}={background_tag_counts[tag]}" for tag in MEASURED_TAGS),
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
