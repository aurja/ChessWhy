# Current state

Updated 2026-10-20.

- Milestone: **M2 complete on branch `m2`, awaiting checkpoint 3** (the Captain tries
  the smoke command and approves merging `m2`). Receipt `Planning/receipts/M2.md`;
  contract `Planning/milestones/M2.md` (changes §15). 13 motifs, report v2, recall
  targets met on test, reference implementation with 0 mismatches on test, dev and
  T fixtures; Release and ASan/UBSan CTests 93/93; slowest `standard` fixture 7.98 s.
- Next milestone: **PostM2_EM1** (vision §11), decided at checkpoint 2b (M2 D11–D13).
  Carry-over:
  - Tactic = a line whose engine-evaluation gain under best replies is measured
    against the second-best move (only-move margin, option A).
  - Fill `Motif::contribution` (`direct`, `indirect`, `none`); always null in M2.
  - `removal_of_defender` returns as a tactic-side label (not a motif). Seed code from
    the removed engine-confirmation tool: commits `ebe0b7f`–`d5b703a` on `m2`.
  - Baseline: the last engine-confirmed precision table in the M2 receipt.
  - Scope proposals from the M2 recall-miss audit (receipt): forks by a capturable
    piece, skewers with a pawn behind, discovered attacks on pawns or defended equal
    pieces are outside the M2 definitions.
- Decided 2026-10-19/20 (M2 D1–D13): see M2 §1. ChessWhy stays MIT; lichess-puzzler
  (AGPL-3.0) is read for definitions only, never copied, and credited.
- Engine CTests run serially (`-j1`); `tactics_oracle` takes ~12 s in Release.
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
