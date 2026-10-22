# Tactics critique: TAC-1 to TAC-5

**Baseline:** `23bf1a52d856060556147f73c1bd5f90c67c8d5d`, 2026-10-22, schema v4. M4 and M5 are planned, not built ([CURRENT_STATE](../CURRENT_STATE.md) line 21; [PROJECT_VISION](../PROJECT_VISION.md) §7).

**Concept nodes (existing terms only):** JudgedLine, BestAlternative, OnlyMoveMargin, TacticJudgement, TacticLine, Contribution, TacticLabel.

## Conclusion

The code mostly follows its written rules; the overreach is in what the labels claim. Contributions measure involvement, not cause (TAC-1). Refutation lists omit root-state motifs (TAC-2). `is_tactic` tests a margin, not a mechanism (TAC-3). The margin compares two searches by design (TAC-4). `removal_of_defender` matches squares, not pieces (TAC-5, unrun through C++).

## Approach

The task investigators inspected source and contracts; they and the parent exercised the CLI and legality probes below. Labels distinguish OBSERVED output, SOURCE rules and historical receipts, and INFERENCE from unexecuted constructions. Every proposal is unrun.

## TAC-1 (highest): glosses claim cause; the rule measures involvement

**Root cause.** [`judge_contributions`](../../src/tactics/tactic.cpp) marks a tactic-side move motif `direct` (rule 2, [PostM2_EM1 §4.3](../milestones/PostM2_EM1.md)); opposing-side motifs take `none` first. Rule 3 marks `indirect` when an X move reaches a square a Y piece of the motif attacks, while no motif piece has moved or been captured. The plain-meaning gloss still reads `none` as "the tactic would work the same without it".

**Evidence.** OBSERVED (parent CLI, `build/release/chesswhy --fen FEN --budget standard --json`): `6k1/5ppp/8/8/8/1q6/5PPP/4R1K1 w - - 0 1`, `Re8#`, margin 1.0: `back_rank_weakness` `none`. `6k1/5ppp/3rr3/q7/6Q1/8/1B3PPP/6K1 w - - 0 1`, `Qxg7#`, margin 1.0: `back_rank_weakness` `indirect`.

OBSERVED (investigator, python-chess legality): removing f7 permits `Kf7` after Re8+, so escape confinement matters to this mating witness despite `none`. In the second board, removing Bb2 permits `Kxg7`; removing either f7 or h7 still leaves mate. The `indirect` rule nevertheless credits the Y king's motif. The receipt's direct share of 1.000 for tactic-side move motifs follows from rule 2; it is not independent causal validation.

**Impact.** Labels read as mechanism; here they credit the wrong piece and miss a needed motif.

**Strongest defense.** Lines 129–130 already say an X piece's guard is its own work. Checkpoint 2 applied that, and the pin share fell from 0.783 to 0.515 ([receipts](../receipts/PostM2_EM1.md) lines 78–79; SOURCE).

