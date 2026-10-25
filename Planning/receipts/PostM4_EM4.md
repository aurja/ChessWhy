# PostM4_EM4 receipt — the conceptual critique and the move report text

Date: 2026-10-25. Machine: `debian-home` (i7-8565U, 15 GiB, Debian 13). Branch
`postm4-em4`. Contract: `Planning/milestones/PostM4_EM4.md` (acceptance §4, contract
changes §5). Terms: `Planning/DBC/DEFINITIONS.md` §3.1, §4.5.

## Delivered

- **Motif repeats merged** (§2.1, R1): in every motif list of a move report,
  restricted-movement motifs that differ only in tag and threat become one entry whose
  `also` lists the other labels in order. The full report keeps every motif.
- **Report v8** (§2.2, §2.6): `schema/report.v8.schema.json` (version only),
  `schema/moves.v8.schema.json` with `also` on motifs and `line_fens` on move reports (the
  FEN after each walked ply).
- **Compact restricted-movement rows** (§2.3): the move report text names every label of a
  merged motif and drops the restricted and free move lists; the full report text keeps
  them.
- **Position walk** (§2.5, R3): the move report text goes position by position. Each
  block has the FEN, the evaluation (the root evaluation, then the line score), the move
  played from it and its side, the `Uses:` rows, then the other facts in impact order. No
  tempo rows in the move report text for now; the full report text and all JSON keep them.
- **Checker** (§2.4, §2.7): the projection check re-implements the merge and replays
  `line_fens` with python-chess.
- **Conceptual critique** of all definitions and concepts (2026-10-24):
  `Planning/critiques/2026-10-24/` 00–08 and the synthesis 09, 20 findings (6 `wrong
  claim`, 10 `misleading`, 4 `naming`). Findings only; the fixes are PostM4_EM5.
- **Docs**: README (v8, compact row, position walk), DBC files, `CURRENT_STATE.md`, this
  receipt, the PostM4_EM5 plan.

## Verification

| # | Check | Result |
|---|---|---|
| 1 | Acceptance 1, 5, 6 (CTests) | Release `unit\|engine\|games\|corpus` 213/213 in 229 s, then `timing` alone pass in 269 s (slowest F fixture F12 12.4 s, limit 15 s). ASan/UBSan (Debug `-O1`) `unit\|sanitize` 176/176 in 80 s, no `runtime error` or `AddressSanitizer` line. After the clang-tidy fixes (below): Release `unit\|engine\|games\|corpus` 213/213 in 310 s, ASan/UBSan 176/176 in 86 s. After the review fix: `report_schema` and `checker_unit` pass |
| 2 | Acceptance 3 | Full report JSON, without `schema_version` (8 vs 7), and full report text byte-identical to a `92dd918` build on 42 F fixtures and V2 (`--moves Bxa4,Ba7`); F15 not compared (its PGN path is relative to the fixture directory) |
| 3 | Acceptance 2, 6 (V2 smoke) | `--fen '1br3k1/5pp1/p3p2p/1bBn4/N7/1P3PP1/5R1P/2R3K1 b - - 0 1' --moves Bxa4,Ba7`, 5.2 s: per move position blocks 0…7, each `Position:` with a FEN then `Evaluation:`, `Move:` in blocks 0…6; no `Start:`, `Ply k:`, `Tempo value:`, `Tempo:`, `Initiative:`, `restricted:` or `free:` row; no two restricted-movement rows of one move report differ only in their label; `--full-report` text keeps 148 restricted/free lists and its tempo rows |
| 4 | Acceptance 4 | Superseded by R3 (part 1c); verified at part 1b (`4a5beed`, Release `unit\|engine` 208/208) |
| 5 | Gates | clang-format clean on the changed C++ units; `clang-tidy --quiet -p build/release` 0 diagnostics after fixes; Ruff (check and format) and basedpyright 0/0 on `tools/validate_report.py` and `tests/checker/unit_tests.py`; no shell script changed; `check_dbc` ok |

The first clang-tidy run found 3 diagnostics: a redundant `also{}` initializer, a no-op
`std::move` of an optional, and an unchecked optional access in a test. Removing `{}` broke
the build (`-Wmissing-field-initializers` on the motif oracle's designated initializer),
so the oracle now names `.also = {}` like every other member. Ruff format had drifted on
both Python files. Fixed in `d414b6a`; the second gate run above followed it.

## Review

One completion review (reviewer on Sol 6.1). Blocker against §2.2 / acceptance 1:
`moves.v8.schema.json` accepted an `also` label `pin_tactical` with `threat: null` (and a
non-tactical label with a threat), because `threat` allowed object or null regardless of
`tag`. Fixed at the root: the `also` items now carry `if tag = pin_tactical then
motif_threat else null`; checker unit test `test_moves_schema_ties_also_threat_to_tag`
rejects both mutants (the old schema accepted all four cases). Recheck by the same
reviewer: closed; the original probe now exits 1 with `/moves/0/motifs/0/also/0/threat:
None is not of type 'object'`. No other findings.

## Contract changes

See §5 of the contract: part 1c replaced the part 1b ply walk with the position walk (R3);
R4 moved the planned part 2 (pure reply "forced and gains nothing" and the critique's
fixes) to PostM4_EM5.

## Checkpoints

1. Part 1 contract (R1) approved 2026-10-24; part 1b (R2) and part 1c (R3) layouts chosen
   by the Captain 2026-10-24/25.
2. The Captain closed the milestone at its current scope (R4, 2026-10-25) and chose
   PostM4_EM5's shape (three parts), VER2-1 option A and REP2-1 option 1.
3. Merge into `main`: awaits the Captain.
