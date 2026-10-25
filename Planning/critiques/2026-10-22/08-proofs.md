# 08 — Proofs: what the checks establish

Date: 2026-10-22. Baseline: `main` at `23bf1a52d856060556147f73c1bd5f90c67c8d5d` (M1, M2,
PostM2_EM1, M3a and M3b complete; report v4). Scope: the `Proof` diagram of
[CONCEPTS.md](../../DBC/CONCEPTS.md) (`ReferenceImplementation`, `PositionalReference`,
`GamesOracle`, `EngineFixtures`, `ContributionFixtures`), `ReportSchema`, rerun identity,
and the proof wording in the contracts and [DEFINITIONS.md](../../DBC/DEFINITIONS.md).

## Conclusion

ChessWhy has no formal proof subsystem and does not need one. Its checks are strong:

- Two references, written from the plan text, are compared field by field on tens of
  thousands of boards.
- A cross-library counter and a clean rebuild reproduce the pinned game table.
- The F harness recomputes every tactic judgement with its own constant.
- Sanitizer runs have caught real errors.

All of these should stay. The weakness is wording: "Proof", "exact", "everywhere" and
"proves" each claim one notch more than their warrant. Each claim should name what it
rests on: conformance on named inputs, a finite search at a stated budget, adjudicated
examples, or shape. Four cheap additions would make those warrants measurable without
theorem proving: seeded faults, kept edge inputs, a budget-stability table and held-out
contribution labels.

## What the checks establish

Counts are historical receipts at their commits, not passes observed in this review.

| Claim | Warrant today | Not established | Falsification case |
|---|---|---|---|
| Board predicates (legality, check, mate, square tests) | Exact relative to chess-library and the written rule | The library, which detectors, both references and the F harness all share | A legality or status disagreement with python-chess |
| Motif recognition is "exact" (M2 D12) | Detector equals reference on every compared list. That is 62,678 `test.csv` lists plus the T fixtures, with `pin_tactical` on 582 sampled puzzles. The first M2 run found 74 mismatching lists and two detector bugs, so the comparison discriminates | Chess meaning (recall covers part of it), inputs outside the corpus, errors both take from the text | A seeded detector fault left unreported |
| Positional features | The same design on 53,378 boards, plus `positional_reference_self_check` | That this comparison ever disagreed; no first-run count is recorded | A seeded threshold change (mobility lead 10 → 9) with zero mismatches |
| Game counts | python-chess byte equality over 81 months; a clean rebuild gives the pinned SHA-256 | Both counters' shared reading of M3b §4 | Edge-case games, as `games_edge.pgn` showed |
| Tactic recognition is "exact by construction" | `is_tactic` is a function of the report's engine values; the F harness recomputes margin, threshold and `forced` on every F report | That `candidates[1]` or search R is the best alternative; stability across budgets | [OBSERVED, parent-run] d5's refutation alternative is exd5 at quick and standard, Bb5 at deep |
| F verdicts, urgency and threats | Captain-reviewed expectations on positions chosen to be stable at all budgets | Stability on unselected positions | [OBSERVED, parent-run] Qf6 is fine at quick, an inaccuracy at standard and deep |
| Contribution "none = the tactic would work the same without it" | The rule reproduces 16 adjudicated C fixtures | Any counterfactual or causal reading | [OBSERVED, parent-run] a back-rank weakness is `none` under a back-rank mate |
| The schema "validates" the report | Closed objects, enums and conditional rules (`forced` ⇒ no margin; not a tactic ⇒ no labels), checked on F reports and eight examples | The truth of any value; no report is validated at runtime | By design; see [09 — Reports](09-reports.md) |
| Rerun identity | Byte identity at a fixed budget and engine | Robustness to a budget or engine change | The two budget rows above |

**Historical gates and claimed scope.** The receipts say plainly how their results were
assembled. In the [M3a receipt](../../receipts/M3a.md), the final ASan/UBSan run is `unit` and
`games` 107/107 (408 s) plus `engine` 33/33. `tactics_oracle` is credited to the branch's
first ASan run, its code unchanged since. [CURRENT_STATE.md](../../CURRENT_STATE.md) shortens
this to "ASan/UBSan CTests 141/141".

