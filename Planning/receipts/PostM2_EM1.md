# PostM2_EM1 receipt — tactic recognition, motif contribution, tactical pin

Date: 2026-10-21. Machine: `debian-home` (i7-8565U, 15 GiB, Debian 13). Branch
`postm2-em1`. Contract: `Planning/milestones/PostM2_EM1.md` (acceptance §14, contract
changes §15). Terms: `Planning/definitions/TERMINOLOGY.md` (now `Planning/DBC/DEFINITIONS.md`).

## Delivered

- **Search R(m)** (§3): `O`'s best alternative to a refutation's first move; budgets
  gain column R; fixed search order, each search after `ucinewgame`.
- **Tactic judgement** (§4.1): the best move's line and every refutation carry `tactic`
  with source `engine`: the next-best first move, its eval, the margin and `is_tactic`
  (margin ≥ 0.13, `is_tactic_margin`); a lone legal move is `forced` with source `rule`.
- **Contributions** (§4.3, rule as changed at checkpoint 2): on a tactic line every motif
  of the tactic side is `direct` (a move motif, or a later capture by X of a Y piece of
  the motif), `indirect` (a Y piece of the motif, still in place, attacks where X lands),
  or `none`. Annotations only: verdicts, lines and motif lists are unchanged.
- **`removal_of_defender`** (§4.4) as a tactic label (after lichess-puzzler's
  `capturing_defender`).
- **`pin_tactical`** (§4.5, batch 3, Captain 2026-10-21): a Y piece held in place because
  removing it enables an X threat of nine kinds (the seven M2 move motifs, checkmate,
  capture), empty guarded squares included. `Motif.threat {kind, square}`; identity
  includes `threat`; `MotifOptions` switches it off for the puzzle evaluation.
- **Report v3** (`schema/report.v3.schema.json`): `tactic`, `contribution`, `threat`,
  `engine.nodes.R`. Template prints tactic lines and contributions; pins, overloaded
  defenders and tactical pins print as `restricted movement (…)`, e.g.
  `restricted movement (tactic: Nd5-f4 fork): n d5 → P g3, Q e2, K g2 (ply 1, direct) [rule]`.
- **Fixtures**: C01–C16 (hand-labelled contributions and labels, `tactics.json`), F21–F24
  (new F rows), T42–T56 (`pin_tactical`; T54–T56 added at the gate), C09's two
  tactical-pin rows.
- **Reference implementation** of §4.5 in `tests/tactics/oracle/`, written from the plan
  text without reading the detector; `tactics_oracle` compares `threat` and samples
  `pin_tactical` on every 16th puzzle and on every T fixture.
- **Definitions**: `Planning/definitions/` (now `Planning/DBC/`; TERMINOLOGY, now
  DEFINITIONS, FIXTURES with rendered boards, TESTS) created during the milestone and kept
  current.

## Verification

| # | Check | Result |
|---|---|---|
| 1 | Release and ASan/UBSan builds; CTests | Release warning-free, 111/111 (84 `unit`, 27 `engine`, engine serial). ASan/UBSan 111/111 (`unit` 1,711 s, `engine` 394 s); `tactics_oracle` carries CTest `TIMEOUT 3600` (about 24 min under ASan) |
| 2 | Mechanical tactic recognition | `is_tactic_margin` boundary tests pass; §9 invariant check holds on every F report |
| 3 | C fixtures | C01–C16 pass: contributions exact, labels as listed (after checkpoint 2); `pin_tactical` checked where listed (C09) |
| 4 | F fixtures | every M1/M2 expectation and new row passes; every F report validates against v3 |
| 5 | Oracle and recall | `tactics_oracle` 0 mismatches: test 62,678 lists, 582 sampled puzzles, 26,180 `pin_tactical` instances compared (detector + reference); T fixtures 127 lists, 130 instances; dev (`build/puzzles/dev.csv`, by hand) 62,588 lists, 0 mismatches, 24,610 instances. `tactics_puzzles` targets met (M2 motifs unchanged; table identical to the M2 receipt) |
| 6 | Rerun identity | `cli_rerun_identity` passes; smoke command twice gives byte-identical JSON |
| 7 | `standard` wall time per F fixture | slowest F11 5.87 s and F12 5.87 s (limit 12 s), Release |
| 8 | Facts first | judgements `engine` (`rule` when forced); contributions, labels and motifs `rule`; golden render tests pass |
| 9 | Gates | clang-format and `clang-tidy --quiet -p build/release` clean on all changed C++; Ruff and basedpyright clean on `tools/render_fixture_boards.py`; no shell script changed |
| 10 | Docs | README, `third_party/DEPENDENCIES.md`, vision §5.5/§6/§11, `CURRENT_STATE.md`, definitions, this receipt |

