# PostM4_EM6 receipt — search phase and explanation phase

Date: 2026-10-25. Machine: `debian-home` (i7-8565U, 15 GiB, Debian 13). Branch
`postm4-em6` (built on `postm4-em5`, neither merged). Contract:
`Planning/milestones/PostM4_EM6.md` §3 (acceptance §3.9, contract changes §4). Terms:
`Planning/DBC/DEFINITIONS.md` §3.9. Started from the Captain's F47
(`2brr3/1p4k1/2p1P3/p4P2/P4pN1/2P4B/5K2/6R1 b - - 0 1`): ChessWhy said Rd3, deeper
Stockfish says Rh8.

## Delivered

- **Two budgets** (H2, H4, N1, N2): `--search-budget N` (default 18) and
  `--explanation-budget N` (default 14), depths 1–60; every search stops at a fixed depth,
  not a node count. `--budget` and `--level` are gone (H1): both are unknown flags.
- **Search phase** (H3, N3, N4, N11): A-best is MultiPV 1 over every legal move; A-rest is
  MultiPV K − 1 over every other legal move at the same depth; C runs at the search depth.
  The best move no longer depends on the width.
- **Explanation phase** (H3): B, D, E, R and every L search run at the explanation depth.
- **Verdicts** (H5, N5, N6): the at-least-score test replaces the same-score test; an A-rest
  line whose score is at least A-best's (score order, mates first) is `best` with loss 0.
- **Root opportunity** (N7): margin unclamped. **`playable_truncated`** (N8): reads A-rest's
  K − 1 lines. **Hang guard** (N9): a search fails only after 120 s with no engine output.
- **Report v10**: `schema/report.v10.schema.json`, `schema/moves.v10.schema.json`;
  `input.search_budget`, `input.explanation_budget`; no `level`, `budget` or `engine.nodes`.
  Text header `Budgets: search depth 18, explanation depth 14 [rule]`.
- **Checker**: N6's at-least test and N8 in `tools/validate_report.py`, with mutants in
  `checker_unit`.
- **Fixtures**: every F fixture re-blessed at 18/14 (F29, F30 at 12/10); F47 new
  (`--moves best,Rd3`); E14 message changed; E23–E26 new (budget flag errors).
- **Docs**: README (flags, v10, timing), vision §3 (speed has no limit for now), DBC files,
  `CURRENT_STATE.md`, this receipt.

## Re-bless (approved at checkpoint 2)

| Change | Fixtures |
|---|---|
| Best move | F15 Nf3 → Nc3; F28 d3 → O-O; F31 Qe5+ → Qd1+ (Qe5+ mate in 11 from A-rest still `best`); F44 d3 → O-O; F46 Rxd4 → Nxd4 (Rxd4 still `best`, loss 0); F47 new, Rh8 |
| Verdict | F12 Ng4 mistake → blunder (loss 0.21); F32 `best,h6` h6 mistake → inaccuracy |
| Candidate set | F05 candidate 5 h6 → Kh8 (h6 asked via C, still a blunder); F07 Kg1 no longer a candidate; F10 the only playable move is a7 |
| Other facts | F05 Ne2+ and F30 a7 become opportunities (F05 margin 0.41 over Qe8, was 0.07); F08 Black's threat Qa1+ (was Nxh5); F21, F22, F24, F25, F31, F40, F41, F42 line and text facts (record in `FIXTURES.md` "Codebase today") |

F47 at the defaults (`--moves best,Rd3`): Rh8 best (loss 0, cp −15), Rd3 best (0, −3),
Bxe6 okay (0.003, −22), Rxe6 blunder (0.3325, −113), Rd2+ mistake (0.4875, −175). At
`--moves playable` (K = 22) Rd3 is cp −18, okay, loss 0.0005: A-rest's lines still depend
on the width (width dependence).

## Timing (measurement, no limit)

`cli_timing` alone at 18/14 after the gate: pass, total 957.7 s (the checkpoint-2
measurement on a cooler machine: 821 s). Per fixture, default run, seconds:

| F | s | F | s | F | s | F | s |
|---|---:|---|---:|---|---:|---|---:|
| F12 | 60.0 | F35 | 59.0 | F25 | 53.8 | F01 | 49.4 |
| F38 | 45.8 | F40 | 45.0 | F32 | 44.2 | F28 | 41.1 |
| F05 | 40.6 | F41 | 37.7 | F44 | 36.9 | F11 | 36.0 |
| F42 | 33.8 | F15 | 33.7 | F08 | 31.7 | F46 | 29.9 |
| F47 | 28.8 | F06 | 25.4 | F07 | 25.0 | F37 | 19.9 |
| F36 | 19.3 | F03 | 18.0 | F09 | 17.7 | F18 | 17.3 |
| F33 | 17.1 | F34 | 13.8 | F04 | 11.6 | F39 | 6.6 |
| F22 | 6.5 | F21 | 6.2 | F26 | 5.6 | F17 | 5.3 |
| F29 | 5.0 | F02 | 4.3 | F27 | 4.2 | F10 | 4.0 |
| F31 | 2.9 | F30 | 2.3 | F43 | 1.9 | F24 | 1.6 |
| F20 | 1.6 | F13 | 1.6 | F14 | 1.5 | F45 | 1.4 |
| F19 | 1.4 | F23 | 1.2 | | | | |

F32 smoke (checkpoint-2 run): `--moves playable` 58.2 s, `--moves all` 126.0 s.
Phase split at checkpoint 2 (explanation budget 1 / 10 / 14, search budget 18): F01
14.4 / 19.5 / 44.5 s, F12 15.8 / 19.2 / 42.3 s, F08 16.4 / 18.6 / 32.5 s. The search phase
costs 14–16 s; the explanation phase at depth 14 costs the rest.

**Deferred, not dropped** (Captain, checkpoint 2): speed. A later milestone makes it fast;
the explanation phase (forced test MultiPV at depth 14) is the first target.

## Verification

| # | Check | Result |
|---|---|---|
| 1 | Acceptance 1–6 (CTests) | Release `unit\|engine\|games\|corpus` 225/225 in 1062 s. ASan/UBSan (Debug `-O1`) `unit\|sanitize` 185/185 in 215 s. After the clang-tidy test fixes: `unit` 170/170 |
| 2 | Acceptance 7 (`cli_timing`) | Alone: pass, 957.7 s (table above) |
| 3 | Smoke | F47 `--moves best,Rd3 --full-report --json --no-llm`: v10, budgets 18/14, candidates as above, exit 0, 18.6 s |
| 4 | Gates | `git clang-format c93d51b^` applied to the changed lines (one remaining warning is an EM5 line in `report.hpp`); `clang-tidy --quiet -p build/release` 0 diagnostics on the 15 changed units after fixing 5 EM5-era test findings (unchecked optional access, a `std::move` of a trivially copyable optional); Ruff check and format, basedpyright 0/0 on `tools/validate_report.py` and `tests/checker/unit_tests.py`; no shell script changed; `check_dbc` ok |

## Review

One completion review (reviewer on Sol 6.1): no blockers; all eight §3.9 rows met. The
reviewer re-ran the flag errors (exit 2), F47 `--moves best` against `top8` (both Rh8, cp
−15, depth 18), a rerun identity on `7k/5Q2/6K1/8/8/8/8/8 w - - 0 1` and the F47 projection
check. Two optional nits applied: the v10 schemas' verdict descriptions name the
at-least-score test (was "exact score equality"), and `engine.multipv` in the full schema
is `[rule]` as in §3.3 (was `[engine]`); `unit` 170/170 and the schema check on every saved
report pass after them. No scope proposals.

## Contract changes

See §4 of the contract: checkpoint 1 (diagrams, forced-test timing risk built as frozen);
checkpoint 2 (re-bless approved, H6 time limits removed, F47 asks `best,Rd3`, defaults stay
18/14, speed deferred).

## Checkpoints

1. Diagrams approved; forced-test timing risk: build as frozen and measure (2026-10-25).
2. Re-bless list approved; time limits removed; F47 `--moves best,Rd3`; defaults 18/14
   (2026-10-25).
