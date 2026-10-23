# 09 — Reports

Baseline `23bf1a52d856060556147f73c1bd5f90c67c8d5d`, 2026-10-22, schema v4. M4 and M5 planned, not built.

## Baseline and concept nodes

- **Report.** Typed `Report` ([report.hpp](../../include/chesswhy/report.hpp)) feeds `--json` and text. The [v4 schema](../../schema/report.v4.schema.json) closes 27 objects (`additionalProperties: false`); nullable keys are always present. SOURCE.
- **Provenance, budgets.** `source` names the producing step, not confidence ([PROJECT_VISION.md](../PROJECT_VISION.md) §2). `engine.nodes` records requested per-kind budgets (standard: A 1,000,000; B, C 200,000; D, E, R 100,000). SOURCE.

## Conclusion

Keep the architecture: typed report, closed definitions, explicit provenance. Meaning lives outside the document: premises sit in plans, `source` names only the last step, structural checks accept contradictions (REP-3), and reproducibility records settings rather than code and data identity. Prioritize schema descriptions and semantic invariants; use small, versioned additions where identity or context cannot otherwise be recovered. No normalization rewrite or confidence model.

## Approach

The task investigator ran the validator and custom-table probe; it and the parent exercised the CLI. Saved artifacts were checked during integration. Labels: OBSERVED runtime output, SOURCE code/contracts, INFERENCE untested consequences, PROPOSED unrun changes. Release `build/release/chesswhy`, Stockfish 19, Threads 1.

- Nf6 probe: `--fen 'r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 3 3' --move Nf6 --budget standard --json` (quick and deep change only `--budget`).
- Root after Nf6: `--fen 'r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4' --budget standard --json`.

## REP-1 Premises implicit in path

- **Root cause.** One `make_threat` ([analyse.cpp](../../src/analysis/analyse.cpp)) builds the A side-to-move line, B null-move threat, E `creates_threat`, and refutation inherited from an A/C reply. R supplies the refutation's alternative, not its original line. Judgements sit at `candidates[0].tactic`, its `asked_move` copy and `refutation.tactic`. No object explicitly names its search context. SOURCE.
- **Evidence.** OBSERVED (predecessor, Nf6 probe): `asked_move.creates_threat` is Black's threat if White passes (Nxh5…; material 9; root_fen `… b KQkq - 4 4`), beside `eval.mate` −1 and refutation `["Qxf7#"]`; the text gives no pass qualifier. Root `tactic.alternative` (A, 1M nodes) is Qd1; `refutation.tactic.alternative` is Qh4 at quick and standard, Qd1 at deep (R).
- **Impact.** INFERENCE: moot threats read as live.
- **Defense.** Meanings are frozen (M1 §4.2); `root_fen` encodes the pass; `engine.nodes` records per-kind budgets.
- **Proposal (PROPOSED).** Schema descriptions first; add search letters only if M4 shows lost qualifiers.
- **Invariants (PROPOSED).** Threat `root_fen` side equals `side`; creates_threat root has the mover to move; side-to-move threat line equals `candidates[0].line`; each present root motif has a non-null contribution iff `candidates[0].tactic.is_tactic`.
- **Acceptance (PROPOSED).** Pass on Nf6 and root reports; fail `threat_side_swapped` and `contribution_without_tactic` (REP-3).

## REP-2 Provenance = last-step origin

- **Root cause.** [`serialize_move`](../../src/report/report.cpp) places verdict, loss, `in_multipv` and `gives_check` under `engine`; verdict applies rule thresholds 60/260/540 to es-units ([analysis.hpp](../../include/chesswhy/analysis.hpp)). `input.source` separately means fen|pgn. `position.side_to_move`, `in_check`, `terminal` and `fine_truncated` carry no source. [render.cpp](../../src/render/render.cpp) adds `[rule]` to nulls. SOURCE.
- **Evidence.** OBSERVED (predecessor, saved text): `Candidate 3: Qf6 (d8f6), inaccuracy, loss=4.0% [engine]`. Saved JSON, predecessor-run:

| Budget | A nodes | Qf6 loss, verdict | Qf6 / Nh6 move-motif counts | Qxf7# refutation alt. | Asked Nf6 creates_threat material / plies |
|---|---|---|---|---|---|
| quick | 250000 | 0.0285, fine | 4 / 19 | Qh4 | 6 / 5 |
| standard | 1000000 | 0.04, inaccuracy | 20 / 17 | Qh4 | 9 / 6 |
| deep | 4000000 | 0.0425, inaccuracy | 20 / 8 | Qd1 | 9 / 6 |