Smoke (`--fen 'k7/8/8/8/3Q3n/8/2P5/4K3 w - - 0 1' --move c3 --budget standard --json
--no-llm`): `candidates[0].tactic.is_tactic` false; the refutation is a tactic with margin
0.4895; its ply-1 fork (knight f3 → e1, d4) is `direct`; the report validates against v3.

### Puzzle check (§7, not gating)

Direct share: on the theme's stratum, among puzzles where the theme is predicted, the
share with at least one mapped solver instance judged `direct` (`pin_tactical` off).

| Theme | Direct share test | Direct share dev |
|---|---|---|
| hangingPiece | 1.000 | 1.000 |
| fork | 1.000 | 1.000 |
| pin | 0.515 | 0.560 |
| skewer | 1.000 | 1.000 |
| discoveredAttack | 1.000 | 1.000 |
| discoveredCheck | 1.000 | 1.000 |
| doubleCheck | 1.000 | 1.000 |
| backRankMate | 1.000 | 1.000 |
| trappedPiece | 1.000 | 0.997 |
| promotion | 1.000 | 1.000 |

`removal_of_defender`: recall on the `capturingDefender` stratum test 0.740 (222/300), dev
0.700 (210/300); Lichess agreement on background test 0.240 (56/233), dev 0.194 (40/206).

The pin share was 0.783 on test after batch 2. Checkpoint 2 then restricted `direct` to
captures by X, and the share is now 0.515. [INFERENCE] The drop comes from that rule:
a pin is no longer `direct` when X never captures the pinned piece on the line.

### `pin_tactical` volume and cost

Measured on the prototype before the contract (9,300 test puzzles, 44,078 boards):
2.50 instances per board; 9.6 more reported motifs per puzzle (M2 tags 3.7); 17.8 ms per
board against 19 µs for the M2 motifs. The Captain accepted the speed (no budget) with
corpus sampling. Effects: `tactics_oracle` takes about 23 s in Release (was 12 s) and
about 24 min under ASan; F fixtures stay under 6 s. 82.7 % of `overloaded_defender`
instances also appear as a `pin_tactical` restriction ([INFERENCE] the rest fall under
§4.5 skips: a king in check, an absolutely pinned piece, no legal capture).

## Contract changes

All in §15: plan freeze; `engine.nodes.R`; C fixture format; asked-move copy of
`candidates[0]`; indirect coverage reduced at checkpoint 1; contribution rule 2/3 and C
hand labels at checkpoint 2; `pin_tactical`, `threat`, `MotifOptions`; restricted-movement
wording; `pin_tactical` exempt from the C exact set with corpus sampling. At the gate,
two B−p consistency rules, both found by the ASan/Debug oracle run aborting inside
chess-library:

- B−p clears the en passant square. The reference aborted on a capture with no pawn to
  take; the detector already cleared the square. T54.
- `pin_tactical` skips B when either king is in check (before: only Y's), and skips p
  when removing it checks X (Captain's decision). The detector aborted on three
  checkers: X was already in double check, and removing p opened a third line (puzzle
  pPHqU). T55 and T56. The identity unit test had relied on an illegal root (White in
  check, Black to move) and was rebuilt on a legal one.

## Review

One completion review (`reviewer`, 2026-10-21), scope §14 and the project principles. It
found no implementation blockers in tactic judgement, contributions, `pin_tactical`,
serialisation, rendering or determinism. One blocker (row 10): the TERMINOLOGY `threat`
row put a `capture` threat on the captured piece's square, which is wrong for en passant
(`7k/8/8/3pP3/8/3n4/8/K2R4 w - d6 0 1` reports `capture` on d6, pawn on d5). The row
was fixed and the recheck found it resolved. One nit (T range in this receipt) was also
fixed.

Checkpoint 3 (2026-10-21): the Captain approved the merge; `postm2-em1` merged into
`main` (`d4c18a3`).
