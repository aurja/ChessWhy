# 05 — Verdicts

Baseline `23bf1a52d856060556147f73c1bd5f90c67c8d5d`, dated 2026-10-22, report schema v4. M4 and M5 are planned, not built.

## Concept nodes

- **Verdict**: grade from a move's ES loss against search A's best line. Fine ≤ 60 units; inaccuracy 61–259; mistake 260–539; blunder ≥ 540. Integer `es_units = 2W + D`; displayed ES is `es_units / 2000` ([rules.cpp](../../src/analysis/rules.cpp) `es_units`, `verdict_for_loss`; [analysis.hpp](../../include/chesswhy/analysis.hpp)).
- **Fine set and `fine_truncated`**: moves graded fine, and a flag for whether A returned five fine lines ([analyse.cpp](../../src/analysis/analyse.cpp) L463–466; [DEFINITIONS](../DBC/DEFINITIONS.md) §3.5).
- **Refutation margin**: O's best reply after a non-fine move, judged against search R ([DEFINITIONS](../DBC/DEFINITIONS.md) §3.3; [PostM2_EM1](../milestones/PostM2_EM1.md) §4.1).

## Conclusion

The metric and thresholds are coherent and pinned by tests. The defects are in what the output claims: `fine_truncated: false` reads as a complete list; printed order is not always score order; grades depend on the measuring search; saturation hides ties and mate distance, a risk for M4. None requires a threshold change.

## Approach

The investigator ran four CLI probes and one direct UCI replay, all successfully.
The child position after F11's h3 was analysed by the actual CLI as well as by the
separate UCI replay. Historical saved fixture reports are supporting evidence,
not newly executed gates; the stale F16 report is excluded from conclusions.

## Findings

### VER-1 — `fine_truncated: false` is read as completeness

- **Type**: contract-level concern. The flag behaves as specified ([M1](../milestones/M1.md) §4.7).
- **Root cause**: the flag means A returned five lines, all fine. It cannot establish that no move outside A will be graded fine by search C. Mixed-depth ordering also breaks an inference from any earlier non-fine entry to all later entries being non-fine: the actual F11-child CLI report has Qd7 non-fine, then Qc8 and g6 fine, with the flag false.
- **Evidence** [OBSERVED, probe artifact `critique-verdicts-start-quick-g3.json`]: `build/release/chesswhy --fen "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" --budget quick --move g3 --json`

| Move | ES units | Loss units | Verdict | In A |
|---|---:|---:|---|---|
| e4 | 1067 | 0 | fine | yes |
| d4 | 1028 | 39 | fine | yes |
| Nf3 | 1028 | 39 | fine | yes |
| e3 | 1013 | 54 | fine | yes |
| c4 | 1004 | 63 | inaccuracy | yes |
| g3 (asked) | 1016 | 51 | fine | no |

`fine_truncated: false`, yet g3 is fine and outside A. Its searched value is 3 units above e3 and 12 above c4. [INFERENCE] Search variation can explain the small difference; its frequency was not measured.

- **Impact**: readers take the fine list as complete. PROJECT_VISION §3 asks for every fine move, best first.
- **Strongest defense**: a completed iteration lists its top five, so unlisted moves rank at or below line 5 ([INFERENCE] on MultiPV semantics). Node-limited budgets keep the flag reproducible ([M1](../milestones/M1.md) §3).
- **Bounded proposal** [PROPOSED]: keep the literal rule; add an explicit scope statement or `fine_complete: false`. Captain decision.
- **Acceptance experiment** [PROPOSED, not run]: at quick and standard on the startpos, output labels g3 as outside the listed set. Passes only if no asked fine move appears as part of a complete list.

### VER-2 — Printed candidates are not guaranteed score order

