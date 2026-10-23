# Explanations critique: template text, M4 prose, claim checking

Baseline `23bf1a52d856060556147f73c1bd5f90c67c8d5d`, 2026-10-22, schema v4. M4 and M5 are planned, not built. Labels: **Observed** runtime output, **Source** code/contracts, **Inference** untested consequences, **Proposed** unrun changes. X1–X13 are constructed claim pairs, not observed model failures. No proposed claim-checking experiment has run.

## Baseline and concept nodes

Relevant nodes: `TemplateRendering`, `RestrictedMovement`, `ExplanationText`, `FactSheet`, `LevelPrompt`, `LanguageModel`, `FactCheck`/`FcSupported`, `BookPassages`, `ReportDocument` ([CONCEPTS.md](../DBC/CONCEPTS.md)). Current template behavior is assessed separately from the planned M4/M5 pipeline.

## Conclusion

The template exposes computed facts and provenance, but conditional lines and rule-scoped labels are easy to overread. The proposed entity-membership check ([§7.3](../PROJECT_VISION.md)) cannot establish polarity, side, timing, causality or numeric agreement. X1–X13 illustrate that limitation; they are not results from an implemented checker.

## Approach

Six observed CLI runs by the parent (R) and explanation investigator (A/K): `build/release/chesswhy --fen FEN --move MOVE --budget standard`, with `--json` or `--no-llm`; all exited 0, empty stderr. Positions (FEN; move):

- R: `r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 3 3`; `Nf6`. Level `club` comes from JSON `input.level`.
- A: `2r3k1/Q5pp/1Bq5/P2pb3/8/2n2R1P/2p2PP1/2R3K1 b - - 3 34`; `h6`.
- K: `8/7R/2r5/8/P3n3/R7/3nk1PP/6K1 w - - 3 39`; `Ra1`.

Records: `critique-probe-records.json` (R), `critique-expl-probe-records.json` (A, K). Quick-budget values: `critique-readme-quick.json`, not re-run.

## EXP-1 — Entity membership does not establish claim truth

- **Root cause.** Membership tests whether named moves, squares and pieces occur, not what is claimed about them [Inference].
- **Evidence.** Handoff-counted, not re-run:

| Run | Squares named (of 64) | SAN tokens |
|---|---|---|
| R | 56 | 67 |
| A | 50 | 61 |
| K | 42 | 63 |

- **Impact (Inference).** A checker that only tests entity membership lacks the information needed to reject the relational errors below. M4's eventual implementation has not been tested.
- **Strongest defense.** Invented moves are cheap to catch, and template fallback caps damage ([§7.3](../PROJECT_VISION.md)). The [M0 receipt](../receipts/M0.md) describes 0.8B greedy output as "coherent but repetitive and factually wrong," supporting a gate rather than trust in fluent text.
- **Proposal.** Keep membership as a necessary check. Realize scoped claims from report fields; verify polarity, side, condition, horizon, numeric values and supported relations. A sentence may need several fields. Mere field citation is not entailment; unsupported causal relations must not pass.
- **Acceptance (Proposed).** Reject the seeded false claims and accept supported paraphrases. Measure false rejection too; do not assume this finite set establishes general natural-language correctness.

## EXP-2 — Qualifiers the text drops

- **Root cause.** Text omits some premises or leaves them in terse rule names. A walked-ply endpoint can read as a settled outcome although it only ends the bounded walk.
- **Evidence.** A prints `Candidate 1: Ne2+ (c3e2), fine, loss=0.0%`, `parries threat: false (deferred_by_check)` and `creates threat: null`. Code has three causes for the latter: check, game end, or no legal move after the null move ([report.hpp](../../include/chesswhy/report.hpp)); the [definition](../DBC/DEFINITIONS.md) names check. R's asked Nf6 text omits "if White passes" from its Nxh5 line despite the Qxf7# refutation. The f4 probe prints `material_ahead: black 1 (ply 6)` without "not at end": the walk stops before quiet ply 7 although the engine PV continues and the pawn is regained at ply 9.

