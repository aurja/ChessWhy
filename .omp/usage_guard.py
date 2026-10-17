#!/usr/bin/env python3
"""Poll OMP usage buckets and guard provider subscription limits.

Usage: usage_guard.py (--watch | --wait | --status) --provider ID
       [--threshold F] [--interval S]

Exit codes: 0 (ok/clear), 1 (fetch error in --status), 2 (usage error),
3 (blocked/fired), 130 (interrupted).
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
import time
from collections.abc import Sequence
from dataclasses import dataclass
from datetime import datetime
from typing import Any, cast


@dataclass(frozen=True)
class WindowResult:
    name: str
    used_fraction: float | None
    resets_at_ms: float | None
    exhausted: bool


@dataclass(frozen=True)
class AccountResult:
    provider: str
    index: int
    windows: list[WindowResult]
    blocked: bool


class UsageFetchError(Exception):
    """Raised when OMP usage cannot be fetched or decoded."""


def _as_dict(value: object) -> dict[str, Any]:
    if isinstance(value, dict):
        return cast(dict[str, Any], value)
    return {}


def _finite_number(value: object) -> float | None:
    if isinstance(value, bool) or not isinstance(value, int | float):
        return None
    try:
        number = float(value)
    except OverflowError:
        return None
    return number if math.isfinite(number) else None


def _used_fraction(amount: dict[str, Any]) -> float | None:
    fraction = _finite_number(amount.get("usedFraction"))
    if fraction is not None:
        return fraction

    used = _finite_number(amount.get("used"))
    limit = _finite_number(amount.get("limit"))
    if used is None or limit is None or limit == 0:
        return None
    return used / limit


def evaluate_reports(
    reports: object, threshold: float, provider: str
) -> tuple[list[AccountResult], bool]:
    """Evaluate usage reports; malformed optional fields are treated as absent."""
    report_items = reports if isinstance(reports, list) else []
    accounts: list[AccountResult] = []

    for index, raw_report in enumerate(report_items, start=1):
        report = _as_dict(raw_report)
        report_provider = report.get("provider")
        account_provider = (
            report_provider if isinstance(report_provider, str) else provider
        )
        metadata = _as_dict(report.get("metadata"))
        limit_reached = metadata.get("limitReached") is True
        raw_limits = report.get("limits")
        limits = raw_limits if isinstance(raw_limits, list) else []
        windows: list[WindowResult] = []

        for raw_limit in limits:
            limit = _as_dict(raw_limit)
            raw_window = _as_dict(limit.get("window"))
            raw_name = raw_window.get("id")
            if not isinstance(raw_name, str) or not raw_name:
                raw_name = limit.get("id")
            name = raw_name if isinstance(raw_name, str) and raw_name else "unknown"

            amount = _as_dict(limit.get("amount"))
            fraction = _used_fraction(amount)
            remaining = _finite_number(amount.get("remaining"))
            resets_at_ms = _finite_number(raw_window.get("resetsAt"))
            exhausted = (
                limit_reached
                or (fraction is not None and fraction >= threshold)
                or remaining == 0
            )
            windows.append(
                WindowResult(
                    name=name,
                    used_fraction=fraction,
                    resets_at_ms=resets_at_ms,
                    exhausted=exhausted,
                )
            )

        blocked = limit_reached or any(window.exhausted for window in windows)
        accounts.append(
            AccountResult(
                provider=account_provider,
                index=index,
                windows=windows,
                blocked=blocked,
            )
        )

    return accounts, bool(accounts) and all(account.blocked for account in accounts)


def _parse_threshold(raw_value: str) -> float:
    try:
        value = float(raw_value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("threshold must be a number") from error
    if not math.isfinite(value) or not 0 <= value <= 1:
        raise argparse.ArgumentTypeError("threshold must be between 0 and 1")
    return value


def _parse_interval(raw_value: str) -> float:
    try:
        value = float(raw_value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("interval must be a number") from error
    if not math.isfinite(value) or value < 30:
        raise argparse.ArgumentTypeError("interval must be at least 30 seconds")
    return value


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Guard OMP provider usage limits.")
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--watch", action="store_true", help="exit 3 when blocked")
    modes.add_argument("--wait", action="store_true", help="wait until usage is clear")
    modes.add_argument("--status", action="store_true", help="show usage once")
    parser.add_argument("--provider", required=True, help="OMP usage provider ID")
    parser.add_argument("--threshold", type=_parse_threshold, default=0.90)
    parser.add_argument("--interval", type=_parse_interval, default=180.0)
    return parser


def _run_omp(arguments: Sequence[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["omp", *arguments],
        capture_output=True,
        check=False,
        text=True,
        timeout=90,
    )


def fetch_reports(provider: str) -> list[object]:
    """Invalidate OMP's cache and fetch one provider's current reports."""
    try:
        _run_omp(["usage", "invalidate", "--provider", provider])
    except (OSError, subprocess.TimeoutExpired, UnicodeError):
        pass

    try:
        result = _run_omp(["usage", "--json", "--provider", provider])
    except subprocess.TimeoutExpired:
        raise UsageFetchError("omp usage timed out after 90 seconds") from None
    except (OSError, UnicodeError) as error:
        raise UsageFetchError(f"could not run omp usage: {error}") from None

    if result.returncode != 0:
        detail = result.stderr.strip()
        suffix = f": {detail}" if detail else ""
        raise UsageFetchError(
            f"omp usage exited with status {result.returncode}{suffix}"
        )

    try:
        payload = json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise UsageFetchError(f"omp usage returned invalid JSON: {error.msg}") from None

    response = _as_dict(payload)
    raw_reports = response.get("reports")
    if not isinstance(raw_reports, list) or not raw_reports:
        raise UsageFetchError(f"omp returned zero reports for provider {provider}")
    return cast(list[object], raw_reports)


def _fraction_text(fraction: float | None) -> str:
    return f"{fraction:.0%}" if fraction is not None else "unknown"


def _local_iso_time(timestamp_ms: float | None) -> str:
    if timestamp_ms is None:
        return "unknown"
    try:
        local_time = datetime.fromtimestamp(timestamp_ms / 1000).astimezone()
    except (OverflowError, OSError, ValueError):
        return "unknown"
    return local_time.isoformat(timespec="seconds")


def _print_status(accounts: list[AccountResult], provider: str, blocked: bool) -> None:
    for account in accounts:
        for window in account.windows:
            state = "blocked" if window.exhausted else "ok"
            print(
                f"{account.provider} {window.name} "
                f"{_fraction_text(window.used_fraction)} "
                f"{_local_iso_time(window.resets_at_ms)} {state}"
            )
    print(f"provider={provider} state={'blocked' if blocked else 'ok'}")


def _heartbeat(
    provider: str,
    state: str,
    accounts: list[AccountResult] | None = None,
) -> None:
    summaries: dict[str, str] = {}
    if accounts is not None:
        for account in accounts:
            for window in account.windows:
                summaries.setdefault(window.name, _fraction_text(window.used_fraction))
    summary = " ".join(f"{name}={fraction}" for name, fraction in summaries.items())
    suffix = f" {summary}" if summary else ""
    timestamp = datetime.now().astimezone().strftime("%H:%M:%S")
    print(f"[{timestamp}] {provider} {state}{suffix}", flush=True)


def _blocking_descriptions(accounts: list[AccountResult]) -> list[str]:
    descriptions: list[str] = []
    for account in accounts:
        blocking = [window for window in account.windows if window.exhausted]
        if not blocking and account.blocked:
            descriptions.append(
                f"account{account.index}:limitReached=unknown resetsAt=unknown"
            )
            continue
        for window in blocking:
            descriptions.append(
                f"account{account.index}:{window.name}={_fraction_text(window.used_fraction)} "
                f"resetsAt={_local_iso_time(window.resets_at_ms)}"
            )
    return descriptions


def _wait_delay(accounts: list[AccountResult], interval: float) -> float:
    reset_times = [
        window.resets_at_ms
        for account in accounts
        if account.blocked
        for window in account.windows
        if window.exhausted and window.resets_at_ms is not None
    ]
    if reset_times:
        delay = min(reset_times) / 1000 + 60 - time.time()
    else:
        delay = interval
    return max(30.0, min(interval, delay))


def _run_status(provider: str, threshold: float) -> int:
    try:
        reports = fetch_reports(provider)
    except UsageFetchError as error:
        print(f"usage guard: {error}", file=sys.stderr)
        return 1
    accounts, blocked = evaluate_reports(reports, threshold, provider)
    _print_status(accounts, provider, blocked)
    return 3 if blocked else 0


def _run_watch(provider: str, threshold: float, interval: float) -> int:
    while True:
        try:
            reports = fetch_reports(provider)
        except UsageFetchError as error:
            _heartbeat(provider, "fetch-error")
            print(
                f"usage guard warning: {error}; retrying", file=sys.stderr, flush=True
            )
            time.sleep(interval)
            continue

        accounts, blocked = evaluate_reports(reports, threshold, provider)
        _heartbeat(provider, "blocked" if blocked else "ok", accounts)
        if blocked:
            details = "; ".join(_blocking_descriptions(accounts))
            print(f"USAGE GUARD FIRED provider={provider} {details}", flush=True)
            return 3
        time.sleep(interval)


def _run_wait(provider: str, threshold: float, interval: float) -> int:
    while True:
        try:
            reports = fetch_reports(provider)
        except UsageFetchError as error:
            _heartbeat(provider, "fetch-error")
            print(
                f"usage guard warning: {error}; retrying", file=sys.stderr, flush=True
            )
            time.sleep(interval)
            continue

        accounts, blocked = evaluate_reports(reports, threshold, provider)
        _heartbeat(provider, "blocked" if blocked else "ok", accounts)
        if not blocked:
            print(f"USAGE GUARD CLEAR provider={provider}", flush=True)
            return 0
        time.sleep(_wait_delay(accounts, interval))


def main() -> int:
    arguments = _parser().parse_args()
    if arguments.status:
        return _run_status(arguments.provider, arguments.threshold)
    if arguments.watch:
        return _run_watch(arguments.provider, arguments.threshold, arguments.interval)
    return _run_wait(arguments.provider, arguments.threshold, arguments.interval)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130) from None
