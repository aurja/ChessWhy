# Searches critique (06): SRCH-1 to SRCH-5

Baseline `23bf1a52d856060556147f73c1bd5f90c67c8d5d` · 2026-10-22 · schema v4 · M4/M5 planned, not built · index: [README](README.md)

## Baseline and concept nodes

- **Engine.** Stockfish 19 (`sf_19`), Threads 1, Hash 64 ([cli.cpp](../../src/cli/cli.cpp)).
- **Budgets.** Standard: A 1,000,000; B, C 200,000; D, E, R 100,000 ([`node_budget`](../../src/analysis/rules.cpp); kinds per [M1 §3](../milestones/M1.md#3-budgets-and-engine-settings)). R searches the refutation's alternative ([PostM2](../milestones/PostM2_EM1.md) §3). Quick and Deep scale rows by ¼ and 4×.
- **Units.** es-units = 2W+D, 0–2000 ([M1 §4](../milestones/M1.md#4-analysis-rules)). Fine ≤ 60; inaccuracy 61–259; mistake 260–539; blunder ≥ 540 ([M1 §4.7](../milestones/M1.md#47-moves-and-verdicts)). Tactic margin ≥ 260 ([PostM2 §4.1](../milestones/PostM2_EM1.md#41-tactic-judgement)).
- **Working terms (not official).** *Deterministic*: identical output for identical configuration on this host. *Stable*: same category across budgets; not guaranteed.

## Conclusion

Fixed settings support reproducible reruns; they do not establish stability across budgets or comparable depth across searches. Qf6 changes verdict across budgets (SRCH-1). Cross-kind scores disagree, but those comparisons do not by themselves prove which grade is correct (SRCH-2). History and executable identity are scope/provenance gaps (SRCH-3/4); the WDL scale is an accepted limitation (SRCH-5).

## Approach

Labels: OBSERVED runtime output from the parent/verdicts investigator; SOURCE code or contract; INFERENCE unobserved consequences; PROPOSED unrun changes. The search investigator inspected source and shared artifacts; it ran no additional searches.

Central root (OBSERVED): 1.e4 e5 2.Bc4 Nc6 3.Qh5, FEN `r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 3 3`. Asked Nf6 (`g8f6`): ES 0, mate −1, loss 1055, blunder. Hand check: 4.Qxf7# follows.

## Prioritized findings

### SRCH-1 — Verdicts sit on hard thresholds that move with budget and depth

- **Root cause.** `MultiPvCollector::finish` selects the newest exact line for each final-batch move, potentially from different depths. Bound lines are excluded; "exact" does not mean converged. Reports record requested per-kind budgets and aggregate `nodes_searched`, not achieved work/depth per search and PV (SOURCE and OBSERVED).
- **Evidence.** One A search at the F11 child root (`go nodes 1000000`, MultiPV 5; `critique-verdicts-child-uci.txt`): Ra8 (`b8a8`) is 774 at depth 15 (651,604 nodes), then 519 at depth 16 (1,000,450 nodes), a 255-unit change in one search. PVs 4–5 stop at depth 15 (OBSERVED). Across budgets (Qf6 = `d8f6`; Nf6 is not in MultiPV):

| Budget | Aggregate nodes_searched | Qf6 ES (units) | Qf6 loss (units) | Verdict | d5 refutation: alternative, margin (units) |
|---|---:|---:|---:|---|---|
| Quick | 586,741 | 984 | 57 | fine | exd5, 953 (tactic) |
| Standard | 2,412,161 | 975 | 80 | inaccuracy | exd5, 1123 (tactic) |
| Deep | 9,616,072 | 970 | 85 | inaccuracy | Bb5, 1164 (tactic) |

- **Impact.** Quick loss 57 sits 3 units under the fine bound. The d5 tactic label holds at every budget; its named alternative does not.
- **Strongest defense.** Thresholds necessarily have boundaries; deeper is not ground truth. M1 §16 historically screened fixtures across budgets, and requested budgets are already reported. No nondeterminism was observed.
- **Proposal.** Keep the default budgets. First measure category changes against distance from each threshold; annotate closeness using existing values. Recording achieved work/depth would improve diagnosis. A runtime `budget_sensitive` label would require extra searches and is a separate cost decision.
- **Acceptance experiment (PROPOSED).** On the calibration roots and fixtures, measure three-budget flips for loss, parry and margin. Test whether a ±10-unit warning band leaves at most 1% of rows with an unflagged flip; widen or abandon that band if not. This is an empirical target, not a proven error bound.

### SRCH-2 — Margins subtract values from different kinds, budgets and depths

- **Root cause.** A (MultiPV, 1,000,000 nodes), C (single PV, requested 200,000), R (100,000) and child-root A are subtracted as if comparable. [PostM2 §4.1](../milestones/PostM2_EM1.md#41-tactic-judgement) accepts negative margins for this reason. Kind and budget are confounded (INFERENCE).
- **Evidence.** Standard unless noted:

| Comparison | Value 1 | Value 2 | Abs. Δ (units) |
|---|---:|---:|---:|
| Qe7: A vs C | 1013 | 1035 | 22 |
| Qf6: A vs C | 975 | 955 | 20 |
| Nh6: A vs C | 966 | 979 | 13 |
| exd5 after 3…d5: R vs child-root A | 829 | 674 | 155 |
| Ra8 after h3 (F11): R vs child-root A (CLI) | 785 | 519 | 266 |

The C-style direct-UCI values differ from A without changing those three verdicts. Last recorded exact Qe7/Qf6 lines occur before the requested cap; requested nodes are not a completed common-depth guarantee.

F11 root `1r1q1rk1/4b1pp/4b3/1p1pPp2/3p1P2/8/1PBB2PP/R2Q1RK1 w - f6 0 18`, `--budget standard --move h3 --json` (CLI, exit 0). Child `1r1q1rk1/4b1pp/4b3/1p1pPp2/3p1P2/7P/1PBB2P1/R2Q1RK1 b - - 0 18`, `--budget standard --json` (CLI, exit 0). The separate UCI replay establishes the mixed depths; 519 also appears in the actual child CLI report, not only in the replay.

- **Impact.** The 266-unit score spread exceeds the tactic threshold. It could change a near-threshold classification (INFERENCE); no tactic flip is established here. F11's negative margin is explicitly permitted by the current contract.
- **Strongest defense.** Saturated F12 null-board and restricted `searchmoves d3e5` runs at 100,000/200,000 give WDL 1000/0/0. d5 margins of 953–1164 remain far above 260.
- **Proposal.** Measure paired same-root restricted searches before changing the design. Pairing inherited r1 against an excluded-r1 search at equal budgets tests one confound; replacing r1 with a child-root best reply changes the definition. Neither equal budgets nor one MultiPV search guarantees equal depth.
- **Acceptance experiment (PROPOSED).** Report paired |Δ| percentiles and classification changes. A candidate design should target 95th-percentile |Δ| below 60 units; failure rejects that target, not the operational definitions or all bounded search.

### SRCH-3 — Repetition history never reaches the engine

- **Root cause.** [`SearchRequest`](../../include/chesswhy/engine.hpp) supplies a FEN rather than a preceding move history; [`Engine::Impl::search`](../../src/engine/engine.cpp) sends `position fen`. The PGN mainline reaches opening lookup, not engine history (SOURCE). Stockfish's [repetition logic](https://github.com/official-stockfish/Stockfish/blob/sf_19/src/position.cpp) needs earlier positions.
- **Example (PROPOSED, UNRUN).** PGN `1. Nf3 Nf6 2. Ng1 Ng8 3. Nf3 Nf6 4. Ng1 *`; inspect the position after seven plies and ask Ng8. Hand-derived root `rnbqkb1r/pppppppp/5n2/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 7 4`. After 4…Ng8 the start position occurs for the third time. CLI `--ply` handling was not exercised for this example.
- **Impact.** Search cannot use pre-root repetition history that was discarded (SOURCE). The resulting evaluation difference is unmeasured; a near-equal opening may get the same score either way.
- **Strongest defense.** The [input contract](../milestones/M1.md#5-input) is position-based, and a bare FEN cannot encode history. This is a scope boundary, not evidence that a frozen position-only acceptance test fails.
- **Proposal.** Explicitly state the history-free limit. If game-aware PGN analysis is wanted, preserve its initial position and mainline (including setup-FEN games), rather than always using `startpos`.
- **Acceptance experiment (PROPOSED).** Compare identical boards with different legal repetition histories against a history-aware reference, including an independently established claimable draw. Equal scores on the neutral example do not falsify history loss; do not use "bestmove must differ" as the criterion.

### SRCH-4 — Identity records configuration, not the executable

- **Root cause.** [`EngineInfo`](../../include/chesswhy/report.hpp) records engine name/settings, not executable digest, NNUE identity or ChessWhy revision. [identity_test.cpp](../../tests/cli/identity_test.cpp) historically checks same-host reruns, not artifact equivalence (SOURCE; test not rerun).
- **Evidence.** OBSERVED UCI name: Stockfish 19; executable SHA-256: `0f83d24cc46d2c66c60f16001af5444873bc112b7d028594513426894c12da19`. The fetch manifest pins a release archive; its hash is not expected to equal the extracted executable's hash. No mismatch is alleged.
- **Impact.** Different binaries can share the same reported identity even when results differ (INFERENCE). Identical configuration is insufficient to reconstruct a past run.
- **Strongest defense.** [fetch_assets.sh](../../tools/fetch_assets.sh) pins the downloaded artifact, and the project has same-host identity tests. These are useful controls, just narrower than a portable reproducibility certificate.
- **Proposal.** Record a ChessWhy revision and executable identity in a versioned report or replay receipt. Avoid collecting expensive environment telemetry: identify the actual inputs first.
- **Acceptance experiment (PROPOSED).** Different executable contents produce different recorded content identities; repeated use of the same contents preserves identity regardless of pathname. Check the report against the loaded artifact.

### SRCH-5 — ES depends on a material-dependent WDL curve

- **Root cause.** ES derives from Stockfish WDL, whose [win_rate_model](https://github.com/official-stockfish/Stockfish/blob/sf_19/src/uci.cpp) fits long-time-control fishtest statistics with material-dependent parameters (SOURCE). The scale limitation is accepted in [M1 §4.7](../milestones/M1.md#47-moves-and-verdicts).
- **Evidence.** README d5 candidate, standard: cp −175, ES 48 units (.024), loss 1007, blunder (OBSERVED).
- **Impact.** This is engine expected score, not a calibrated prediction for this user's games. Material-dependent scaling may affect comparisons across positions (INFERENCE; no isolated effect measured).
- **Strongest defense.** The limit is recorded; integer units avoid floating-point boundary errors. A fixed centipawn curve is not automatically a better chess scale.
- **Proposal.** Keep ES and its explicit engine scope. An offline fixed-curve comparison can measure classification sensitivity, not establish a replacement as truth.
- **Acceptance experiment (PROPOSED).** Count changes on a fixed corpus and inspect their chess relevance independently. Zero changes would bound observed sensitivity on that corpus, not validate human win-probability claims.

## Strengths

- Fixed nodes, `Threads=1`, `Hash 64`, `ucinewgame` before each search ([M1 §2](../milestones/M1.md#2-calibration-evidence), historical).
- Integer es-units; accepted limits recorded with reasons ([M1 §4.7](../milestones/M1.md#47-moves-and-verdicts); [PostM2 §4.1](../milestones/PostM2_EM1.md#41-tactic-judgement)).

## Verification limits

- Existing Release binary and project Stockfish were exercised; no full suite, rebuild, timing benchmark or corpus calibration was run for this critique.
- Reports expose aggregate work and requested budgets; the direct-UCI replay supplies achieved depths for the mixed-depth example.
- The optional ±1% A-jitter probe failed before a search began; it supplies no evidence.
- The probe executable was not rebuilt against the reviewed commit. Runtime observations belong to that local build; source/contract conclusions are separately identified.

## Sources

Repo links above. Local records read, not linked: `critique-verdicts-probe-records.json`, `critique-verdicts-child-uci.txt`, `critique-verdicts-f11-standard-h3.json`, `critique-verdicts-f11-after-h3-standard.json`, `critique-readme-{quick,standard,deep}.json`, `critique-d5-refutation-root.json`, `critique-direct-uci.json`, `critique-recovery.md`, `handoff-CritiqueSearches.md`.
