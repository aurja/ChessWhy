# Current state

Updated 2026-10-25.

- Milestone: **M3a and M3b complete, merged into `main` 2026-10-22** (Captain ran both
  smoke commands and approved). Receipts `Planning/receipts/M3a.md` and
  `Planning/receipts/M3b.md`; contracts `Planning/milestones/M3a.md` (changes §15) and
  `Planning/milestones/M3b.md` (changes §13). Report v4.
  - M3a: 24 positional tags (`rule`) for pawn structure, king safety, pieces, space,
    development and material. `position.positional` lists the root's features; every
    candidate and asked-move line lists each change at its ply with `holds_at_end`.
    70 P feature boards (10 from books) and 7 P lines; `positional_oracle` 0 mismatches
    on 53,378 boards.
  - M3b: game counts (`games`) from 81 months of Lichess broadcasts (2020-01..2026-09),
    both players 2200+, ≤ 40 plies, ≥ 5 games: 263,568 games kept, 117,058 positions,
    table `lichess_broadcast_2200@e0cb7fab3212`, built by `tools/fetch_assets.sh games`.
    The Python oracle over all 81 months and a rebuild from a clean directory both give
    the pinned SHA-256. `--games FILE`; a missing or invalid table exits 3.
  - Release and ASan/UBSan CTests 141/141 (107 `unit`, 33 `engine`, 1 `games`); slowest
    `standard` F fixture 7.80 s (F06).
- Milestone: **PostM3_EM2 complete, merged into `main` 2026-10-23** (Captain ran the
  smoke command and called it good at checkpoint 3). Receipt
  `Planning/receipts/PostM3_EM2.md`; contract `Planning/milestones/PostM3_EM2.md` (changes
  §15). Report v5: restricted and free moves over the move set (hypothetical pawn
  captures; never a king capture), contribution rule 3′, `removal_of_defender`
  continuity, data digests, semantic checker, conditions in the text, new positional
  tag `tripled_pawns` (25 positional tags), T60. Release and ASan/UBSan CTests 155/155
  (118 `unit`, 37 `engine`/`games`; the checkpoint-3 additions were gated in Release
  only); slowest `standard` F fixture 7.91 s (F12).
- Milestone: **M4 playable moves and move reports complete, merged into `main`
  2026-10-24** (Captain approved at checkpoint 3). Receipt
  `Planning/receipts/M4.md`; contract `Planning/milestones/M4.md` (changes §15). Report
  v6: one `--moves` flag, verdict scale best/okay/inaccuracy/mistake/blunder, tactic and
  opportunity, one move report per reported move with a relevance filter, four outputs
  (`--json` × `--full-report`), projection check; F32–F39, E19–E22. Width dependence
  accepted as intended at checkpoint 2. Release CTests 179/179 (134 `unit`, 43 `engine`,
  2 `games`); ASan/UBSan all pass (F32 rerun alone at 1,730 s after the timeout raise).
  The root `back_rank_weakness` behind Re8# now prints through `root_motifs`. Milestones
  renumbered: M5 = libllama prose (five GGUFs in `assets/models/`; pinning them is an
  M5 task), M6 = books, M7 = native apps.
