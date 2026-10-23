# PostM3_EM2 receipt — critique fixes before prose

Date: 2026-10-23. Machine: `debian-home` (i7-8565U, 15 GiB, Debian 13). Branch
`postm3-em2`. Contract: `Planning/milestones/PostM3_EM2.md` (acceptance §14, contract
changes §15). Terms: `Planning/DBC/DEFINITIONS.md`.

## Delivered

- **Restricted and free moves** (§4.1, G6, K2): `pin_absolute`, `pin_relative`,
  `overloaded_defender` and `pin_tactical` carry `restriction`: the held piece, `held_to`
  (at square, along a file, rank or diagonal, or only to the listed squares), its
  restricted moves and its free moves, judged with the held side to move. Existence
  condition: no restricted move, no motif. Checkpoint 2 (Captain): a held piece is judged
  by its **move set**, which for a pawn adds a **hypothetical capture** onto each
  diagonal-front square, whatever stands there. At the gate: a move set never captures a
  king.
- **Contribution rule 3′** (§4.2, G5, K1): a motif is `indirect` iff it denies Y an answer
  to a later X move; `none` iff, without it, Y gains no answer. `back_rank_weakness` has
  denied moves for contribution only (K3).
- **`removal_of_defender` continuity** (§4.3, G4): the defended piece must still stand on
  its square when X captures there.
- **Conditions in the text** (§4.4, G2): null-move premise of threat lines, deferred
  parry wording, `creates_threat_skipped`, `walked_plies` ("facts read to ply N of M").
- **Report v5** (§5, K4): `position.legal_moves`, `depth`, `walked_plies`,
  `creates_threat_skipped`, `restriction`; the template prints the engine and budget
  header, the candidate count with the fine-set caveat (G8), `es` and `depth` per
  candidate (G9, G10), restriction rows and `SEE N` (G7).
- **Data digests** (§4.5, G11, K5): in-tree SHA-256; `games.table` and `opening.dataset`
  report the shipped ID when the loaded bytes match the pin, `custom@<12 hex>` otherwise.
- **Semantic checker** (§4.6, G3): `tools/validate_report.py` checks the relations the
  schema cannot express; it replaced the F harness's tactic invariant check.
- **Fixtures**: C17, C18; T57–T59; F31; T/C rows re-set at checkpoint 2 (FIXTURES.md).
- **Docs**: vision (§13 items, contract commit), README, `third_party/DEPENDENCIES.md`,
  DBC files, `CURRENT_STATE.md`.

## Verification

| # | Check | Result |
|---|---|---|
| 1 | Release and ASan/UBSan builds; CTests | Both warning-free. Release 155/155 (118 `unit` -j8; 37 `engine`/`games` serial). ASan/UBSan 155/155 (`unit` 2,501 s, `engine`/`games` 1,622 s) |
| 2 | §4.1 T fixtures and oracle | Every T fixture passes with its `restriction`; T47 and T59 included, T46 restricted g5-f4, g5-g4, g5-h4 at square g5. `tactics_oracle` (ASan): test 62,678 lists, 0 mismatching, 582 sampled puzzles, 24,732 `pin_tactical` instances; T fixtures 133 lists, 0 mismatching, 128 instances |
| 3 | §4.2 C fixtures | C01–C18 pass after checkpoint 2, including C17 `back_rank_weakness` indirect, C18 `back_rank_weakness` none and its Qg4 pin direct |
| 4 | §4.3 | TAC-5 near miss not labelled; C05–C09, C11, C15 keep their labels |
| 5 | §4.4, §5 | Every F report validates against `report.v5.schema.json` and passes the semantic checks; all 14 mutants fail (below) |
| 6 | §4.5 | FIPS vectors pass; shipped table and openings report the shipped IDs; a one-byte change reports `custom@…` |
| 7 | Puzzles and positional oracle | `tactics_puzzles` M2 targets met (below); `positional_oracle` 0 mismatches over 9,300 puzzles, 53,464 boards, 320,784 count values, P boards included |
| 8 | Identity and wall time | `cli_rerun_identity` passes; slowest `standard` F fixture F12 7.91 s (limit 12 s), Release, engine lock |
| 9 | Facts first | New fields carry provenance; golden render tests pass |
| 10 | Gates | clang-format and `clang-tidy --quiet -p build/release` clean on every changed C++ unit; Ruff and basedpyright clean on `tools/validate_report.py`; no shell script changed; `check_dbc` ok |
| 11 | Docs | §13 items, README, DEPENDENCIES, DBC, this receipt |