| Candidate (R) | Quick loss | Standard loss |
|---|---|---|
| g6 | 0.0 | 0.0 |
| Qe7 | 0.004 | 0.021 |
| Qf6 | 0.0285 | 0.04 |
| Nh6 | 0.05 | 0.0445 |
| d5 | 0.5005 | 0.5035 |

- **Impact.** `parries threat` is a threshold rule's result ([DEFINITIONS](../DBC/DEFINITIONS.md) line 109), not a verdict on the move, yet `parries threat: false` beside a fine move (A lines 52, 99) can read as contradiction. Qf6 crosses the 0.03 fine threshold ([DEFINITIONS](../DBC/DEFINITIONS.md) line 129) between budgets; the f4 text prints no budget.
- **Strongest defense.** JSON paths and contracts preserve more context than the text, though not every achieved-search detail ([09-reports](09-reports.md)). Qf6's flip is budget sensitivity, not nondeterminism. The Captain chose to keep positional changes in template output; M4 prose selection remains separate.
- **Proposal.** Print the budget, pass premise and unavailability reason. Mark the walk's endpoint as a horizon, not a final chess outcome.
- **Acceptance (Proposed).** Semantic assertions verify those qualifiers on the three positions without changing JSON. Do not replace brittle exact-wording tests with newly pinned strings.

## EXP-3 — Restricted piece roles vary; "tactic:" has two meanings

- **Root cause (Source).** `restricted movement (…)` prints `actor → targets`. Ordinary pins have the pinner as actor and the held piece at `targets[0]` ([M2 §4.1](../milestones/M2.md)); `pin_tactical` also holds `targets[0]`, but its actor is the beneficiary's threatening piece ([PostM2 §4.5](../milestones/PostM2_EM1.md)). For `overloaded_defender`, the held defender is the actor. A universal "actor is restricted" or "target is restricted" rule is wrong. `tactic:` also heads both a motif label and a margin judgement ([render.cpp](../../src/render/render.cpp)).
- **Evidence.** R's g6 conditional E line prints `restricted movement (relative pin): q g5 → P d2, B c1 (ply 3)`. This concerns the later Qg5 position, not the board immediately after g6.
- **Impact (Inference).** Actor-first arrows invite role inversion; X6 deliberately swaps the roles within that motif.
- **Strongest defense.** The Captain's 2026-10-21 report wording is an accepted decision, not an unresolved proposal ([DEFINITIONS](../DBC/DEFINITIONS.md)). The tags and contracts distinguish these roles.
- **Proposal.** Preserve that wording, explicitly naming the held piece according to tag and the margin as an only-move judgement. Separate mechanism labels from that judgement without pretending `is_tactic` itself proves a mechanism.
- **Acceptance (Proposed).** Name d2 as held in the R example; name the actor as held for an overloaded defender. Reject the role-inverted X6.

## EXP-4 — Choose facts in C++, not in the model

- **Root cause.** The model "never decides chess content" ([PROJECT_VISION](../PROJECT_VISION.md) §1), while §7.2 leaves fact-sheet selection insufficiently specified for enforcement.
- **Evidence (Source).** Current CLI stores level for echo; template rendering does not use it. This is not evidence about future model output.
- **Existing requirement.** Level changes wording rather than analysis ([DEFINITIONS](../DBC/DEFINITIONS.md)). Byte-identical fact sheets and a mandatory claim set are proposed enforcement choices, not a claim that M4 already implements them.
- **Impact (Inference).** Unconstrained selection can omit a refutation or null premise while every remaining sentence uses allowed entities.
- **Strongest defense.** Flexible selection can improve readability. The risk is loss of material qualifiers, not every difference in verbosity.
- **Proposal.** Select claims and required qualifiers deterministically before generation; let level alter wording. Keep a minimum content contract across levels. Decide separately whether optional facts may be omitted.
- **Acceptance (Proposed).** On the three positions, all levels retain mandatory claims and qualifiers. Byte-identical sheets are one enforceable design; identical prose is neither required nor desirable.

## EXP-5 — Mention metrics can miss contradictions

