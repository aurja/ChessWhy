# Current state

Updated 2026-10-18.

- Milestone: **M1 batch 1 done** on branch `m1` (`Planning/milestones/M1.md`): WP1–WP5
  (input, engine, openings, report + render, analysis) built; Release CTests 34/34
  (31 unit, 3 engine). Next: batch 2 (WP6 CLI + integration), then WP7 (README,
  LICENSE) and the milestone gate. M0: `Planning/receipts/M0.md`.
- Contract changes 2026-10-18 (M1 §16): `to_json` → `ordered_json`; F03 and F11 fixtures
  re-blessed against SF19. Two of three F fixtures checked so far were stale; WP6 may
  find more.
- `.clang-tidy`: third-party headers excluded, `clang-analyzer-optin.core.EnumCastOutOfRange`
  off (it fired only inside chess-library).
- Vision: `Planning/PROJECT_VISION.md`.
- System packages from `.omp/RESOURCE_WISHLIST.md` §1 (git-ignored): all installed.
  M1 setup on debian-home complete (M1 §13); `.venv/` holds the schema validator.
- Decided 2026-10-17: license MIT; verdict thresholds 0.03 / 0.13 / 0.27.
- Open decisions for the Captain: default model (M4), HTTP consumer details (M6).
