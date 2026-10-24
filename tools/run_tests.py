#!/usr/bin/env python3
"""Run CTest verbosely while showing progress and concise failure tails."""

from __future__ import annotations

import argparse
import re
import signal
import subprocess
import sys
import time
from collections import deque
from pathlib import Path
from types import FrameType
from typing import TextIO, cast

START_LINE = re.compile(r"^\s*Start\s+(\d+):\s*(.+?)\s*$")
TEST_OUTPUT_LINE = re.compile(r"^(\d+): ?(.*)$")
COMPLETION_LINE = re.compile(r"^\s*(\d+)/(\d+)\s+Test\s+#(\d+):")
# CTest -V chatter before and between tests that carries no result.
NOISE_PATTERNS = tuple(
    re.compile(pattern)
    for pattern in (
        r"^test \d+$",
        r"^UpdateCTestConfiguration\b",
        r"^Constructing a list of tests$",
        r"^Done constructing a list of tests$",
        r"^Updating test list for fixtures$",
        r"^Added \d+ tests to meet fixture requirements$",
        r"^Checking test dependency graph",
        r"^Internal ctest changing into directory:",
        r"^Parse Config file:",
        r"^$",
    )
)
COMPLETION_STATUS = re.compile(
    r"\.{2,}\s*\*{0,3}(Passed|Failed|Timeout|Not Run|Skipped|Exception)\b"
)
TAIL_SIZE = 40


def parse_arguments() -> tuple[Path, list[str]]:
    parser = argparse.ArgumentParser(
        description="Run CTest with live progress lines and concise failure output.",
        usage="%(prog)s [--build-dir DIR] [ctest args...]",
    )
    _ = parser.add_argument("--build-dir", default="build/release", type=Path)
    arguments, ctest_arguments = parser.parse_known_args()
    if ctest_arguments and ctest_arguments[0] == "--":
        ctest_arguments = ctest_arguments[1:]
    return cast(Path, arguments.build_dir), ctest_arguments


def print_failure_tail(name: str, lines: deque[str]) -> None:
    print(f"--- last {len(lines)} lines from {name} ---", flush=True)
    for line in lines:
        print(f"  [{name}] {line}", flush=True)
    print(f"--- end {name} output ---", flush=True)


def run_ctest(build_dir: Path, ctest_arguments: list[str]) -> int:
    command = ["ctest", "--test-dir", str(build_dir), "-V", *ctest_arguments]
    try:
        process: subprocess.Popen[str] = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
    except OSError as error:
        print(f"run_tests.py: unable to start ctest: {error}", file=sys.stderr)
        return 127

    if process.stdout is None:
        process.kill()
        _ = process.wait()
        print("run_tests.py: ctest output pipe was not created", file=sys.stderr)
        return 127
    output = cast(TextIO, process.stdout)

    names: dict[int, str] = {}
    output_tails: dict[int, deque[str]] = {}
    started = time.monotonic()

    def handle_interrupt(_signal_number: int, _frame: FrameType | None) -> None:
        # CTest shares the terminal's process group and receives Ctrl-C directly.
        return

    previous_handler = signal.signal(signal.SIGINT, handle_interrupt)
    try:
        for raw_line in output:
            line = raw_line.rstrip("\r\n")
            start_match = START_LINE.match(line)
            if start_match is not None:
                names[int(start_match.group(1))] = start_match.group(2)
                continue

            output_match = TEST_OUTPUT_LINE.match(line)
            if output_match is not None:
                number = int(output_match.group(1))
                content = output_match.group(2)
                tail = output_tails.setdefault(number, deque(maxlen=TAIL_SIZE))
                tail.append(content)
                if content.startswith("progress:"):
                    name = names.get(number, f"test #{number}")
                    print(f"  [{name}] {content}", flush=True)
                continue

            completion_match = COMPLETION_LINE.match(line)
            if completion_match is not None:
                print(
                    f"{line.strip()} ({time.monotonic() - started:.1f} s elapsed)",
                    flush=True,
                )
                status_match = COMPLETION_STATUS.search(line)
                if status_match is not None and status_match.group(1) not in {
                    "Passed",
                    "Skipped",
                }:
                    number = int(completion_match.group(3))
                    name = names.get(number, f"test #{number}")
                    print_failure_tail(name, output_tails.get(number, deque[str]()))
                continue

            if not any(pattern.match(line) for pattern in NOISE_PATTERNS):
                print(line, flush=True)
        return process.wait()
    finally:
        _ = signal.signal(signal.SIGINT, previous_handler)
        output.close()


def main() -> int:
    build_dir, ctest_arguments = parse_arguments()
    return run_ctest(build_dir, ctest_arguments)


if __name__ == "__main__":
    raise SystemExit(main())