- **Root cause.** [§7.4](../PROJECT_VISION.md) measures unsupported-reference rate, key-fact coverage and speed. If coverage is implemented as mere mention detection, a negated or inverted claim may still count (Inference; the plan does not require that weak implementation).
- **Evidence.** X1 uses report entities while reversing the recorded verdict. Its semantic error is independent of whether its references are recognized.
- **Impact (Inference).** A mention-based benchmark can reward fluent contradictions.
- **Strongest defense.** Existing metrics are cheap, reproducible and useful as a first gate.
- **Proposal.** Add contradiction and false-rejection rates on labelled claims, plus fallback rate by level. An offline model judge may flag candidates for review; it must not become the authority for chess truth.
- **Acceptance (Proposed).** Compare entity-only and semantic checks on the same labelled sentences and paraphrases. Matching model rankings alone would not show that either check catches false claims.

## X1–X13 (PROPOSED)

These pairs use observed report facts but constructed prose. They test scope and relations, not hypothetical new chess discoveries.

| ID | Supported claim | Seeded false claim | Contradicting field |
|---|---|---|---|
| X1 (R) | Nf6 is a blunder with Qxf7# as refutation | Nf6 is fine | verdict `blunder`, loss .5275 |
| X2 (R) | If White passes after Nf6, Black's line starts Nxh5 | Nf6 guarantees winning the queen regardless of White's reply | pass-conditioned E line; actual refutation Qxf7# |
| X3 (R) | The parry test marks d5 true, despite its blunder verdict | The parry test marks d5 false | `parries_threat.value: true` |
| X4 (R) | Of the listed candidates, g6 and Qe7 are fine | g6 is the only listed fine candidate | both verdicts `fine`; no exhaustive-legal-move claim |
| X5 (R) | g6's line fails the only-move tactic threshold | g6's line passes that threshold | `is_tactic: false`, margin .021 |
| X6 (R) | In g6's E line at ply 3, Qg5 pins pawn d2 to Bc1 | In that motif, pawn d2 pins Qg5 to Bc1 | actor/target roles of that `pin_relative` instance |
| X7 (R) | d5's listed changes include doubled Black c-pawns | That listed change belongs to g6's line | change belongs to d5's candidate |
| X8 (R) | Nh6's line has a temporary queen gain, followed by Bxg5 | Black retains that nine-unit queen gain after Bxg5 | ply-7 gain marked not-at-end; recapture at ply 8 |
| X9 (R) | The urgency rule names White | The urgency rule names Black | `more_urgent: white` |
| X10 (A) | Ne2+ is fine; its false parry result is deferred by check | Ne2+ is a blunder because its parry result is false | verdict `fine`, loss 0; `deferred_by_check` |
| X11 (A) | Ne2+ has a fork motif targeting Rc1 and Kg1 | Ne2+ has no fork motif | recorded fork; null `creates_threat` does not remove it |
| X12 (K) | Ra1 passes the parry test but allows mate in 3 | Passing that test means White is safe after Ra1 | blunder; refutation mate_in 3 |
| X13 (K) | Ra1's if-pass line has material consequence 0 | That line has material consequence 5 | `creates_threat.consequence.material: 0` |

Parry, urgency, tactic judgement and contributions retain their operational meanings ([DEFINITIONS](../DBC/DEFINITIONS.md), [PostM2 §4.3/§15](../milestones/PostM2_EM1.md)). A false `is_tactic` does not prove no fork; a `positional` consequence does not prove no threat. The pairs deliberately avoid those invalid inferences.

## Strengths to keep

- Level has no path into analysis (EXP-4 evidence).
- M3b bars ranking by count: "No wording that ranks moves by count" ([M3b](../milestones/M3b.md) line 206).
- Saturation and two-winners limits stay documented ([DEFINITIONS](../DBC/DEFINITIONS.md) lines 209–210).

## Verification limits

- No model generation, claim checker, latency benchmark or cross-level model run. M4 risks are prospective, not observed implementation failures.
- CLI text/JSON and entity counts came from the named investigator/parent runs. The template's null causes and role mappings were source-checked.
- The pawn-recovery and queen-recapture examples concern recorded PVs and bounded material walks, not guarantees about all continuations.

## Sources

Repo-relative links above. `critique-*` artifacts and the handoff are checking evidence, not links.