`position.motifs`, `positional`, `games` and `opening` match across budgets; `eval_white` does not (es 0.4795 quick; 0.4725 standard, deep).

- **Impact.** INFERENCE: rule-labelled facts on engine lines (creates_threat consequence, motif counts) move with budget yet look stable.
- **Defense.** Vision §2 defines origin; the verdict is a function of `eval.es`; PostM2_EM1 §14 item 8 chose engine for judgements.
- **Proposal (PROPOSED).** No new values; document "source = producing step; rule facts on engine lines inherit budget dependence." Reconcile M1 §7 with the verdict's origin.
- **Acceptance (PROPOSED).** From descriptions alone, a reviewer names the producing step of each field that differs between quick and standard.

## REP-3 Schema validity does not establish semantic consistency

- **Root cause.** SOURCE: 5 `if` conditionals (lines 756, 782, 951, 989, 1268); 0 `description` keys. Line 951 ties `forced` to null `alternative` and `margin`, and `is_tactic` false. Nothing relates verdict to loss, terminal to candidates, or copies to candidates.
- **Evidence.** OBSERVED (investigator): 14 copies of the standard Nf6 JSON, one path changed each, pass `.venv/bin/python tools/validate_report.py schema/report.v4.schema.json <copy>` (exit 0); 2 controls exit 1.

| Mutant (path: from→to) | Contradiction |
|---|---|
| `verdict_vs_loss`: `$.candidates[0].verdict` fine→blunder | blunder, loss 0 |
| `blunder_without_refutation`: `$.asked_move.refutation` →null | refutation missing |
| `loss_not_best_minus_eval`: `$.asked_move.loss` 0.5275→0.01 | arithmetic |
| `threat_side_swapped`: `$.threats.white.side` white→black | side vs root_fen |
| `mate_class_without_mate_in`: `$.asked_move.refutation.consequence.mate_in` 1→null | mate class |
| `parry_source_test_mismatch`: `$.asked_move.parries_threat.test` searched→threat_move_illegal | engine source, rule test (M1 §16) |
| `null_move_reliable_null_not_in_check`: `$.threats.null_move_reliable.value` true→null | null outside check |
| `urgency_null_side_nonquiet`: `$.threats.urgency.more_urgent` white→null | rule consequence_class |
| `eval_cp_and_mate_conflict`: `$.asked_move.eval.cp` null→500 | cp with mate −1 |
| `in_multipv_true_absent`: `$.asked_move.in_multipv` false→true | absent from candidates |
| `terminal_with_searches`: `$.position.terminal` null→checkmate | candidates present |
| `is_tactic_below_margin`: `$.candidates[0].tactic.is_tactic` false→true | margin .021 |
| `contribution_without_tactic`: `$.position.motifs[0].contribution` null→direct | no root tactic |
| `alternative_not_candidate1`: `$.candidates[0].tactic.alternative.san` Qe7→Qh4 | not candidates[1] |

Controls (exit 1): `$.candidates[0].tactic.forced` false→true with an alternative; `$.position.motifs[0].gain` null→3.

- **Impact.** A schema-valid report can contradict itself and pass the validator.
- **Defense.** JSON Schema cannot express loss or margin arithmetic; F fixtures pin outcomes.
- **Proposal (PROPOSED).** Intra-object `if/then`: mate class ⇔ `mate_in`; `fine` ⇔ null refutation; `searched` ⇔ engine; `threats.white.side` const `white`; `more_urgent` null ⇔ rule `quiet`; `cp` null when `mate` set. Generalize `check_tactic_invariants` ([engine_fixture_test.cpp](../../tests/cli/engine_fixture_test.cpp)). INFERENCE: six of 14 are intra-object.
- **Acceptance (PROPOSED).** All 14 fail, both controls still fail, current reports pass.

## REP-4 Reproducibility omits code and data identity