- Milestone: **PostM4_EM3 tempo, initiative and the impact rating complete, merged into
  `main` 2026-10-24** (Captain ran the smoke commands at checkpoint 3). Receipt
  `Planning/receipts/PostM4_EM3.md`; contract `Planning/milestones/PostM4_EM3.md`
  (changes §17). Report v7: tempo value with tempo-dependent / non-tempo / zugzwang
  labels, tempo ledger on every walked ply of every detailed line (budget `L`, 30k at
  `standard` after the timing fallback), initiative runs with ending and outcome, net
  change and the use test in move reports, impact order; F40–F44, PU01–PU18. At
  checkpoint 2 V2 follows the built ledger (Bxa4 neither; White's Bxa7 the only tempo).
  Test redesign (Captain, 2026-10-24): one analysis per fixture run, oracle sample under
  `unit`, labels `corpus`, `timing`, `sanitize`, ASan at `-O1`. Release
  `unit|engine|games|corpus` 205/205 in 376 s, `timing` 261 s (F12 12.0 s, limit 15 s);
  ASan/UBSan `unit|sanitize` 168/168 in 101 s; F32 `--moves playable` 27.9 s (limit 30 s).
- Milestone: **PostM4_EM4 the conceptual critique and the move report text complete, merged
  into `main` 2026-10-25** (branch `postm4-em4`). Receipt
  `Planning/receipts/PostM4_EM4.md`; contract `Planning/milestones/PostM4_EM4.md`. Report
  v8: motif repeats merged (`also`), compact restricted-movement rows, `line_fens`, the
  position walk (no tempo rows in the move report text for now). Conceptual critique of all
  definitions and concepts in `Planning/critiques/2026-10-24/` (00–08, synthesis 09): 20
  findings, 6 `wrong claim`. The planned part 2 became PostM4_EM5 (Captain, R4). Release
  `unit|engine|games|corpus` 213/213, `timing` pass (F12 12.4 s), ASan/UBSan
  `unit|sanitize` 176/176.
- Milestone: **PostM4_EM5 part 1, reply evidence, complete on branch `postm4-em5`
  2026-10-25** (not merged; parts 2 and 3 follow on the same plan). Receipt
  `Planning/receipts/PostM4_EM5.md`; contract `Planning/milestones/PostM4_EM5.md` §3
  (changes §4). Report v9: terminal draws and the skipped refutation, the settled walk,
  reply gain and forced test, the ply labels gaining / unforced / unresolved reply (a tempo
  only against a pure reply), run endings answered / unresolved and outcome unknown,
  `captured`, the king-file shelter witness; tempo rows in the move report text; the
  forced-test suffix in the full report text (checkpoint 2). F45, F46, PU19, PU20 new; F07,
  F21, F25, F28, F40, F41, F42 re-blessed. Release `unit|engine|games|corpus` 224/224,
  `timing` 309 s (F12 13.1 s, limit 15 s); ASan/UBSan `unit|sanitize` 185/185; F32
  `--moves playable` 24.6–29.9 s (limit 30 s).
- Milestone: **PostM4_EM6, search phase and explanation phase, complete on branch
  `postm4-em6` (from `postm4-em5`) 2026-10-25** (not merged). Receipt
  `Planning/receipts/PostM4_EM6.md`; contract `Planning/milestones/PostM4_EM6.md` §3
  (changes §4). `--level` and `--budget` removed; `--search-budget` (18) and
  `--explanation-budget` (14) depths; search A splits into A-best and A-rest (the best move
  no longer depends on the width); the at-least-score test; unclamped root margin; 120 s
  hang guard; report v10. Every F fixture re-blessed; F47 new (Rh8 and Rd3 both `best`).
  Release `unit|engine|games|corpus` 225/225; `timing` a measurement only, 957.7 s (F12
  60.0 s); ASan/UBSan `unit|sanitize` 185/185; F32 `--moves playable` 58.2 s, `--moves
  all` 126.0 s. Speed is deferred to a later milestone.
- Next: **PostM4_EM5 part 2, correctness** (D10–D14 open for the Captain; plan §2 part 2
  table, which also holds the forced test's cost, the collector audit and the P11 en
  passant wording), then part 3 (D15–D16, block approval). Each part's contract is frozen
  before its work starts; fresh orchestrator session.
- Test tooling (Captain's requests, 2026-10-24): long test drivers print progress lines;
  `tools/run_tests.py --build-dir DIR [ctest args]` streams them and writes
  `DIR/test_progress.json`; `.omp/extensions/test-progress.ts` shows running tests in
  omp (loads at omp start).
- Decided 2026-10-22 (Captain, option A): lines keep printing every positional change
  (F26/F27 show 20–25 rows per line). Superseded for the output by M4's relevance
  filter (net change over the walk); the full report keeps every change.
- DBC (Driven by Concepts), since 2026-10-21: `Planning/DBC/` holds CONCEPTS (concept
  diagrams; first version approved as the current baseline, Captain reviewing), CODE (concept → source), DEFINITIONS
  (glossary, was `Planning/definitions/TERMINOLOGY.md`), FIXTURES with boards and TESTS,
  kept current by the orchestrator; drift check `python3 tools/check_dbc.py`.
- Scope proposals still open from the M2 recall-miss audit: forks by a capturable piece,
  skewers with a pawn behind, discovered attacks on pawns or defended equal pieces.
- Evaluation positions V1–V3 (Captain, 2026-10-24; `Planning/DBC/FIXTURES.md` §10):
  a quiet move, a loud move and a mating attack, explained by the Deputy from
  ChessWhy's text only and compared with trainers' analysis. ChessWhy reported no wrong
  fact; the gaps are in the text (comparisons between moves, the exchange behind a
  pin, ply order, repeated and positional rows, ES saturation). §10.1 records which
  rows were useful and which were noise, for the M5 fact sheet. Test results, not
  scope proposals; the Captain's directive from V1 stands: where the best move has no
  clear reason of its own, explain why the other moves do not work.
- Captain's stance (2026-10-21): comprehensiveness first, conciseness later
  (`pin_tactical` adds about 9.6 reported motifs per puzzle).
- PostM2_EM1 complete, merged into `main` 2026-10-21 (receipt
  `Planning/receipts/PostM2_EM1.md`): tactic judgement, contributions,
  `removal_of_defender`, `pin_tactical`, report v3; CTests 111/111.
- M2 complete, merged into `main` (receipt `Planning/receipts/M2.md`): 13 motifs, report
  v2, recall targets met, reference implementation 0 mismatches.
- Decided 2026-10-19/20 (M2 D1–D13): see M2 §1. ChessWhy stays MIT; lichess-puzzler
  (AGPL-3.0) is read for definitions only, never copied, and credited.
- Engine CTests: the F fixture tests run in parallel; `cli_timing` is one serial
  measurement, registered only with `CHESSWHY_CHECK_TIMING` on (Release); no time limit
  from 2026-10-25 (15 s before). Speed is deferred: a later milestone makes it fast.
  CTest `TIMEOUT 3600` on `tactics_oracle` and the F fixture tests (F32 takes about 29 min
  alone under ASan/UBSan at M4).
- M1 complete, merged into `main` (receipt `Planning/receipts/M1.md`): CLI, engine
  analysis, report v1. Release and ASan/UBSan CTests 73/73; slowest `standard` fixture
  6.67 s.
- Contract changes during M1: `Planning/milestones/M1.md` §16 (PGN reader, fixture
  rebuild, `MultiPvCollector`, parry provenance, stalemate after the null move, FEN
  counter bounds, `chesswhy-pgn` argument order, F16 dropped).
- Vision: `Planning/PROJECT_VISION.md`. M0: `Planning/receipts/M0.md`.
- `.clang-tidy`: third-party headers excluded, `clang-analyzer-optin.core.EnumCastOutOfRange`
  off (it fired only inside chess-library).
- System packages from `.omp/RESOURCE_WISHLIST.md` §1 (git-ignored): all installed;
  `.venv/` holds the schema validator.
- Decided 2026-10-17: license MIT; verdict thresholds 0.03 / 0.13 / 0.27.
- Open decisions for the Captain: default model
  (M5), native app design and platforms (M7; decided 2026-10-22: native apps on Linux
  and Windows, possibly Android, replace the local HTTP API).