- **Type**: contract-level / documentation concern; prospective risk for "best first" wording.
- **Root cause** [SOURCE]: `Line.depth` is parsed ([info_line.cpp](../../src/engine/info_line.cpp) `parse_info_line`) but not carried into the report or used by analysis. The collector keeps the newest exact line per slot at any depth ([engine.hpp](../../include/chesswhy/engine.hpp) `MultiPvCollector`). PostM2_EM1 §4.1 assumes A's order is by score.
- **Evidence** [OBSERVED in recorded UCI replay, not re-run]: child FEN `1r1q1rk1/4b1pp/4b3/1p1pPp2/3p1P2/7P/1PBB2P1/R2Q1RK1 b - - 0 18`, with `Threads 1`, `Hash 64`, `UCI_ShowWDL true`, `MultiPV 5`, `go nodes 1000000`. ES is for the side to move (Black).

| Slot | Move | Depth | cp | ES units |
|---:|---|---:|---:|---:|
| 1 | Ra8 | 16 | −98 | 519 |
| 2 | b4 | 16 | −102 | 479 |
| 3 | Qd7 | 16 | −117 | 303 |
| 4 | Qc8 | 15 | −102 | 479 |
| 5 | g6 | 15 | −102 | 475 |

Slot 3 (303) precedes slot 4 (479): an adjacent inversion of 176 units at a depth boundary.
The actual CLI report contains these same values; the independent UCI replay exposes
the mixed depths. The historical F06 report also has an inversion, but its cause was not
isolated by this review.

- **Impact**: score-order wording is false for these lines. The assumption also affects completeness reasoning and using `candidates[1]` as the best alternative.
- **Strongest defense**: these particular trailing inversions do not change the recorded best line. Node limits are deliberate, and retaining the freshest slot values avoids discarding newer information. No above-best candidate was observed; the loss clamp changes neither a negative nor a zero loss from fine.
- **Bounded proposal** [PROPOSED]: describe current order as engine slot order and expose per-line depth. If a coherent ranked comparison is required, decide explicitly whether to retain a completed MultiPV iteration; simply sorting stale and fresh values does not make them comparable.
- **Acceptance experiment** [PROPOSED, not run]: reproduce this CLI/UCI pair and ensure the output states its ordering and depth policy. Cross-link [06-searches.md](06-searches.md).

### VER-3 — Grades depend on the measuring search (accepted limitation, partly rebutted)

- **Type**: accepted limitation.
- **Root cause**: comparisons reuse estimates from searches with different roots, restrictions and budgets. The report records requested budgets per search kind, but not each estimate's achieved depth or complete search context.
- **Evidence** [OBSERVED, investigator-run]: `build/release/chesswhy --fen "1r1q1rk1/4b1pp/4b3/1p1pPp2/3p1P2/8/1PBB2PP/R2Q1RK1 w - f6 0 18" --budget standard --move h3 --json`, plus the child-root command in VER-2. Best Bd3 = 1764 units. Only the first row below is the original CLI's verdict for h3; the other grades are counterfactual arithmetic against that same baseline.

| Measured by | Black's Ra8 (units) | White line (units) | Loss (units) | Verdict |
|---|---:|---:|---:|---|
| Search A, h3 line | — | 1517 | 247 | inaccuracy |
| Child, depth 16 (final, about 1.0M nodes) | 519 | 1481 | 283 | mistake |
| Child, depth 15 (intermediate, 651,604 nodes) | 774 | 1226 | 538 | mistake |
| Search R | 785 | 1215 | 549 | blunder |

Black's Ra8 spans 519–785 units across searches of one position, a 266-unit spread above the 260-unit mistake cut. None is ground truth. R's depth is unrecorded, so the cause is [INFERENCE]. Saved `F11.json` (asked `exf6`, blunder) shows h3 at `candidates[3]` with margin −0.151, matching search R [OBSERVED, saved artifact].

- **Fixture gap** [SOURCE]: the F11 fixture pins `exf6` as a blunder ([positions.json](../../tests/fixtures/positions.json) L229); `h3` is unpinned, so the ≥30-unit margin rule ([M1](../milestones/M1.md) §9.1) does not cover it.
- **README `Qf6`** ([README](README.md)): quick loss 0.0285 (57 units, 3 under the fine cut) is fine; standard (80 units) and deep (85 units) are inaccuracy.
- **Strongest defense**: node-limited budgets ([M1](../milestones/M1.md) §3); wall-clock out of scope (§15); fixtures kept ≥30 units from thresholds; boundaries unit-tested.
- **Bounded proposal** [PROPOSED]: print the asked move's loss and ES in text (the asked block, `render.cpp` L341–345, prints only verdict and `in_multipv`); add a per-budget sensitivity table. Cross-link [09-reports.md](09-reports.md). Captain decision.
- **Acceptance experiment** [PROPOSED, not run]: F11 `h3` at quick, standard, and deep, each grade shown with its budget. Passes if every grade change is labelled.

