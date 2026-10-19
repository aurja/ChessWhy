# Current state

Updated 2026-10-19.

- Milestone: **M2 batch 1 integrated** on branch `m2`: tactical motif detectors. Plan
  `Planning/milestones/M2.md`, frozen except the motif fixture table (§9) and the
  calibrated definitions and targets (§8), which the Captain approves at the §12
  checkpoints. Batch 1 landed WP1 (SEE, all §4 detectors, unit tests, T-fixture loader),
  WP2 (puzzle sampler, `tests/fixtures/puzzles/test.csv`, `tactics_puzzle_eval`), WP3
  (report v2 JSON, motif rows in the template) and WP5 (proposed T01–T36 and F
  `motifs_include` rows, smoke command). Release CTests: unit 64/64, engine 23/23.
  Waiting on: checkpoint 1 (Captain reviews T table, F motif rows, smoke command). Next:
  batch 2 (WP4 analysis wiring, `motifs_include` in the F harness).
- Decided 2026-10-19 (M2 briefing D1–D10): see M2 §1. ChessWhy stays MIT; lichess-puzzler
  (AGPL-3.0) is read for definitions only, never copied, and credited.
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
- Open decisions for the Captain: default model (M4), HTTP consumer details (M6).