**Proposal (unrun).** Reword glosses as operational involvement, not necessity (Captain's call); add both boards as candidate C rows. Restricting rule 3 to the relevant held piece could remove false associations, but would still not establish causation.

**Acceptance experiment (unrun).** Use controlled legal variations to test a claimed constraint against the specific mating witness. Do not remove kings or treat arbitrary piece deletion as a proof about all continuations.

## TAC-2: refutation lists omit root-state motifs

**Root cause.** [`assess_move`](../../src/analysis/analyse.cpp) builds the refutation from the position after the asked move. [`line_motifs`](../../src/tactics/motifs.cpp) seeds from that root and appends newly appearing motifs. SOURCE: occurrences already present at that root are omitted; a later disappearance and reappearance can create a new occurrence.

**Evidence.** OBSERVED (predecessor run, exit 0): `build/release/chesswhy --fen '4k3/4p3/8/8/1q6/6B1/8/4R1K1 b - - 0 1' --move Qd6 --budget standard --json --no-llm`. Qd6 scores ES 0. Its refutation `Bxd6 e5 Bc5 e4 Be3 Ke7 Re2 Kf6 Rh2 Ke7` has margin 0.5015 and labels `[]`.

| Motif | Recorded at | In refutation list? |
|---|---|---|
| `hanging_piece`, queen d6 | `asked_move.motifs`, ply 1, contribution null | no |
| `pin_absolute` e1→e7,e8 | `position.motifs`, contribution null | no |
| `pin_absolute` e1→e5,e8 and e1→e4,e8 | refutation plies 2 and 4 | yes, `none` |

**Impact.** This board's hand labels (SOURCE, [tactics.json](../../tests/fixtures/tactics.json) lines 200–201) give `direct` and `indirect` at ply 0; the refutation shows neither.

**Strongest defense.** [PostM2_EM1 §4.2](../milestones/PostM2_EM1.md) keeps contributions on a move's own motifs null because that line is unjudged. The motifs themselves remain visible, including the queen in `asked_move.motifs`.

**Proposal (unrun).** Either document the omission of refutation-root occurrences, or include and judge those occurrences (a scope change; Captain decides).

**Acceptance experiment (unrun).** If coverage is expanded, this refutation should expose the e1 pin and d6 queen and judge them under the same operational rules as root motifs. If coverage stays bounded, the consumer must not imply that an empty contributing list establishes no mechanism.

## TAC-3: `is_tactic` tests the margin, not mechanism

**Root cause.** `is_tactic` (TacticJudgement) holds when the OnlyMoveMargin is at least 0.13 ES ([DEFINITIONS §4.5](../DBC/DEFINITIONS.md) line 201). The render text `"tactic: next best … worse [engine]"` ([render.cpp](../../src/render/render.cpp) line 135) reads like a mechanism claim.

**Evidence.** OBSERVED (parent run; F01 at standard, F22 proof-fork at quick and deep; fixtures per [FIXTURES](../DBC/FIXTURES.md)):

| Case | Asked move | Margin (ES) | `is_tactic` | Motifs and labels |
|---|---|---|---|---|
| F01 `r4r1k/pppb2bp/2q1n1p1/4p3/8/1BPPBN2/PP2QPP1/R3K2R w KQ - 6 19` | `O-O-O` | .3845 (.9825 vs Kf1 .598) | true | every X-side motif `none`; labels `[]` |
| F22 `k7/8/8/8/3Q3n/8/2P5/4K3 w - - 0 1` | `c3` | 0.00 over Qe4+ | false | proof-fork: ES 1, margin 0 |

**Impact.** A tactic verdict can carry no mechanism (F01). `is_tactic: false` can mean two equally good moves (F22), not no tactic. Accepted limits stay: saturation (E2) and two winners (E4) ([DEFINITIONS](../DBC/DEFINITIONS.md) lines 209–210).

**Strongest defense.** The verdict follows the written definition ([PostM2_EM1 §4.1](../milestones/PostM2_EM1.md) line 88). [§4.6](../milestones/PostM2_EM1.md) (lines 183–186) puts tactic names and quiet-tactic detection beyond the margin out of scope.

**Proposal (unrun).** In the M4 consumer contract, describe `is_tactic` as an only-move-margin test. Any stronger mechanism explanation needs separately supported chess relations; a `direct`/`indirect` flag alone is insufficient (TAC-1). Keep the accepted saturation and two-winners limits.

**Acceptance experiment (unrun).** Count tactic lines with neither a direct/indirect motif nor a label across the F and C boards.

## TAC-4: the margin compares two searches, by design

**Root cause.** [PostM2_EM1 §4.1](../milestones/PostM2_EM1.md) (line 86) takes the refutation's line ES from `2000 − m.eval.es_units` and the BestAlternative from R(m), a separate search (100k nodes, MultiPV 1, all moves but r1; handoff-reported). The margin may be negative (line 88).

**Evidence.** d5 refutation (OBSERVED, parent run): README root `r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 3 3`, candidate d5. Fresh root: `r1bqkbnr/ppp2ppp/2n5/3pp2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 0 4`.

| Source | Budget | Best | BestAlternative | Alt ES | Margin |
|---|---|---|---|---|---|
| R search | quick | ≈ .980 (derived) | exd5 | .5035 | .4765 |
| R search | standard | .976 (inherited) | exd5 | .4145 | .5615 |
| R search | deep | ≈ .9785 (derived) | Bb5 | .3965 | .582 |
| Fresh root after d5 | standard | .983 (Bxd5) | Bb5 | .432 | .551 |

F11 (OBSERVED, verdicts investigator): standard `--move h3` on `1r1q1rk1/4b1pp/4b3/1p1pPp2/3p1P2/8/1PBB2PP/R2Q1RK1 w - f6 0 18`. Margin −.151; R's Ra8 is .3925. The actual child-root CLI report, FEN `1r1q1rk1/4b1pp/4b3/1p1pPp2/3p1P2/7P/1PBB2P1/R2Q1RK1 b - - 0 18`, gives Ra8 .2595. The 0.133 ES spread exceeds the 0.13 threshold. This is observed disagreement between searches, not proof that either grade is correct.

**Impact.** The BestAlternative and its value shift with search context. A classification flip near the threshold is a risk, not an observed result of these d5 or F11 comparisons.

**Strongest defense.** §4.1 writes the two-source design down, so the comparison is intended. Fixture F12 gives WDL 1000/0/0 under B200k, restricted 100k and restricted 200k (parent-shared artifact).

**Proposal (unrun).** Record search source and achieved depth. Compare with a fresh child-root MultiPV search, explicitly noting when the inherited refutation r1 is absent rather than assuming it is in the top two. Pair restricted r1 and excluded-r1 searches at comparable effort to isolate the inherited-value effect. Replacing r1 with the fresh best reply changes the contract.

**Acceptance experiment (unrun).** Report flip counts for F01, F11, F22 and the d5 board under both margins; cross-check [05-verdicts](05-verdicts.md) and [06-searches](06-searches.md).

## TAC-5: `removal_of_defender` checks squares, not pieces (unrun through C++)

**Root cause.** [PostM2_EM1 §4.4](../milestones/PostM2_EM1.md) (lines 134–137) states the rule by square: a later X capture "on t's square" with `see ≥ 1`. [`tactic_labels`](../../src/tactics/tactic.cpp) checks `future.to() == target_square` (lines 259–261) and never checks that t still stands there. [PROJECT_VISION §5.5](../PROJECT_VISION.md) (line 152) says "winning the piece", which the rule does not check.

**Evidence.** Constructed FEN `7k/8/3b4/q1P1n3/8/8/7N/4R2K w - - 0 1`, line `c5d6 e5g6 h2g4 a5e5 g4e5`; legality checked (handoff-reported). Source trace (INFERENCE; not run through C++): the label fires at ply 1, actor pawn d6, targets d (bishop d6) and t (knight e5). The knight escapes to g6, and a later capture on e5 takes the queen.

| Receipt (SOURCE, [receipts](../receipts/PostM2_EM1.md) lines 75–76) | Test | Dev |
|---|---|---|
| `removal_of_defender` recall, `capturingDefender` | .740 (222/300) | .700 (210/300) |
| Background Lichess agreement | .240 (56/233) | .194 (40/206) |

**Impact (INFERENCE).** The square-based rule permits credit for winning a different piece from the original target. This construction needs execution through the C++ labeller before being called an observed false positive.

**Strongest defense.** The code implements §4.4 as written, so the gap is in the rule text, not a coding slip.

**Proposal (unrun).** Require that t still stand on its square, unmoved and uncaptured, at the later capture. `judge_contributions` already tracks Y pieces. The captured defender d needs no subsequent continuity check: it has already left the board.

**Acceptance experiment (unrun).** On the constructed FEN, no label should fire for t = knight e5 at ply 1. Report test and dev counts before and after.

## Strengths

- Rules are numbered and testable ([PostM2_EM1 §4.3](../milestones/PostM2_EM1.md)).
- Annotations never change verdicts ([M2](../milestones/M2.md) line 23, D7).

## Verification limits

- Runs were concurrent; no timing claims. Legality checks used python-chess, not the engine.
- TAC-5's firing is a source trace; its FEN was not run through the labeller.
- F11 comes from the verdicts investigator's saved CLI report and separate UCI replay; d5 and F12 come from the parent's saved probes. No corpus-wide error rate or causal validator was measured.
- Receipt test/dev agreement is historical evidence, not a newly executed benchmark.

## Sources

Code: [tactic.cpp](../../src/tactics/tactic.cpp), [motifs.cpp](../../src/tactics/motifs.cpp), [analyse.cpp](../../src/analysis/analyse.cpp), [render.cpp](../../src/render/render.cpp), [tactics.json](../../tests/fixtures/tactics.json).
Planning: [PostM2_EM1](../milestones/PostM2_EM1.md), [M2](../milestones/M2.md), [DEFINITIONS](../DBC/DEFINITIONS.md), [FIXTURES](../DBC/FIXTURES.md), [receipts](../receipts/PostM2_EM1.md), [PROJECT_VISION](../PROJECT_VISION.md), [CURRENT_STATE](../CURRENT_STATE.md).
Siblings: [05-verdicts](05-verdicts.md), [06-searches](06-searches.md).
