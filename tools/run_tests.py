#!/usr/bin/env python3
"""Run CTest verbosely while showing progress and concise failure tails."""

from __future__ import annotations

import argparse
import json
import os
import re
import signal
import subprocess
import sys
import time
from collections import deque
from pathlib import Path
from types import FrameType
from typing import Literal, TextIO, TypedDict, cast

START_LINE = re.compile(r"^\s*Start\s+(\d+):\s*(.+?)\s*$")
TEST_OUTPUT_LINE = re.compile(r"^(\d+): ?(.*)$")
COMPLETION_LINE = re.compile(r"^\s*(\d+)/(\d+)\s+Test\s+#(\d+):")
PROGRESS_LINE = re.compile(r"^(?:\[[^\]]+\]\s+)?progress:\s*(.*)$")


class RunningTestStatus(TypedDict):
    name: str
    started: float
    progress: str | None


class TestProgressStatus(TypedDict):
    version: int
    build_dir: str
    pid: int
    started: float
    updated: float
    state: Literal["running", "finished"]
    exit_code: int | None
    done: int
    total: int | None
    failed: list[str]
    running: list[RunningTestStatus]


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


def parse_arguments() -> tuple[Path, Path, list[str]]:
    parser = argparse.ArgumentParser(
        description="Run CTest with live progress lines and concise failure output.",
        usage="%(prog)s [--build-dir DIR] [--status-file PATH] [ctest args...]",
    )
    _ = parser.add_argument("--build-dir", default="build/release", type=Path)
    _ = parser.add_argument("--status-file", type=Path)
    arguments, ctest_arguments = parser.parse_known_args()
    if ctest_arguments and ctest_arguments[0] == "--":
        ctest_arguments = ctest_arguments[1:]
    build_dir = cast(Path, arguments.build_dir)
    status_file = cast(Path | None, arguments.status_file)
    if status_file is None:
        status_file = build_dir / "test_progress.json"
    return build_dir, status_file, ctest_arguments


def print_failure_tail(name: str, lines: deque[str]) -> None:
    print(f"--- last {len(lines)} lines from {name} ---", flush=True)
    for line in lines:
        print(f"  [{name}] {line}", flush=True)
    print(f"--- end {name} output ---", flush=True)


def write_status(
    status_file: Path,
    status: TestProgressStatus,
    last_write: float | None,
    *,
    force: bool = False,
) -> float | None:
    now = time.monotonic()
    if not force and last_write is not None and now - last_write < 0.5:
        return last_write

    status["updated"] = time.time()
    temporary_file = status_file.with_name(f".{status_file.name}.{os.getpid()}.tmp")
    try:
        status_file.parent.mkdir(parents=True, exist_ok=True)
        with temporary_file.open("w", encoding="utf-8") as output:
            json.dump(status, output, separators=(",", ":"))
            _ = output.write("\n")
        os.replace(temporary_file, status_file)
    except OSError as error:
        print(f"run_tests.py: unable to write status file: {error}", file=sys.stderr)
        try:
            temporary_file.unlink(missing_ok=True)
        except OSError:
            pass
        return last_write
    return now


def run_ctest(build_dir: Path, status_file: Path, ctest_arguments: list[str]) -> int:
    command = ["ctest", "--test-dir", str(build_dir), "-V", *ctest_arguments]
    now = time.time()
    status: TestProgressStatus = {
        "version": 1,
        "build_dir": str(build_dir),
        "pid": os.getpid(),
        "started": now,
        "updated": now,
        "state": "running",
        "exit_code": None,
        "done": 0,
        "total": None,
        "failed": [],
        "running": [],
    }
    last_write = write_status(status_file, status, None, force=True)
    running: dict[int, RunningTestStatus] = {}
    exit_code = 1

    try:
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
            exit_code = 127
            return exit_code

        if process.stdout is None:
            process.kill()
            _ = process.wait()
            print("run_tests.py: ctest output pipe was not created", file=sys.stderr)
            exit_code = 127
            return exit_code
        output = cast(TextIO, process.stdout)

        names: dict[int, str] = {}
        output_tails: dict[int, deque[str]] = {}
        started = time.monotonic()

        def handle_interrupt(_signal_number: int, _frame: FrameType | None) -> None:
            # CTest shares the terminal's process group and receives these signals.
            return

        previous_sigint_handler = signal.signal(signal.SIGINT, handle_interrupt)
        previous_sigterm_handler = signal.signal(signal.SIGTERM, handle_interrupt)
        try:
            for raw_line in output:
                line = raw_line.rstrip("\r\n")
                start_match = START_LINE.match(line)
                if start_match is not None:
                    number = int(start_match.group(1))
                    name = start_match.group(2)
                    names[number] = name
                    running[number] = {
                        "name": name,
                        "started": time.time(),
                        "progress": None,
                    }
                    status["running"] = list(running.values())
                    last_write = write_status(
                        status_file, status, last_write, force=True
                    )
                    continue

                output_match = TEST_OUTPUT_LINE.match(line)
                if output_match is not None:
                    number = int(output_match.group(1))
                    content = output_match.group(2)
                    tail = output_tails.setdefault(number, deque(maxlen=TAIL_SIZE))
                    tail.append(content)
                    progress_match = PROGRESS_LINE.match(content)
                    if progress_match is not None and number in running:
                        running[number]["progress"] = progress_match.group(1).strip()
                        status["running"] = list(running.values())
                        last_write = write_status(status_file, status, last_write)
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
                    number = int(completion_match.group(3))
                    name = names.get(number, f"test #{number}")
                    status["done"] += 1
                    status["total"] = int(completion_match.group(2))
                    _ = running.pop(number, None)
                    status["running"] = list(running.values())
                    status_match = COMPLETION_STATUS.search(line)
                    if status_match is not None and status_match.group(1) not in {
                        "Passed",
                        "Skipped",
                    }:
                        status["failed"].append(f"{name} ({status_match.group(1)})")
                        print_failure_tail(name, output_tails.get(number, deque[str]()))
                    last_write = write_status(
                        status_file, status, last_write, force=True
                    )
                    continue

                if not any(pattern.match(line) for pattern in NOISE_PATTERNS):
                    print(line, flush=True)
            exit_code = process.wait()
            return exit_code
        finally:
            _ = signal.signal(signal.SIGINT, previous_sigint_handler)
            _ = signal.signal(signal.SIGTERM, previous_sigterm_handler)
            output.close()
    finally:
        status["state"] = "finished"
        status["exit_code"] = exit_code
        status["running"] = []
        _ = write_status(status_file, status, last_write, force=True)


def main() -> int:
    build_dir, status_file, ctest_arguments = parse_arguments()
    return run_ctest(build_dir, status_file, ctest_arguments)


if __name__ == "__main__":
    raise SystemExit(main())