Several errors surfaced outside the equality gates:

- M2's terminal-board text gap: both implementations agreed, and the recall audit found it.
- PostM2_EM1's inconsistent B−p boards: found when ASan/Debug runs aborted.
- M3b's position-key and variation-parsing bugs: found in review.
- M3a's smoke text, which put the bishop-pair change at ply 1 when the code was right.

So errors appeared in code, in oracles and in contract text, and the process caught each
one. Zero mismatches is necessary but not sufficient.

## Findings

### PRF-1 — Proof words outrun their warrants (contract-level concern)

**Root cause.** One vocabulary covers five kinds of warrant.

**Evidence.** [SOURCE]

- CONCEPTS L1 defines `Proof` as fixtures and measurements "that show the facts are
  right".
- `ReferenceImplementation` says "both must agree everywhere", yet DEFINITIONS §5.7
  samples `pin_tactical` on one puzzle in 16.
- DEFINITIONS §5.7: "If both agree everywhere, the detectors do what the text says".
- M2 §15, checkpoint 2b row: the reference "proves exact recognition".
- DEFINITIONS §4.5 calls tactic recognition "exact by construction".
- The L1 `Tactics` node speaks of motifs that "make it work".

Provenance tags compress the same way. A consequence's `source` is the constant `rule` in
[the schema](../../../schema/report.v4.schema.json), but `class=mate` and `mate_in` come from
the engine's mate score (M1 §4.3 step 4; `consequence` in
[rules.cpp](../../../src/analysis/rules.cpp)).

[OBSERVED, parent-run] The README command `--fen 'r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 3 3' --move Nf6 --budget standard --no-llm`
prints `White threat consequence: class=mate, mate_in=1, … [rule]`.

`parries_threat` shows the better pattern: its `test` field says how it was established.

**Impact.** [INFERENCE] The Captain decides at the DBC level, and M4 will put these fields
into words. Wording that fixtures "show the facts are right" invites reading a run with
zero mismatches as chess truth. The planned fact check tests references, not modality
([PROJECT_VISION.md](../../PROJECT_VISION.md) §7.3).

**Strongest defense.** The frozen acceptance texts are scoped. M2 D12 says "every position of
the test sample". The vision's milestone table says labels "follow mechanically from the
engine lines". Vision §5.6 concedes that "a wrong label would still carry `rule`
provenance", and every template row prints its source.

**Recommendation.** [PROPOSED] Keep a claim and warrant table like the one above next to
[TESTS.md](../../DBC/TESTS.md), and reword the overclaims:

- Describe `Proof` as checks "against definitions, chosen examples and outside labels".
- Replace "everywhere" with "on every compared position".
- Qualify "exact by construction" as "relative to the engine values at the report's
  budget".
- Describe `rule` on consequences and contributions as "a rule applied to an engine line".

No code changes are needed.

**Acceptance.** No "exact", "proves" or "everywhere" appears without its comparison set. M4's
seeded-statement benchmark includes overclaims such as calling an engine line "forced";
see [10 — Explanations](10-explanations.md).

### PRF-2 — Independent oracles share library, text and corpus (contract-level concern; prospective risk)

**Evidence.** [SOURCE] Both references call chess-library:

- [The motif reference](../../../tests/tactics/oracle/reference.cpp) uses
  `movegen::legalmoves`, `isAttacked`, `makeMove` and `makeNullMove`.
- [The positional reference](../../../tests/positional/oracle/reference.cpp) uses
  `castlingRights` and `makeMove`.

So do the detectors and `legal_move_count` in
[the F harness](../../../tests/cli/engine_fixture_test.cpp). The library is vendored
header-only at a pinned commit ([DEPENDENCIES.md](../../../third_party/DEPENDENCIES.md)). The
project's CMake does not build its upstream perft tests.