Smoke (`--fen '6k1/5ppp/8/8/8/1q6/5PPP/4R1K1 w - - 0 1' --budget standard --no-llm`):
engine and budget header; `Candidates: 5 of 20 legal moves …`; `Candidate 1: Re8# (e1e8),
fine, es=100.0%, loss=0.0%, depth=19`; `back_rank_weakness` rows on the threat and
refutation lines. JSON: `position.legal_moves` 20, `position.motifs[0]`
`back_rank_weakness` (white, ply 0) contribution `indirect`; the report passes the
checker. The text template does not print `position.motifs` (not in §10), so the ply-0
row with its `indirect` contribution appears in the JSON only.

### Semantic-checker mutants (§4.6)

Each mutant is one F report with one field changed (throwaway script under `build/`).
First failure line from `tools/validate_report.py`:

| # | Mutant | Failure |
|---|---|---|
| 1 | verdict | `/candidates/1/verdict: verdict must be 'mistake' for 400 loss units (thresholds 60/260/540)` |
| 2 | loss units | `/candidates/1/loss: loss must equal max(0, candidate[0] ES units - move ES units) = 400` |
| 3 | eval cp and mate | `/candidates/0/eval: eval must set exactly one of cp and mate` |
| 4 | mate_in | `/candidates/1/refutation: refutation must be set iff verdict is not fine` |
| 5 | refutation | `/candidates/1/refutation: refutation must be set iff verdict is not fine` |
| 6 | parry | `/candidates/0/parries_threat/test: searched requires the threat's first move to be legal after the assessed move` |
| 7 | urgency | schema: `/threats: {…} is not of type 'null'`; semantic: `/threats/urgency/more_urgent: more_urgent must be null iff urgency.rule is quiet` |
| 8 | terminal | `/candidates: a terminal position must have no candidate moves` |
| 9 | position FEN | `/position/side_to_move: side_to_move must be 'black' from position.fen` |
| 10 | SAN/UCI | `/candidates/0/san: SAN must name a legal move from this position` |
| 11 | asked-move copy | `/asked_move/depth: asked_move field 'depth' must equal candidates[0]` |
| 12 | `creates_threat_skipped` | `/candidates/1/creates_threat_skipped: creates_threat_skipped must be None from the board (check, game over, then no legal move after the opponent's null move)` |
| 13 | tactic | `/asked_move/tactic: asked_move field 'tactic' must equal candidates[0]` |
| 14 | restriction | `/position/motifs/0/restriction: restricted and free must partition the held piece's moves on By (missing ['e7e6'], unexpected [])` |

### Puzzle check (§7, not gating; test split)

| Tag | Recall | Direct share | Untagged firing rate |
|---|---|---|---|
| hangingPiece | 1.000 | 1.000 | 0.607 (3,531/5,821) |
| fork | 0.980 | 1.000 | 0.080 |
| pin | 0.997 | 0.512 | 0.430 (2,386/5,551) |
| skewer | 0.927 | 1.000 | 0.072 |
| discoveredAttack | 0.870 | 1.000 | 0.042 |
| discoveredCheck | 1.000 | 1.000 | 0.002 |
| doubleCheck | 1.000 | 1.000 | 0.000 |
| backRankMate | 1.000 | 1.000 | 0.007 |
| trappedPiece | 1.000 | 1.000 | 0.085 |
| promotion | 1.000 | 1.000 | 0.000 |

