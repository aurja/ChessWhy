# M3a receipt — positional facts

Date: 2026-10-22. Machine: `debian-home` (i7-8565U, 15 GiB, Debian 13). Branch `m3`
(shared with M3b). Contract: `Planning/milestones/M3a.md` (acceptance §14, contract
changes §15). Terms: `Planning/DBC/DEFINITIONS.md`.

## Delivered

- **Detectors** (§4): 24 tags with `rule` provenance in four groups. Pawn structure:
  `isolated_pawn`, `doubled_pawns`, `backward_pawn`, `passed_pawn`,
  `connected_passed_pawns`, `pawn_islands`, `pawn_chain`. King safety:
  `castling_kingside`, `castling_queenside`, `king_shield_gap`, `open_file_near_king`,
  `half_open_file_near_king`, `king_zone_attacker`. Pieces: `bishop_pair`,
  `rook_open_file`, `rook_half_open_file`, `rook_seventh`, `outpost`, `bad_bishop`,
  `mobility_lead`. Space, development and material: `space_lead`, `development_lead`,
  `material_ahead`, `exchange_up`.
- **Changes along a line** (§5): `position.positional` lists the root's features. Each
  candidate line and the asked move's line list every feature that appears or
  disappears, at its ply, with `holds_at_end`.
- **Report v4** (`schema/report.v4.schema.json`, shared with M3b). The template prints one
  row per change, e.g. `+ doubled_pawns: black, c file: p c6, p c7 (ply 2) [rule]`.
- **Fixtures**: P fixtures in `tests/fixtures/positional.json`, 70 feature boards and 7
  lines. 10 boards are book positions transcribed to FEN; the rest are constructed.
  F rows F25–F30 with the harness key `positional_include` (and `text_contains`).
- **Reference implementation** in `tests/positional/oracle/`, written from the plan text
  without reading the detectors. `positional_oracle` compares feature lists, line
  changes and the §4.4 count values.

## Verification

| # | Check | Result |
|---|---|---|
| 1 | Release and ASan/UBSan builds; CTests | Builds use `-Wall -Wextra -Wpedantic -Werror`. Final (after the review fixes): Release 141/141 (107 `unit`, 33 `engine` serial with `CHESSWHY_CHECK_TIMING=1`, 1 `games`). ASan/UBSan 141/141: `unit` and `games` 107/107 (408 s) plus `engine` 33/33 serial (750 s) after the review fixes; `tactics_oracle` (code unchanged since) passed in the branch's first ASan run (`unit` and `games` 106/106, 3,081 s) |
| 2 | P fixtures | all pass after checkpoint 2, including the 10 book positions; no rule/label mismatch came up |
| 3 | `positional_oracle` | 0 mismatches on 9,300 puzzles (test split): 53,378 feature lists, 9,300 lines, 320,268 count values; per-tag counts below |
| 4 | F fixtures | every earlier expectation and every new row passes; every F report validates against v4. F01–F24 facts were rechecked by hand at the gate and are unchanged (FIXTURES snapshot) |
| 5 | Rerun identity | `cli_rerun_identity` passes on F01, F03, F12, F25, F29 and F30 |
| 6 | `standard` wall time per F fixture | slowest F06 7.80 s in the final run (F11 7.23 s in the run before; limit 12 s), Release, serial |
| 7 | Facts first | every feature and change carries `rule`; the template prints only report fields; golden render tests pass |
| 8 | Gates | clang-format and `clang-tidy --quiet -p build/release` clean on all 20 changed C++ units; Ruff (lint and format) and basedpyright clean on `tools/count_games.py` and `tools/render_fixture_boards.py`; ShellCheck clean on `tools/fetch_assets.sh`; `check_dbc` ok |
| 9 | Docs | README, `third_party/DEPENDENCIES.md`, vision §5.6/§11, `CURRENT_STATE.md`, DBC files, this receipt |

Smoke (`--fen 'r1bqkbnr/1ppp1ppp/p1n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4' --move
Bxc6 --budget standard --json --no-llm`): `asked_move.positional` lists black
`doubled_pawns` on c (p c6, p c7) appearing at ply 2 with `holds_at_end` true, and white
`bishop_pair` disappearing at ply 2.

### §9 counts (not gating)

Boards of the 53,378 that carry each tag:

| Tag | Boards | Tag | Boards |
|---|---|---|---|
| `isolated_pawn` | 40,801 | `king_zone_attacker` | 46,733 |
| `doubled_pawns` | 13,949 | `bishop_pair` | 13,764 |
| `backward_pawn` | 13,304 | `rook_open_file` | 29,721 |
| `passed_pawn` | 27,803 | `rook_half_open_file` | 18,161 |
| `connected_passed_pawns` | 6,414 | `rook_seventh` | 6,295 |
| `pawn_islands` | 53,306 | `outpost` | 6,504 |
| `pawn_chain` | 12,189 | `bad_bishop` | 2,385 |
| `castling_kingside` | 5,286 | `mobility_lead` | 11,500 |
| `castling_queenside` | 5,090 | `space_lead` | 16,017 |
| `king_shield_gap` | 41,455 | `development_lead` | 1,208 |
| `open_file_near_king` | 26,702 | `material_ahead` | 42,516 |
| `half_open_file_near_king` | 29,824 | `exchange_up` | 7,662 |

Every tag fires on some board. `pawn_islands` fires on almost every board (53,306), as
expected: every board with a pawn has at least one island. [INFERENCE] `development_lead`
is rare (1,208) because puzzle boards come mostly from middlegames, where development is
complete.

### Volume along lines

Lines list every positional change, so long candidate lines carry many rows: F26 and F27
show 20–25 positional rows per line. This follows the contract (§5, Captain 2026-10-21:
every ply, every change). Shortening the printed list is a scope proposal for the
Captain, not part of M3a.

## Contract changes

§15: the smoke command's white `bishop_pair` disappears at ply 2, not ply 1 (the pair
goes with Black's recapture).

## Review

One completion review (`reviewer`, 2026-10-22) covering M3a and M3b together, scope M3a
§14, M3b §12 and the project principles. It found no M3a blockers in the detectors,
line changes, report v4 or the template. Its two blockers were in M3b (see that
receipt); the shared position-key fix reran every F fixture here as well. One recheck
found both resolved.