Only the games oracle crosses libraries, and a library-semantics error appeared exactly
there. `position_key` relied on chess-library dropping a pseudo-legal en passant square,
which it does not do after `makeMove` ([M3b receipt](../../receipts/M3b.md), Review). Review
found it with the game now kept as `games_edge.pgn`. The 81-month equality could not
reveal it, because the pinned table was unaffected.

The plan text is a second shared source. PostM2_EM1 §4.5's B−p could be an inconsistent
board, and ASan/Debug assertions caught it (commit `b4fbb30`). [INFERENCE] Release builds
compile those assertions out.

The shared corpus is thin at the edges:

- [OBSERVED] [test.csv](../../../tests/fixtures/puzzles/test.csv) has 16 `enPassant` and
  2 `underPromotion` puzzles, and no `castling` or `stalemate` theme.
- The M2 reviewer's nine edge inputs gave 0 mismatches but are not oracle inputs. No
  [motifs.json](../../../tests/fixtures/motifs.json) line under-promotes, and only T54 starts
  with an en passant square.
- [positional_oracle](../../../tests/positional/oracle/positional_oracle.cpp) reads only the
  puzzle CSV, so the P fixtures' boundary boards are never compared detector against
  reference (`reference_self_check` runs the reference alone on a few of them).

**Strongest defense.** chess-library is pinned and perft-tested upstream. [OBSERVED] It
rejects the classic pinned en passant capture (receipt below). M2's 74 → 0 history shows
the motif comparison discriminates, and ASan/Debug oracle runs are a required gate.

**Recommendation.** [PROPOSED, unrun]

- Seed one fault per oracle per milestone in a scratch build and record the mismatch count.
  Candidates: the mobility threshold, the SEE tie-break, a games exclusion reason. This
  mirrors M4's "seeded false statements".
- Keep the reviewer's edge inputs and the P-fixture boards as oracle inputs.
- Add a dev-only python-chess check of legal moves and check, checkmate and stalemate status
  on the compared boards.
- Run the vendored perft suite on library bumps.

**Acceptance.** Every seeded fault produces mismatches, and the libraries agree on the compared inputs.

### PRF-3 — Engine fixtures exclude the instability they would need to measure (accepted limitation; scope proposal)

**Evidence.** [SOURCE] M1 §9.1 requires every expectation to hold at quick, standard and deep.
M1 §16 records 541 positions screened and 24 shortlisted; "12 shortlisted positions dropped
because urgency or verdicts changed between budgets". PostM2_EM1 §9 repeats the rule, so
the repository documents this choice.

[OBSERVED, parent-run] On the README FEN with `--move Nf6`:

- Qf6 is fine at quick (loss 0.0285) but an inaccuracy at standard (0.04) and deep
  (0.0425), so its refutation exists only at the two larger budgets.
- d5's refutation stays a tactic, but its alternative changes from exd5 to Bb5 at deep.
- g6 stays best and Nf6 stays a blunder.

On the smoke FEN `k7/8/8/8/3Q3n/8/2P5/4K3 w - - 0 1 --move c3` at quick and deep, the
frozen fields hold. The best move changes (Qh8+ against Qxh4, all at ES 1.0), and so does
the urgency rule (counterattack against consequence_class).

**Strongest defense.** Thresholds on any finite search behave like this, and deep is not ground
truth. A parent-run direct UCI search of Qe7, Qf6 and Nh6, one at a time, moved no expected
score by more than 0.011 and changed no grade. Loss and margin already expose the distance
to each threshold. Historical identity checks cover same-budget reruns; the observed
budget changes are not evidence of nondeterminism. Negative margins and mixed contexts
are contractual; see [05 — Verdicts](05-verdicts.md) and [06 — Searches](06-searches.md).

**Impact.** A passing F suite cannot show how often a reported category depends on the budget.

**Recommendation.** [PROPOSED, unrun] Each receipt that touches the engine prints a
non-gating stability table:

- 200 fixed `test.csv` roots, each run at the three budgets.
- The flip rate per category (verdict, `is_tactic`, urgency side and rule, parry,
  alternative move), by distance to the threshold.
- The F harness invariant check applied to every sampled report.

