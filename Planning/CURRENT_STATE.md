# Current state

Updated 2026-10-23.

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
- Now: **M4 playable moves and move reports** (Captain's redefinition, 2026-10-23;
  contract `Planning/milestones/M4.md`, frozen at checkpoint 1 on 2026-10-23, branch
  `m4`; batch 1 = WP1 core cutover and batch 2 = WP2–WP5 done (report v6, move
  reports, `--moves`, move report text, projection check; `unit` 134/134 green in
  Release; `engine` fixtures still use `--move` and wait for the WP6 re-bless); next
  batch 3 = WP6 fixtures, then checkpoint 2). Milestones
  renumbered: M5 = libllama prose (five GGUFs in `assets/models/`; pinning them is an
  M5 task), M6 = books, M7 = native apps. M4 closes the item deferred on 2026-10-23
  (the text template does not print `position.motifs`, e.g. the root
  `back_rank_weakness` behind Re8#) through `root_motifs` in the move report.
- Decided 2026-10-22 (Captain, option A): lines keep printing every positional change
  (F26/F27 show 20–25 rows per line). Superseded for the output by M4's relevance
  filter (net change over the walk); the full report keeps every change.
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
- Open decisions for the Captain: default model
  (M5), native app design and platforms (M7; decided 2026-10-22: native apps on Linux
  and Windows, possibly Android, replace the local HTTP API).
