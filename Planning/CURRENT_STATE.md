# Current state

Updated 2026-10-18.

- Milestone: **M1 complete** on branch `m1` (receipt `Planning/receipts/M1.md`): CLI,
  engine analysis, report v1. Release and ASan/UBSan CTests 74/74; slowest `standard`
  fixture 6.67 s. Awaiting the Captain's smoke check and approval to merge `m1`
  (M1 §13 item 4).
- Contract changes during M1: `Planning/milestones/M1.md` §16 (PGN reader, fixture
  rebuild F01–F20, `MultiPvCollector`, parry provenance, stalemate after the null move,
  FEN counter bounds, `chesswhy-pgn` argument order).
- Scope proposal for the Captain: drop F16 (receipt "Open").
- Vision: `Planning/PROJECT_VISION.md`. M0: `Planning/receipts/M0.md`.
- `.clang-tidy`: third-party headers excluded, `clang-analyzer-optin.core.EnumCastOutOfRange`
  off (it fired only inside chess-library).
- System packages from `.omp/RESOURCE_WISHLIST.md` §1 (git-ignored): all installed;
  `.venv/` holds the schema validator.
- Decided 2026-10-17: license MIT; verdict thresholds 0.03 / 0.13 / 0.27.
- Open decisions for the Captain: default model (M4), HTTP consumer details (M6).