**Acceptance.** The rates are printed. Flips confined to narrow bands around the thresholds
bound the concern.

### PRF-4 — Contribution fixtures are adjudicated regressions, not tests of the plain meaning (contract-level concern)

**Evidence.** [SOURCE] Checkpoint 2 touched 8 of the 16 C fixtures:

- Labels changed on C01, C08, C09 and C16.
- Rule changes made C10 and C12–C14 match.

The record is in [PostM2_EM1 §15](../../milestones/PostM2_EM1.md) and the Comments column of
[FIXTURES.md](../../DBC/FIXTURES.md). The hand-label definition permits this adjudication.
Unlike M2, the C fixtures have no held-out split. The puzzle check prints only a direct
share (`stratum_direct`, [puzzle_eval.cpp](../../../tests/tactics/puzzle_eval.cpp)).

[OBSERVED, parent-run] `chesswhy --fen '6k1/5ppp/8/8/8/1q6/5PPP/4R1K1 w - - 0 1' --budget standard --json`
judges Re8# a tactic with margin 1.0 and `back_rank_mate` `direct`. The root
`back_rank_weakness` is `none`, although the mate needs it.

In the README run, a `pin_tactical` (b7 held because it guards c6) is `direct`, even
though b7 recaptures on c6. Both results follow the rule; [02 — Tactics](02-tactics.md)
covers them.

**Strongest defense.** The rule is deliberately syntactic and Captain-approved, and
contributions never change verdicts.

**Recommendation.** [PROPOSED, unrun]

- State the plain meanings in rule terms.
- Label a blind held-out set of 15–20 `test.csv` lines before running the rule, never use it
  to change the rule, and report agreement.
- Add indirect and all-none shares to the puzzle check, and audit ten all-none `pin`
  puzzles as in M2 §8.3.

**Acceptance.** Agreement and shares appear in the receipt.

## Strengths to retain

- References written from the plan text without shared detector code, compared field for
  field. Every M2 mismatch was traced to its root and recorded.
- The cross-library games oracle, the rebuild to the pinned SHA, and `games_oracle_edge`.
- The invariant check with its own constant, fixtures robust at every budget, and Captain
  review.
- Blind P labels with no rule-versus-label mismatch, including ten book positions.
- Recall against Lichess themes as a validity check, kept separate from conformance.
- The closed schema, rerun identity, ASan/Debug oracle runs and candid receipts.

## Limitations and verification receipt

- [OBSERVED] `build/release/chesswhy --fen 'r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4' --json --no-llm`
  exits 0. It reports `position.terminal: "checkmate"`, `engine.nodes_searched: 0`, no
  candidates, no threats and no state motifs. A rule can check this mate-in-one witness;
  it cannot check a mate in N against every defense.
- [OBSERVED] `build/release/chesswhy --fen '8/8/8/KPp4r/8/8/8/7k w - c6 0 1' --move bxc6 --json --no-llm`
  prints `chesswhy: error: illegal move bxc6` and exits 2.
- Parent-run probes all exited 0 with no stderr on Stockfish 19; the SHA-256 is in the
  [README](README.md). I inspected their JSON and did not rerun them.
- Counts come from the committed CSV and JSON. I reran no CTest, oracle, fixture suite or
  schema validation, and I ran no proposed experiment.
- I found no fixture problem that needs a Comments entry.

Sources: [M1](../../milestones/M1.md) §4.3, §9.1, §16; [M2](../../milestones/M2.md) §1, §7.4, §15;
[PostM2_EM1](../../milestones/PostM2_EM1.md) §4.3, §9, §15; [M3a](../../milestones/M3a.md) §8;
[M3b](../../milestones/M3b.md) §9; receipts [M2](../../receipts/M2.md),
[PostM2_EM1](../../receipts/PostM2_EM1.md); [motif_oracle.cpp](../../../tests/tactics/motif_oracle.cpp);
[reference_self_check.cpp](../../../tests/positional/oracle/reference_self_check.cpp);
[validate_report.py](../../../tools/validate_report.py); [count_games.py](../../../tools/count_games.py).
