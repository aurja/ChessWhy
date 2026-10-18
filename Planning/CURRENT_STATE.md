# Current state

Updated 2026-10-18.

- Milestone: **M1 complete, merged into `main`** with the Captain's approval (receipt
  `Planning/receipts/M1.md`): CLI, engine analysis, report v1. Release and ASan/UBSan
  CTests 73/73; slowest `standard` fixture 6.67 s.
- Next: M2, tactical motif detectors (vision §5.5, milestone table). Start in a fresh
  orchestrator session; contract to be frozen in `Planning/milestones/M2.md`.
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
