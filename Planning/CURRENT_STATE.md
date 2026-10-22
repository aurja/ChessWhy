# Current state

Updated 2026-10-22.

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
- Next: M4 (libllama prose). Four candidate GGUFs are downloaded to `assets/models/`;
  pinning them is an M4 task.
- Decided 2026-10-22 (Captain, option A): lines keep printing every positional change
  (F26/F27 show 20–25 rows per line); M4 decides what the prose says.
- DBC (Driven by Concepts), since 2026-10-21: `Planning/DBC/` holds CONCEPTS (concept
  diagrams; first version approved as the current baseline, Captain reviewing), CODE (concept → source), DEFINITIONS
  (glossary, was `Planning/definitions/TERMINOLOGY.md`), FIXTURES with boards and TESTS,
  kept current by the orchestrator; drift check `python3 tools/check_dbc.py`.
- Scope proposals still open from the M2 recall-miss audit: forks by a capturable piece,
  skewers with a pawn behind, discovered attacks on pawns or defended equal pieces.
- Captain's stance (2026-10-21): comprehensiveness first, conciseness later
  (`pin_tactical` adds about 9.6 reported motifs per puzzle).
- PostM2_EM1 complete, merged into `main` 2026-10-21 (receipt
  `Planning/receipts/PostM2_EM1.md`): tactic judgement, contributions,
  `removal_of_defender`, `pin_tactical`, report v3; CTests 111/111.
- M2 complete, merged into `main` (receipt `Planning/receipts/M2.md`): 13 motifs, report
  v2, recall targets met, reference implementation 0 mismatches.
- Decided 2026-10-19/20 (M2 D1–D13): see M2 §1. ChessWhy stays MIT; lichess-puzzler
  (AGPL-3.0) is read for definitions only, never copied, and credited.
- Engine CTests run serially (`-j1`). ASan/UBSan `tactics_oracle` runs for about 25 min
  (CTest `TIMEOUT 3600` on that test).
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
- Open decisions for the Captain: default model (M4), native app design and platforms
  (M6; decided 2026-10-22: native apps on Linux and Windows, possibly Android, replace
  the local HTTP API).
