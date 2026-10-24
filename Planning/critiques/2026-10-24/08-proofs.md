# 08 — Measurement and proof

Date: 2026-10-24. Baseline: `fbc5947a81984066d68ddd20fb72522d4036227e`.

Concept nodes: `Proof`, `HandLabel`, `EngineFixtures`, `InputFixtures`, `MotifFixtures`, `ContributionFixtures`, `ReferenceImplementation`, `PuzzleSample`, `Recall`, `PositionalFixtures`, `PositionalReference`, `GamesOracle`. Scope: DEFINITIONS §5 and §6.2–6.3. Runtime and CSV observations below were made before the pause at `8b561c4fcfe58a8226910234b40413b9880f62e4`; resumed source inspection confirms the cited measurement and sampling rules, not a fresh runtime pass.

## Conclusion

The new substantive finding is that fewer motif instances need not mean less noise, even at unchanged recall.
The statistic rewards losing additional correct facts that puzzle-level recall cannot distinguish.
The second finding is narrower: old oracle descriptions no longer identify the comparison sets used by today's gates.
Stride 31 and the selected sanitizer fixtures are reasonable bounded checks, not demonstrated defects.
Release corpus agreement and sampled sanitizer safety must remain separate warrants.
Recall, agreement, direct share and untagged firing rate are sound operational counts, not independent identification of a puzzle's idea.
Prior holdout, shared-oracle and adjudicated-label limitations remain; they are not new findings here.
No builds, CTests or sanitizer runs were performed for this critique.

## Findings

### PRF2-1 Output volume is not noise, even at equal recall

**Definition — Source.** `Planning/DBC/DEFINITIONS.md:361`: “Average number of motif instances (all tags, both sides, root plus line) per background puzzle; a noise measure, lower is better at equal recall”. The historical keep rule at line 363 also requires that a change not raise this mean.

**What it claims vs what it computes.** It measures output cardinality, without deciding whether an instance is false, redundant or irrelevant. Recall is a Boolean union over mapped instances: two correct instances receive no more credit than one. Removing a correct second instance can therefore improve the alleged noise measure without changing recall. Moreover, the implemented count excludes `pin_tactical`, rather than covering “all tags” (`tests/tactics/puzzle_eval.cpp:431–443`).

**Counterexample — Observed; hypothetical metric comparison.** Python-chess confirmed that this position is valid, White is not in check, and `Rxb4` is legal:

```text
7k/8/7n/6B1/Rq6/8/8/2K5 w - - 0 1
```

The existing binary, run with `--fen` above and `--moves a4b4 --full-report --json`, returned two White root `hanging_piece` instances: black queen b4, gain 9; black knight h6, gain 3. Both are actual undefended pieces attacked by White. **Inference:** for a puzzle labelled `hangingPiece` with this root, retaining only the queen still predicts the theme. Dropping the correct knight instance lowers the count with identical puzzle-level recall. No detector was altered, and this position is not claimed to be a Lichess row.

**Root cause.** Proxy ≠ claim and composition (families 1, 5): existential theme coverage cannot protect the completeness of an instance list. Counting both sides also charges correct opponent facts against a solver-only recall benefit.

**Inherited by.** Calibration's historical keep rule, interpretations of motif-density receipts, and any future attempt to tune explanation quality by this mean. Calibration is closed; this is not a current failing gate.

**Candidate fixes.**

1. Rename it “enabled-detector output volume”; state that lower is preferable only after correctness/completeness constraints, not merely equal recall. Same computation, zero extra search; it ceases to claim a noise estimate.
2. Print counts split by side, tag and root/line, with exact duplicate identities counted separately. Cheap report bookkeeping, no searches; diagnoses redundancy and volume without pretending that a true additional fact is noise.

**Severity:** `misleading` — the measured number is real, but its optimisation direction is not justified.

### PRF2-2 Oracle scope wording hides the two-stage sample

**Definition — Source.** `Planning/DBC/DEFINITIONS.md:375`: “Every position of every `test.csv` puzzle … and every T fixture”; tactical pins use rows “divisible by 16”. By contrast, lines 439–441 define today's unit oracle as stride 31, with tactical pins on every sixteenth **compared** puzzle. `CONCEPTS.md:718` still says positional references agree on “every puzzle board and every P fixture board”.

**What it claims vs what it computes.** The broad comparison descriptions lack the build/gate distinction now required to interpret them. Unit/ASan sees 300 CSV puzzles; tactical pins see only 19 of those. Release corpus comparison retains 9,300 puzzles and 582 tactical-pin puzzles. Fixtures remain included. This is a new scope-specific instance of still-open prior PRF-1, not a demand to restore exhaustive ASan runs.

**Counterexample — Observed CSV audit; Source selection.** Counting the committed CSV with the specified row selectors gave:

