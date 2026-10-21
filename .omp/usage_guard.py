#!/usr/bin/env python3
"""Guard provider subscription limits using the usage data OMP already fetched.

Reads OMP's usage cache (``agent.db`` table ``cache``) read-only. Running OMP
sessions refresh that cache every few minutes and from rate-limit response
headers; the guard itself never calls a usage API, so it cannot be throttled
and cannot discard OMP's last good report.

Cache format this script expects (OMP 18.12; not a public OMP interface):
  file   $PI_CODING_AGENT_DIR/agent.db, default ~/.omp/agent/agent.db
  table  cache(key TEXT, value TEXT, expires_at INTEGER)
  key    usage_cache:report:[<cache version>:]<provider>:<base url>:<auth>|<account>
  value  {"value": {"provider", "fetchedAt": ms, "metadata": {"limitReached"},
          "limits": [{"id", "window": {"id", "resetsAt": ms},
                      "amount": {"usedFraction" | "used"+"limit", "remaining"}}]}}
If OMP changes it, the guard fails loudly (exit 1) and prints what it read.
Inspect the real cache (sqlite3 -readonly on the file above) and adapt the
constants below and read_reports() / evaluate_reports(), then update this
block. Never fix it by calling the usage API.

Usage: usage_guard.py (--watch | --wait | --status) --provider ID
       [--threshold F] [--interval S]

Exit codes: 0 (ok/clear), 1 (cache unreadable: at once in --status, after
3 consecutive failures in --watch/--wait), 2 (usage error), 3 (blocked/fired),
130 (interrupted).
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import sqlite3
import sys
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, cast

CACHE_TABLE = "cache"
USAGE_KEY_PREFIX = "usage_cache:"
REPORT_KEY_PREFIX = "usage_cache:report:"
# Key tail after the prefix: optional cache version, then the provider ID.
REPORT_KEY_PROVIDER = re.compile(r"(?:\d+:)?(?P<provider>[^:]+):")
EXPECTED_VALUE = (
    '{"value": {"fetchedAt": ms, "limits": [{"window": {"id", "resetsAt"}, '
    '"amount": {"usedFraction"}}]}}'
)
REPAIR_HINT = (
    "omp's usage cache format may have changed: edit .omp/usage_guard.py to "
    "match it (expected format in its docstring)."
)
# OMP serves a last good report for 24 h after its fetchedAt; older is gone.
LAST_GOOD_MAX_AGE_MS = 24 * 60 * 60 * 1000
# OMP sessions refresh every 5 min; three missed refreshes mark data stale.
STALE_AFTER_S = 15 * 60
MAX_CONSECUTIVE_ERRORS = 3


@dataclass(frozen=True)
class WindowResult:
    name: str
    used_fraction: float | None
    resets_at_ms: float | None
    reset: bool
    exhausted: bool


@dataclass(frozen=True)
class AccountResult:
    provider: str
    index: int
    windows: list[WindowResult]
    age_s: float | None
    blocked: bool


class UsageFetchError(Exception):
    """Raised when OMP's usage cache cannot be read or holds no usable report."""


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
    reports: object, threshold: float, provider: str, now_ms: float
) -> tuple[list[AccountResult], bool]:
    """Evaluate usage reports; malformed optional fields are treated as absent.

    A window whose reset time has passed is never exhausted: its cached usage
    predates the reset.
    """
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
            reset = resets_at_ms is not None and resets_at_ms <= now_ms
            exhausted = not reset and (
                limit_reached
                or (fraction is not None and fraction >= threshold)
                or remaining == 0
            )
            windows.append(
                WindowResult(
                    name=name,
                    used_fraction=fraction,
                    resets_at_ms=resets_at_ms,
                    reset=reset,
                    exhausted=exhausted,
                )
            )

        fetched_at_ms = _finite_number(report.get("fetchedAt"))
        age_s = None if fetched_at_ms is None else (now_ms - fetched_at_ms) / 1000
        blocked = any(window.exhausted for window in windows) or (
            limit_reached and not windows
        )
        accounts.append(
            AccountResult(
                provider=account_provider,
                index=index,
                windows=windows,
                age_s=age_s,
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
    parser.add_argument("--interval", type=_parse_interval, default=300.0)
    return parser


def _agent_db_path() -> Path:
    agent_dir = os.environ.get("PI_CODING_AGENT_DIR")
    base = Path(agent_dir).expanduser() if agent_dir else Path.home() / ".omp/agent"
    return base / "agent.db"


def _key_pattern(provider: str) -> str:
    return f"{REPORT_KEY_PREFIX}[<n>:]{provider}:…"


def read_source(provider: str) -> str:
    """Describe where and what the guard reads, for failure messages."""
    return (
        f"read {_agent_db_path()} table {CACHE_TABLE} key {_key_pattern(provider)} "
        f"expecting value {EXPECTED_VALUE}"
    )


def _has_usable_limit(report: dict[str, Any]) -> bool:
    limits = report.get("limits")
    if not isinstance(limits, list):
        return False
    return any(
        _used_fraction(_as_dict(_as_dict(limit).get("amount"))) is not None
        for limit in cast(list[object], limits)
    )


def read_reports(provider: str, now_ms: float) -> list[object]:
    """Return the provider's cached OMP usage reports that OMP would still serve.

    Raises UsageFetchError naming the file, table and key, and what was found
    instead, when no usable report exists or a matching entry has an
    unexpected shape.
    """
    path = _agent_db_path()
    if not path.is_file():
        raise UsageFetchError(
            f"{path} does not exist (omp's agent dir moved? set PI_CODING_AGENT_DIR)"
        )
    try:
        connection = sqlite3.connect(f"{path.as_uri()}?mode=ro", uri=True, timeout=10)
        try:
            rows = cast(
                list[tuple[object, object]],
                connection.execute(
                    f"SELECT key, value FROM {CACHE_TABLE} WHERE key LIKE ? ORDER BY key",
                    (f"{USAGE_KEY_PREFIX}%",),
                ).fetchall(),
            )
        finally:
            connection.close()
    except sqlite3.Error as error:
        raise UsageFetchError(
            f"could not query table {CACHE_TABLE} in {path}: {error}"
        ) from None

    reports: list[object] = []
    malformed: list[str] = []
    newest_old_ms: float | None = None
    families: set[str] = set()
    for key, value in rows:
        if not isinstance(key, str):
            continue
        families.add(key.split("|", 1)[0])
        if not key.startswith(REPORT_KEY_PREFIX):
            continue
        match = REPORT_KEY_PROVIDER.match(key, len(REPORT_KEY_PREFIX))
        if match is None or match["provider"] != provider:
            continue
        try:
            entry = _as_dict(json.loads(value) if isinstance(value, str) else None)
        except json.JSONDecodeError:
            entry = {}
        report = _as_dict(entry.get("value"))
        fetched_at_ms = _finite_number(report.get("fetchedAt"))
        if fetched_at_ms is None or not _has_usable_limit(report):
            malformed.append(key.split("|", 1)[0])
            continue
        if now_ms - fetched_at_ms > LAST_GOOD_MAX_AGE_MS:
            newest_old_ms = max(fetched_at_ms, newest_old_ms or fetched_at_ms)
            continue
        reports.append(report)

    if malformed:
        raise UsageFetchError(
            f"{len(malformed)} entr{'y' if len(malformed) == 1 else 'ies'} "
            f"in {path} table {CACHE_TABLE} match {_key_pattern(provider)} "
            f"but lack value.fetchedAt or a limit with a used fraction "
            f"(first: {malformed[0]}); the value format changed"
        )
    if reports:
        return reports
    if newest_old_ms is not None:
        raise UsageFetchError(
            f"newest {provider} report in {path} was fetched "
            f"{_local_iso_time(newest_old_ms)}, over 24 h ago; "
            "start an omp session so it refreshes usage"
        )
    present = ", ".join(sorted(families)) or "none"
    raise UsageFetchError(
        f"no key in {path} table {CACHE_TABLE} matches {_key_pattern(provider)}; "
        f"{USAGE_KEY_PREFIX}* keys present: {present} "
        "(no omp session has fetched usage yet, or the key format changed)"
    )


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


def _age_text(accounts: list[AccountResult]) -> str:
    ages = [account.age_s for account in accounts if account.age_s is not None]
    if not ages:
        return "age=unknown stale"
    oldest = max(ages)
    stale = " stale" if oldest > STALE_AFTER_S else ""
    return f"age={oldest:.0f}s{stale}"


def _window_state(window: WindowResult) -> str:
    if window.reset:
        return "reset"
    return "blocked" if window.exhausted else "ok"


def _print_status(accounts: list[AccountResult], provider: str, blocked: bool) -> None:
    for account in accounts:
        for window in account.windows:
            print(
                f"{account.provider} {window.name} "
                f"{_fraction_text(window.used_fraction)} "
                f"{_local_iso_time(window.resets_at_ms)} {_window_state(window)}"
            )
    print(
        f"provider={provider} state={'blocked' if blocked else 'ok'} "
        f"{_age_text(accounts)}"
    )


def _heartbeat(
    provider: str,
    state: str,
    accounts: list[AccountResult] | None = None,
) -> None:
    parts = [provider, state]
    if accounts is not None:
        summaries: dict[str, str] = {}
        for account in accounts:
            for window in account.windows:
                text = "reset" if window.reset else _fraction_text(window.used_fraction)
                summaries.setdefault(window.name, text)
        parts.extend(f"{name}={text}" for name, text in summaries.items())
        parts.append(_age_text(accounts))
    timestamp = datetime.now().astimezone().strftime("%H:%M:%S")
    print(f"[{timestamp}] {' '.join(parts)}", flush=True)


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
    now_ms = time.time() * 1000
    try:
        reports = read_reports(provider, now_ms)
    except UsageFetchError as error:
        print(f"usage guard: {error}", file=sys.stderr)
        print(f"usage guard: {read_source(provider)}", file=sys.stderr)
        print(f"usage guard: {REPAIR_HINT}", file=sys.stderr)
        return 1
    accounts, blocked = evaluate_reports(reports, threshold, provider, now_ms)
    _print_status(accounts, provider, blocked)
    return 3 if blocked else 0


def _run_loop(provider: str, threshold: float, interval: float, wait: bool) -> int:
    """--watch (wait=False) exits 3 when blocked; --wait exits 0 when clear."""
    errors = 0
    while True:
        now_ms = time.time() * 1000
        try:
            reports = read_reports(provider, now_ms)
        except UsageFetchError as error:
            errors += 1
            _heartbeat(provider, "read-error")
            if errors >= MAX_CONSECUTIVE_ERRORS:
                print(
                    f"USAGE GUARD BLIND provider={provider} "
                    f"after {errors} consecutive read errors: {error}",
                    flush=True,
                )
                print(f"usage guard: {read_source(provider)}", flush=True)
                print(f"usage guard: {REPAIR_HINT}", flush=True)
                return 1
            print(
                f"usage guard warning: {error}; "
                f"retrying ({errors}/{MAX_CONSECUTIVE_ERRORS})",
                file=sys.stderr,
                flush=True,
            )
            time.sleep(interval)
            continue
        errors = 0

        accounts, blocked = evaluate_reports(reports, threshold, provider, now_ms)
        _heartbeat(provider, "blocked" if blocked else "ok", accounts)
        if wait and not blocked:
            print(f"USAGE GUARD CLEAR provider={provider}", flush=True)
            return 0
        if not wait and blocked:
            details = "; ".join(_blocking_descriptions(accounts))
            print(f"USAGE GUARD FIRED provider={provider} {details}", flush=True)
            return 3
        time.sleep(_wait_delay(accounts, interval) if wait else interval)


def main() -> int:
    arguments = _parser().parse_args()
    if arguments.status:
        return _run_status(arguments.provider, arguments.threshold)
    return _run_loop(
        arguments.provider, arguments.threshold, arguments.interval, arguments.wait
    )


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130) from None