`removal_of_defender`: recall 0.740 (222/300); Lichess agreement on background 0.250
(56/224). Pin untagged firing rate: 44.3 % before the milestone, 31.0 % with the existence
condition over legal moves only (batch 1), 43.0 % with hypothetical captures (checkpoint
2). Pin direct share: 0.515 before, 0.495 after batch 1, 0.512 now.

## Contract changes

All in §15. Batch 1: `held_to` never null; the `pin_absolute` partition in the checker;
missing assets fail the digest-identity tests. Checkpoint 2: the move set with
hypothetical captures (T47 back to an include, T59 an include, C18 gains a direct pin);
C10's and C01's ply-1 pins drop (Black in check); digest-identity tests relabelled
`games`/`engine`. Gate: no king captures in a move set. The ASan oracle aborted on
puzzle UG08H after `g2f3 c6d4` (White in check from Nd4): with Black given the move,
`d4f3` took the king and SEE ran on a kingless board; Release had computed on it
silently. Production, oracle and checker now drop king captures; regression unit test
"held move restrictions never include capturing a king". Review: refutation judgement
required by the checker; en passant stands in for the hypothetical capture; the
`fine_truncated` wording (below).

## Review

One completion review (reviewer), one round, scope §14 and the Project stance.

- **Blocker (row 5, §4.6 item 13), fixed.** The semantic checker returned early when a
  refutation with plies had `tactic: null`, so F12 (`--move Ng4`) with
  `/candidates/2/refutation/tactic` set to null passed; the C++ check it replaced had
  rejected that. Root cause: the early return treated "no judgement" as allowed, but
  production sets one on every refutation with plies. The checker now fails it with
  `/candidates/2/refutation/tactic: a refutation with plies must have a tactic judgement`;
  all saved F reports still pass; `ctest -R report_schema` (33 tests with its fixture
  dependencies) passes. Recheck:
  resolved.
- **Scope proposal, recorded.** En passant and the hypothetical capture onto the same
  square share a UCI but differ on the board (`4Q3/8/2B5/3pP3/4r3/5k2/8/K7 w - d6 0 1`:
  e5-d6 is free as en passant, restricted without the en passant right). Production and
  oracle let the real move stand in; §4.1 and §15 now say so, DEFINITIONS likewise.
- **Nits.** `fine_truncated` wording now says "A returned all K lines and every one is
  fine" (schema, `report.hpp`, README), as computed since M1. `CURRENT_STATE.md`
  refreshed.

## Checkpoint 3

2026-10-23: the Captain ran the smoke command ("it worked") and called the milestone good
after two scoped additions (§15, last two rows), with no further review round:

- **T60** (UG08H after `g2f3 c6d4`): `overloaded_defender` Nd4 → e2, f5 at ply 2,
  restricted d4-b3, d4-b5, d4-c2, d4-c6, d4-e6, free none; d4-f3, the king capture, is in
  neither list. The T fixture test and `tactics_oracle` (Release) pass.
- **`tripled_pawns`**: three or more X pawns on one file; `doubled_pawns` unchanged. P01
  includes it, P12 excludes it for White's two c-pawns. `positional_oracle` (Release):
  53,464 boards, 9,307 lines, 0 mismatches; 127 puzzle boards carry the tag.

Gates for the additions: Release build warning-free, Release CTests (118 `unit`, 37
`engine`/`games` serial), saved F reports validate, clang-format clean, `check_dbc` ok.
ASan/UBSan and clang-tidy were not rerun for these mirror-of-`doubled_pawns` changes and
the new fixture (the Captain asked for a light cycle).

Deferred to M4 (Captain): the template does not print `position.motifs`, so the root
`back_rank_weakness` behind Re8# appears in the JSON only.