### VER-4 — Saturation and relative-loss wording (prospective M4 risk)

- **Type**: prospective risk.
- **Root cause**: ES saturates near 0 and 1. A tactic needs a margin of at least 260 units (`kTacticMarginUnits = kMistakeUnits`; unit test L54–57). Saturation is documented only for tactics ([DEFINITIONS](../DBC/DEFINITIONS.md) §4.5). Grades and margins inherit it.
- **Evidence** [OBSERVED, artifacts `critique-proof-fork-deep.json`, `critique-proof-fork-quick.json`]: FEN `k7/8/8/8/3Q3n/8/2P5/4K3 w - - 0 1`, `--move c3`.

| Budget | Five lines ES | Mate distance | Root margin | Root tactic |
|---|---:|---|---:|---|
| deep | 1.0 (all) | Qxh4 mate 8; four lines mate 10 | 0.0 | no |
| quick | 1.0 (all) | none shown | 0.0 | no |

ES cannot separate Qxh4 (mate 8) from mate-10 lines. The root is not a tactic at a tie, as the smoke row expects. Both commands included the asked c3 and its refutation; this finding concerns the root alternatives, not that refutation.

- **Impact**: at M4, a grade alone hides ties; a non-tactic "refutation" may be only the opponent's best continuation.
- **Strongest defense**: the limit is documented and accepted ([DEFINITIONS](../DBC/DEFINITIONS.md) §4.5), and the smoke row expects this non-tactic root.
- **Bounded proposal** [PROPOSED]: state saturation in DEFINITIONS §3.5; pair grades with ES or mate distance at M4; use "opponent's best continuation" for non-tactic refutations. Captain decision.
- **Acceptance experiment** [PROPOSED, not run]: on this FEN, output marks the ES 1.0 tie and prints mate distance beside ES; passes if no line is shown as the sole best on ES alone. Cross-link [02-tactics.md](02-tactics.md) and [08-proofs.md](08-proofs.md).

## Strengths

- One ES scale serves verdict, parry, and tactic ([analyse.cpp](../../src/analysis/analyse.cpp) L154, L342).
- Thresholds are pinned inclusively by unit tests.
- The MultiPV collector bug is fixed and logged ([M1](../milestones/M1.md) §16).

## Verification limits

- Four investigator-run CLI probes and one UCI replay exited 0; shared parent probes supplied the README and saturation examples. No new builds, test suites or formatters were run for this report.
- The child-root evaluations, mixed depths and false completeness flag were observed. Regrading h3 against child/R estimates is arithmetic illustrating comparability, not a replacement product verdict.
- No exhaustive legal-move sweep or population-wide instability rate was measured.
- Historical fixture observations remain historical; no claim depends on an unreconciled count of saved build artifacts.

## Sources

Code: [analysis.hpp](../../include/chesswhy/analysis.hpp), [engine.hpp](../../include/chesswhy/engine.hpp), [rules.cpp](../../src/analysis/rules.cpp), [analyse.cpp](../../src/analysis/analyse.cpp), [info_line.cpp](../../src/engine/info_line.cpp), [render.cpp](../../src/render/render.cpp), [unit_tests.cpp](../../tests/analysis/unit_tests.cpp), [positions.json](../../tests/fixtures/positions.json). Docs: [M1](../milestones/M1.md), [PostM2_EM1](../milestones/PostM2_EM1.md), [DEFINITIONS](../DBC/DEFINITIONS.md), [PROJECT_VISION](../PROJECT_VISION.md), [README](README.md). Siblings: [02-tactics.md](02-tactics.md), [06-searches.md](06-searches.md), [08-proofs.md](08-proofs.md), [09-reports.md](09-reports.md).
