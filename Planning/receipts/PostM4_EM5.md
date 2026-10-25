# PostM4_EM5 receipt, part 1 — reply evidence

Date: 2026-10-25. Machine: `debian-home` (i7-8565U, 15 GiB, Debian 13). Branch
`postm4-em5`. Contract: `Planning/milestones/PostM4_EM5.md` §3 (acceptance §3.11, contract
changes §4). Terms: `Planning/DBC/DEFINITIONS.md` §3.7. Parts 2 and 3 (D10–D16 and the
block approval) are not started; this receipt covers part 1 only.

## Delivered

- **Terminal draw and the skipped refutation** (P1–P2, R5): a walked line that ends in
  stalemate or insufficient material records `terminal_draw` (kind and ply); the move that
  ends the game gets `refutation_skipped` and `creates_threat_skipped` `game_over` (F45 Qxg3,
  F21 Nxf6+).
- **Settled walk and settle point** (P3–P4, R6): `walked_plies` reads the line on to its
  settle point; F07 Kg1 is read to ply 8 (material −6, was +3 at ply 6).
- **Reply evidence** (P5–P7, R7–R9): every reply to a threatening ply has a reply gain
  (material through the settle point) and a forced test (the MultiPV replies, acceptable or
  not, stopping the threat or not, P6 option A). The ply labels `gaining_reply`,
  `unforced_reply` and `unresolved_reply` join `pure_reply`; a tempo is gained only against
  a pure reply. F41's Nxc2 becomes a gaining reply and F42's dxc5 an unforced reply: both
  lose their tempo gained and their initiative run.
- **Run endings and outcome** (P8–P9, R9, R12): runs end `answered` or `unresolved`; outcome
  `unknown` for an unfinished run (F46).
- **Positional evidence** (P10–P11, R10–R11): `captured` on a disappearance by capture (F25,
  F46); the king-file shelter witness presses the king on the changed file (PU19 cut, PU20
  kept; PU10's `king_shield_gap` g becomes cut).
- **Report v9**: `schema/report.v9.schema.json`, `schema/moves.v9.schema.json`.
- **Texts**: the full report text prints the walk suffixes, terminal draws, the skipped
  refutation, the new labels with the reply gain and, from checkpoint 2, the forced-test
  suffix (F42 `dxc5 unforced reply (acceptable, leaves the threat: Qxc5; 5 of 34 replies
  searched)`, F28 `f6 unresolved reply (…; 5 of 36 replies searched)`); the move report text
  prints tempo rows in the position walk (§3.8, V2 and F40).
- **Checker**: P1–P11 re-derived independently in `tools/validate_report.py`, one mutant per
  rule in `checker_unit`.
- **Fixtures**: F45, F46, PU19, PU20 new; F07, F21, F25, F28, F40, F41, F42 re-blessed (26
  paths, approved at checkpoint 2).
- **Docs**: README (v9 fields and rows), DBC files, `CURRENT_STATE.md`, this receipt.

## Verification

| # | Check | Result |
|---|---|---|
| 1 | Acceptance 1–6 (CTests) | Release `unit\|engine\|games\|corpus` 224/224 in 381 s; after the clang-tidy fixes 224/224 in 494 s (under outside load). ASan/UBSan (Debug `-O1`) `unit\|sanitize` 185/185 in 130 s, no `runtime error` or `AddressSanitizer` line. After the review fix: `checker_unit` and `report_schema` pass |
| 2 | Acceptance 7 (`cli_timing`) | Alone, after the fixes: pass in 309 s, slowest F12 13.1 s (limit 15 s). Two earlier runs failed (F11 18.9 s, F12 17.6 s) while the Captain's `chessproblems` jobs held a core; not counted. F32 `--moves playable` 24.6 / 28.4 / 29.9 s (limit 30 s) |
| 3 | Smoke | F45 `--moves Qxg3 --full-report`: `facts read to ply 1 of 1 (stalemate)`, `creates threat: not searched (game over)`, `Refuted by: none, the move ends the game`. F46 `--moves Rxd4`: `Initiative: Black from ply 3, 2 tempi; unfinished at ply 6; outcome unknown`. F42 `--moves Qc4 --full-report`: the forced-test suffix above |
| 4 | Gates | clang-format clean on the changed lines (`git clang-format --diff main`); `clang-tidy --quiet -p build/release` 0 diagnostics on the six changed units after fixes; Ruff (check and format) and basedpyright 0/0 on `tools/validate_report.py` and `tests/checker/`; no shell script changed; `check_dbc` ok |

The first static pass found 550 unformatted changed lines, 8 clang-tidy diagnostics (a loop
convert, a branch clone in the ply labels, a nested conditional in the run outcome, implicit
bool-to-int conversions, a cast without `auto`), 4 Ruff findings and 6 basedpyright errors
in the checker tests. Ruff's B023 (closure over loop variables) was not a runtime bug: the
closure runs inside `any(...)` in the same iteration. Fixed in `6426a58`; the second gate
run followed it.

## Review

One completion review (reviewer on Sol 6.1). Two blockers:

1. Acceptance 4: the checker rejected a correct `in_use: false` for `king_zone_attacker`
   after an en passant capture (`8/8/3B4/4Pp2/6k1/8/8/K7 w - f6 0 1 --moves exf6`). Root:
   `king_pressed` used the removed pawn's square for every witness, not only the king-file
   shelter witness. Fixed in `5893c1d` with two checker tests; recheck closed.
2. Acceptance 7: `cli_timing` failed. The failing run shared the machine with outside jobs;
   the rerun alone passed (row 2). Closed by the rerun.

Scope proposals: P11's wording for the en passant square of the shelter witness (part 2
table); `in_use` coverage in the positional oracle. Nits applied: `UtCastle` wording in
`CONCEPTS.md`.

## Contract changes

See §4 of the contract: P8 (ply `e + 1` is never a pure reply), PU10 cut under P11, P6
short MultiPV (m = lines returned), §3.8 sample `17.2%`, checkpoint 2 (forced-test suffix in
§3.7; the forced test's cost and the collector audit go to part 2).

## Checkpoints

1. Diagrams, P6 option A and the §3.8 sample approved 2026-10-25 (D1–D9).
2. Re-bless list approved; timing option B (budgets unchanged, cost work in part 2);
   forced-test suffix option A; P6 short MultiPV and `17.2%` ratified (2026-10-25).