| CSV selection | Puzzles | `enPassant` themes | `underPromotion` themes |
|---|---:|---:|---:|
| Whole file | 9,300 | 16 | 2 |
| Row index divisible by 31 | 300 | 1 | 0 |
| Row index divisible by 496 | 19 | 0 | 0 |

`tests/tactics/motif_oracle.cpp:545–547` first selects stride rows, then takes every sixteenth compared puzzle: here the effective original-row stride is 496. These are theme counts, not measured path coverage; sanitizer F17 explicitly covers underpromotion. No passing oracle or sanitizer execution is claimed by this audit.

**Root cause.** Composition and scope mismatch (families 1, 5): two accurate local sampling rules compose into a much smaller per-tag set; older “every puzzle” wording no longer names which gate supplies that evidence. Release equality cannot substitute for executing the same inputs with sanitizer instrumentation.

**Inherited by.** `ReferenceImplementation`, `PositionalReference`, `Proof`, gate summaries and receipts that collapse sampled safety and corpus conformance into one pass count.

**Candidate fixes.**

1. Define comparison sets explicitly by gate, build and tag; print both selected-puzzle and tactical-pin-puzzle counts. Existing selectors suffice; negligible cost, no chess-strength claim.
2. If broader safety coverage is desired, augment the cheap sample with explicitly retained edge cases selected by board events. Small replay/runtime cost; targeted coverage still is not exhaustive safety.

**Severity:** `naming` — §6.3 states the new scopes honestly; the defect is the inconsistent shorthand elsewhere, not the chosen reduction.

## Sound definitions

The following are sound within their literal operational bounds; related terms share a line where their warrant is identical.

- **Lichess puzzle database:** a pinned external dataset, not a chess oracle.
- **Theme:** explicitly the puzzle's idea, not an exhaustive motif inventory.
- **Eligible:** reproducible filters; not representativeness of all chess positions.
- **Root / solver / solution line:** opponent move removed, solver perspective and expected suffix are explicit.
- **Background / stratum:** reproducible sampling groups; “fair picture” must remain restricted to eligible puzzles.
- **Mapping / predicted:** explicit tag union and solver-side quantification.
- **TP / FP / FN / miss / disagreement:** agreement counts relative to Lichess labels, not truth labels for individual motifs.
- **Recall:** correctly computes tagged-puzzle coverage. Its stronger “chess meaning” sentence remains prior REF-1; untagged firing does not resolve instance identity.
- **Lichess agreement:** appropriately renamed and not gated as precision.
- **Untagged firing rate:** correctly exposes predictions outside named themes without asserting those predictions are wrong.
- **Puzzle check / direct share / removal-of-defender recall:** transparent conditional counts. Direct share conditions on detection and uses the rule's own `direct` labels; it is not independent causal validation.
- **Calibration / keep rule:** explicit historical procedure, closed in 2026; the volume preference is PRF2-1.
- **Target:** honestly a regression floor, not an adequacy judgement.
- **Sampling noise:** approximately correct one-standard-error arithmetic under binomial assumptions; not a confidence guarantee or correction for selection bias.
- **Audit:** explicitly a bounded hand review, not a population error estimate.
- **Split:** disjoint IDs are meaningful; continued “unfitted holdout” status is the prior REF-2 limitation, not newly established here.
- **Former engine-confirmed precision / sample N / board B / tracked pieces / hit / confirmed / no-hit / hit-no-gain:** clearly archived engine-line measurements, not present gates or proof of pattern meaning.
- **Reference implementation / mismatch:** valuable independent implementations and field comparisons; shared-text/library limits remain prior PRF-2.
- **Fixture / F / U / E / parse / T / C / P / use-test fixtures:** explicit expected cases and inputs, not universal guarantees; prior PRF-3/4 cover stability selection and adjudication.
- **Hand label / positive / near miss / constructed / re-blessing:** clear provenance and workflow terms, not extra validation.
- **Evaluation position:** honestly qualitative and non-CTest; no invented automated acceptance result.
- **Invariant check / mutant:** check internal relations and rejection sensitivity, not independent truth of every input value.
- **`motifs_include` / `positional_include` / `text_contains` / `full_text_contains` / `playable_includes` / `runs`:** accurately scoped partial expectations, strings and extra invocations.
- **CTest label / tactics_puzzles / games_oracle:** dependency and comparison contracts are explicit; byte equality establishes counter agreement, not the interpretation of game frequencies (see REP2-1).
- **Oracle sample / sanitize fixtures / gate / Release–ASan builds:** explicit bounded selection and instrumentation; sanitizer success applies to exercised paths only.
- **Smoke command / golden test / timing check:** one end-to-end invocation, byte equality, and isolated default-run timing respectively—not general correctness or every selector's latency.
- **Usage guard:** an operational workload safeguard, not a chess-quality measure.
- **Report-v8 replay evidence:** legal `line_fens` replay establishes board/line consistency, not upstream semantic truth; see critique 07 rather than duplicating it.