- **Root cause.** SOURCE: `run` ([cli.cpp](../../src/cli/cli.cpp)) sets `reproducible` from threads and takes the engine name from the UCI id. No ChessWhy or rules revision appears (CMake 0.1.0 is unused). `opening.dataset` is `kDataset` ([openings.cpp](../../src/openings/openings.cpp)); `table` is `kTable` ([games.cpp](../../src/games/games.cpp)); months come from the file.
- **Evidence.** OBSERVED (investigator): one-row table (copied header; months 2025-01..2025-02; games read/kept 5; start-position e2e4 count 5; digest prefix `0abcccf99b1a`), terminal FEN `rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3`, `build/release/chesswhy --fen FEN --json --games <file>`: exit 0; reported `lichess_broadcast_2200@e0cb7fab3212`, the shipped identity. The tool transcript records the result; separate stdout was not retained.
- **Defense.** M3b §5 makes `table` = `kTable`; the 2026-10-21 change row relies on the SHA-256 that [fetch_assets.sh](../../tools/fetch_assets.sh) checks. This works for the fetched bytes, not every file at the default path. Budget flips are recorded (`input.budget`, `engine.nodes`): sensitivity, not nondeterminism ([05-verdicts.md](05-verdicts.md)).
- **Impact.** OBSERVED: a custom table reports the shipped identity. INFERENCE: consumers cannot identify which data produced the report from that field.
- **Proposal (PROPOSED).** Identify the bytes actually loaded for both default and custom datasets. Preserve supported custom inputs; do not reject them merely to keep the constant identity true. A separately requested pinned-only mode could reject mismatches. Add a rules revision at the next schema version.
- **Acceptance (PROPOSED).** The valid one-row input loads and reports its measured identity; changing the default file's bytes also changes its identity.

## REP-5 Nulls and copies mostly sound

- **Root cause.** Null meanings live in comments and plans, not the schema. The `parries_threat` comment ([report.hpp](../../include/chesswhy/report.hpp) line 274) omits the in-check case, where every candidate is null (check probe; M1 §4.5).
- **Evidence.** OBSERVED: check probe (`rnbqkbnr/ppp1pppp/3p4/1B6/4P3/8/PPPP1PPP/RNBQK1NR b KQkq - 1 2`, Bb5+): `in_check` true; `threats.white` and `null_move_reliable.value` null. Terminal probe: `[]` motifs, 12 positional facts; source inspection attributes the empty motif list to skipped computation. g6 probe (Nf6 FEN, `--move g6`): `asked_move` deep-equals `candidates[0]`; the side-to-move threat repeats its line and motifs; the root alternative copies `candidates[1]`. [05-verdicts.md](05-verdicts.md) separately demonstrates why `fine_truncated: false` does not certify exhaustive coverage.
- **Impact.** INFERENCE: consumers may misread null as absent, and summing motifs across copies double-counts.
- **Defense.** Judged-not-tactic is `{is_tactic: false}`, distinct from null/unjudged; copies are deliberate (PostM2_EM1 §15). A `positional` consequence is a residual class, not proof of "no threat". Check, game end and no legal move after a pass explain different unavailable results; consumers must preserve those distinctions.
- **Proposal (PROPOSED).** Keep both; put null meanings in schema descriptions; add equality invariants per copy.
- **Acceptance (PROPOSED).** The g6 report passes; `alternative_not_candidate1` fails.

## Strengths

Required nullable keys make absence explicit; parry source is recorded per candidate; `root_fen` and lines are replayable; games sources carry `found`; terminal reports keep engine name and settings.

## Verification limits

- CLI, validator and custom-table results are investigator/parent observations; the drafting pass did not rerun them.
- Unrun: proposed semantic validator, revision-identity changes and M4 consumption checks. M4 has no implementation yet.
- `reproducible` is threads-only in code. It is not a certificate of identical executable, data, hardware or future output.
- Probe engine: Stockfish 19, SHA-256 `0f83d24cc46d2c66c60f16001af5444873bc112b7d028594513426894c12da19`.
- [DEFINITIONS.md](../DBC/DEFINITIONS.md)'s "report" row names schema versions 2 and 3, not 4; unchanged here.

## Sources

[PROJECT_VISION.md](../PROJECT_VISION.md); [DEFINITIONS.md](../DBC/DEFINITIONS.md); [M1](../milestones/M1.md); [M2](../milestones/M2.md); [M3b](../milestones/M3b.md); [PostM2_EM1](../milestones/PostM2_EM1.md); [validate_report.py](../../tools/validate_report.py); [engine_fixture_test.cpp](../../tests/cli/engine_fixture_test.cpp); siblings [04-threats.md](04-threats.md), [05-verdicts.md](05-verdicts.md), [06-searches.md](06-searches.md), [10-explanations.md](10-explanations.md).
