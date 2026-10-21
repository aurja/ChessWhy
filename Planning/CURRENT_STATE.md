# Current state

Updated 2026-10-21.

- Milestone: **PostM2_EM1 complete, merged into `main`** (Captain, checkpoint 3,
  2026-10-21). Receipt `Planning/receipts/PostM2_EM1.md`; contract
  `Planning/milestones/PostM2_EM1.md` (changes §15). Tactic judgement (margin ≥ 0.13 over
  the next-best first move, search R), contributions `direct`/`indirect`/`none`,
  `removal_of_defender` tactic label, report v3, new state motif `pin_tactical` with
  `threat` (14 motif tags), shared "restricted movement" wording. Release and ASan/UBSan
  CTests 111/111; slowest `standard` F fixture 5.87 s; `tactics_oracle` 0 mismatches
  (about 23 s in Release).
- Next: M3, split by the Captain (2026-10-21) into **M3a** (positional facts) and **M3b**
  (opening statistics), planned together and built in parallel. Contracts not yet drafted.
  Decided: M3a scope = pawn-structure basics, king safety, pieces, space, development lead,
  material balance (named structure families and Silman's imbalances later). M3b source =
  Lichess broadcasts only, both players 2200+, January 2020 onward (online Lichess Elite
  dropped; broadcast license is CC BY-SA 4.0, was recorded as CC0). Minimum game count
  and ply limit: frozen after a pilot build (proposed 5 and 40). M3a measures changes at
  every ply of the candidate and asked-move lines, with a "still holds at the end" flag.
- DBC (Driven by Concepts), since 2026-10-21: `Planning/DBC/` holds CONCEPTS (concept
  diagrams; first version approved as the current baseline, Captain reviewing), CODE (concept → source), DEFINITIONS
  (glossary, was `Planning/definitions/TERMINOLOGY.md`), FIXTURES with boards and TESTS,
  kept current by the orchestrator; drift check `python3 tools/check_dbc.py`.
- Scope proposals still open from the M2 recall-miss audit: forks by a capturable piece,
  skewers with a pawn behind, discovered attacks on pawns or defended equal pieces.
- Captain's stance (2026-10-21): comprehensiveness first, conciseness later
  (`pin_tactical` adds about 9.6 reported motifs per puzzle).
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
- Open decisions for the Captain: default model (M4), HTTP consumer details (M6).
