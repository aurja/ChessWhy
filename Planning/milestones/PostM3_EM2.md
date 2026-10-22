# PostM3_EM2 — Critique fixes before prose

Status: **frozen** on branch `postm3-em2` (2026-10-22), except the items the Captain
approves at the checkpoints in §12: the Deputy's designs marked **(D)** (checkpoint 1,
before any worker starts) and every fixture mismatch the rule changes cause
(checkpoint 2). Changes to anything else are contract changes: the orchestrator makes
them, records them in §15 and tells the Captain.

Intent: fix what the conceptual audit (`Planning/critiques/`, 2026-10-22) found before
M4 puts the report into words. The report must state the conditions its facts depend
on, a "none" contribution must mean the tactic works without the motif, restricted
movement must say which moves are restricted, the report's relations must be checked
by a machine, and data and budget identity must be visible. Acceptance intent: every
fixed finding has a fixture, a test or a doc row that would fail on the old behaviour.

## 1. Captain's decisions

Briefed 2026-10-22 as decisions 1–15 after the audit. The Captain approved every
recommendation except 5 and 10, which she changed (G5, G6).

| # | Topic | Decision |
|---|---|---|
| G1 | M4 fact check (EXP-1, EXP-5, PRF-1) | M4 prose is limited to **claim units** built from report fields; the M4 fact check splits prose into claim units and checks each one, relations included, against a seeded relational test set (audit cases X1–X13 as seeds). Doc change now (vision §7.3, §7.4, §11 M4 row); built in M4 |
| G2 | Conditions in the template (THR-1–3, REP-1, REP-5, EXP-2, MOT-1) | The text states the conditions a fact depends on: the threat line's null-move premise, a parry deferred by check, why `creates_threat` is null, how many plies the facts were read from. New fields `creates_threat_skipped`, `walked_plies` (§5). The parry boolean and its score test stay; "parries" is documented as "the threat no longer wins against the pass baseline", not "the threat was prevented". MOT-1: doc only (a threat line with a null root has no ply-0 motif) |
| G3 | Semantic checker (REP-3) | `tools/validate_report.py` checks relations between fields, not only shape (§4.6); schema v5 adds `if/then` rules for local relations. It absorbs the F harness's tactic invariant check (clean cutover) |
| G4 | `removal_of_defender` continuity (TAC-5) | The defended piece must still stand on its square when X captures there (§4.3) |
| G5 | Contribution (Captain: "None should mean that the tactic works without it") | Rule 3 is replaced (§4.2): μ is `indirect` iff it denies Y an answer to a later X move; `none` iff without μ, Y gains no answer |
| G6 | Restricted movement (Captain: "Restricted doesn't mean can't move at all. It means can't move in all the ways it otherwise could") | The report names, per restricted-movement motif, the restricted piece, its **restricted moves** and its **free moves** (§4.1, §5, §10) |
| G7 | Glosses and `SEE` (part of #10) | `exchange_up` and `outpost` glosses state their exact rules (§13); the text prints `SEE N` instead of `gain N`; JSON `gain` unchanged, described as an SEE value |
| G8 | Fine set (VER-1) | No asked move: ChessWhy judges the engine's top candidates only; unlisted moves are not judged and may also be fine. New `position.legal_moves`. Vision §3 narrowed |
| G9 | Candidate order (VER-2) | Candidates stay in engine (MultiPV) order; each line prints its search depth (new `depth`) |
| G10 | Budget sensitivity | The text prints the engine and budget; vision §7 gets tie-aware wording for M4. No stability table |
| G11 | Data identity (REF-5, REP-4) | `games.table` and `opening.dataset` are computed from the loaded bytes (SHA-256): the shipped ID when the digest matches the pin, `custom@<12 hex>` otherwise (§4.5). `reproducible` means the configuration (budget, threads, hash, nodes), not the engine binary or ChessWhy revision |
| G12 | Fact selection for M4 | Vision §7.2: mandatory core claims at every level, root features only when linked to a line or verdict, "holds at the end of the analysed walk", tag-aware role phrasing, an empty refutation motif list is not "no mechanism", tactic and blunder evidence first |
| G13 | Evidence (PRF, POS) | Recall is "coverage of Lichess-tagged puzzles"; untagged firing rates published; `positional_oracle` also runs the P fixture boards; book support recorded per book P row |
| G14 | Book use in M5 (REF-4) | M5 acceptance adds on-topic, applicability and stance checks |
| G15 | Position only (SRCH-3) and test load | Analysis is position-only (history is not an input; documented). Engine CTests get `RESOURCE_LOCK engine` |

Accepted with no action: MOT-4 (doc fix only, §13), MOT-5, TAC-2 (M4 note), TAC-3,
TAC-4, THR-5, POS-4, PRF-3, SRCH-2, SRCH-5.

## 2. Contract files

Created by the orchestrator in the contract commit, after checkpoint 1 and before
batch 1 (the headers depend on the (D) designs):

| File | Contents |
|---|---|
| `include/chesswhy/report.hpp` | `MotifRestriction`; `Motif::restriction`; `CreatesThreatSkip`; `MoveAssessment::{depth, walked_plies, creates_threat_skipped}`; `PositionInfo::legal_moves`; `kSchemaVersion = 5` (§6) |
| `include/chesswhy/tactics.hpp` | `denied_moves`, `answers` (§6) |
| `include/chesswhy/digest.hpp` | `sha256_hex` (§6) |
| `schema/report.v5.schema.json` | replaces `report.v4.schema.json` (deleted; git history keeps it) |
| `src/report/report.cpp` | `to_string(CreatesThreatSkip)` only, so WP6 does not wait for WP4 |

## 3. Scope

In: G2–G11, G13, G15 code and fixtures; G1, G12, G14 and every doc fix in §13.

Searches and node budgets are unchanged. Motif recognition changes only by the
existence condition (§4.1) and `removal_of_defender` only by continuity (§4.3).
Engine fixtures keep every value that does not depend on those two rules.

## 4. Rules

Notation as in PostM2_EM1 §4: X = the tactic side (or the motif's side), Y = the other
side; P₀ … Pₙ the boards of a line, mvⱼ the move from Pⱼ₋₁ to Pⱼ. A "move" below is a
legal move unless it says pseudo-legal; moves that capture a king are ignored (the
`trapped_piece` convention).

### 4.1 Restricted and free moves (G6)

For the four restricted-movement tags, the **held piece** h and its **denied moves** on
a board B with Y to move:

| Tag | h | Denied moves of h on B |
|---|---|---|
| `pin_absolute` | targets[0] (p) | pseudo-legal moves of p whose destination is off the segment from the actor's square to the king's square (actor's square included) |
| `pin_relative` | targets[0] (p) | moves of p whose destination is off the segment from the actor's square to q's square (actor's square included), after which `see(q's square, X) ≥ 1` |
| `overloaded_defender` | the actor (d) | moves of d after which some target of μ, still on its square, has `see(t, X) ≥ 1` |
| `pin_tactical` | targets[0] (p) | moves of p after which μ's threat is enabled: X's move a→b is legal and produces the threat kind (move-motif kinds: an M2 §4.2 motif of that tag for X; `checkmate`: it mates); `capture`: `see(b, X) ≥ 1` and X has a legal capture on b |
| `back_rank_weakness` | the Y king | king moves to a front or diagonal-front square holding a Y piece, legal on B with that piece removed (contribution only, §4.2; no `restriction` field) |

`hanging_piece` and `trapped_piece` deny nothing.

**Restriction** of a motif μ found on board P (tags `pin_absolute`, `pin_relative`,
`overloaded_defender`, `pin_tactical`): let By = P if Y is to move, else P after a null
move. Then

- `piece` = h, as on P;
- `restricted` = h's denied moves on By;
- `free` = h's legal moves on By that are not restricted.

Both lists in UCI, sorted ascending as strings. For `pin_absolute` the restricted moves
are the moves the pin makes illegal; for the others they are legal moves that cost
something.

**Existence condition (D):** a motif of these four tags exists only if `restricted` is
non-empty. A piece that cannot move at all, or whose every move stays allowed, is not
restricted by the motif. Effects known today:

- README root (`r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 3 3`): the
  `pin_tactical` motifs whose p is Rh8 (no legal moves) are dropped.
- T47 (`q7/8/k7/8/p7/8/7K/1R6 w - - 0 1`): its include turns into an exclude: a4's only
  move, a3, keeps the a-file blocked.
- T46 (`8/8/6k1/3N2p1/8/8/4q3/K7 b - - 0 1`): stays; restricted `[g5g4]`, free `[]`.
- C fixture pin rows (python-chess scan, 2026-10-22). Pins whose held piece has no
  restricted move are no longer reported, so their rows go at checkpoint 2:
  `pin_absolute` C05 ply 0 (none), C09 ply 0 (none), C10 ply 0 (**direct** today:
  Rxh7+ captures the h7 pawn, but the pin held nothing), C12 plies 2, 4, 6 (none);
  `pin_relative` C01 ply 1 (none; the a7 pawn's pushes stay on the a-file).
- C12 and C14 ply 0: the pinned pawn's only capture square holds its own queen, so it
  has no restricted move and the pin is not reported at ply 0. It first holds at ply 1,
  after Bxd6, where `e7d6` (`c7d6`) is restricted; the C rows move from ply 0 to ply 1.
  Rule 3′ therefore counts the X move that made the motif's board (§4.2).
- Lichess `pin` recall may shift; `tactics_puzzles` must still meet the M2 targets
  (otherwise checkpoint 2).

Alternative for the Captain at checkpoint 1: apply the existence condition to
`pin_tactical` only, so the M2 pins keep their M2 definitions.

### 4.2 Contribution rule 3′ (G5) (D)

Replaces PostM2_EM1 §4.3 rule 3. Rules 1, 2 and 4 are unchanged.

3′. μ is **indirect** iff it is not direct, and there is an X move mvⱼ with k ≤ j ≤ n
(j ≥ 1; j = k only when mvₖ is an X move, the one that made μ's board), not castling,
such that no move mvₖ₊₁ … mvⱼ has moved or captured any piece of μ, and one of μ's
denied moves (§4.1) on Pⱼ **answers** mvⱼ:

- Y in check on Pⱼ: after the denied move (for `back_rank_weakness`: on Pⱼ with the
  piece removed, then the king move), the Y king is attacked by no X piece. For
  `pin_absolute`, the actor's attack through the held piece's vacated square is ignored
  (it is the attack the pin itself creates); every other attack counts, the actor's own
  check included (C10's ply-1 pin by the checking rook Rh7 stays none). This covers
  king escapes, captures of the checker and interpositions.
- Otherwise: the denied move captures the X piece on mvⱼ's destination.

Plain meaning (for hand labels): indirect = the motif takes away an answer Y would have
to a tactic move (a recapture, an escape, an interposition, a capture of the checker);
none = without the motif, Y gains no answer. Indirect does not claim the tactic fails
without the motif.

Expected effects (checked at checkpoint 2):

| Board | Motif | Before | After |
|---|---|---|---|
| Re8# `6k1/5ppp/8/8/8/1q6/5PPP/4R1K1 w - - 0 1` | `back_rank_weakness` (ply 0) | none | indirect (Kf7, Kg7, Kh7 would escape with the blocker gone) |
| Qxg7# `6k1/5ppp/3rr3/q7/6Q1/8/1B3PPP/6K1 w - - 0 1` | `back_rank_weakness` (ply 0) | indirect | none (f7 and h7 stay attacked by the queen) |
| C13 | the ply-0 pin | indirect | indirect (Nxf5 is restricted and would recapture) |
| C12, C14 | the pawn pins | indirect at ply 0 | indirect at ply 1 (the pin first holds after Bxd6; exd6 / cxd6 is restricted and would recapture the move that made the board) |
| C01 | `pin_relative` Bg5 → f6, d8 (ply 0) | none | indirect expected: Nf6-d7 would block Qa4+, but the pin denies it (Bxd8 would then win the queen) |

### 4.3 `removal_of_defender` continuity (G4)

PostM2_EM1 §4.4 gains one condition: no move mvₖ₊₁ … mvⱼ₋₁ starts or ends on t's
square, so the original t still stands there when mvⱼ captures. Near miss (TAC-5):
`7k/8/3b4/q1P1n3/8/8/7N/4R2K w - - 0 1`, line `c5d6 e5g6 h2g4 a5e5 g4e5`; today it is
labelled at ply 1 (actor d6, targets d6, e5); after the fix it is not (the knight left
e5 and a queen arrived). This board is not a tactic (Black is winning), so it becomes a
case of the unit test `removal-of-defender labels reject near misses`, not a C fixture.

### 4.4 Parry, threat and walk conditions (G2)

- `creates_threat_skipped` (rule): `gives_check` | `game_over` (the move ends the game)
  | `no_legal_move` (no legal move after the opponent's null move). Null iff
  `creates_threat` is set; exactly one reason otherwise, checked in that order.
- `walked_plies` (rule): the plies of `line_san` the motif and positional facts were
  read from (`rules.cpp` `walked_plies`); `motifs` and `positional` cover plies
  1..`walked_plies`.
- `parries_threat` with test `deferred_by_check`: unchanged in JSON; the text names the
  reason (§10).

### 4.5 Data digests (G11)

- SHA-256 over the loaded bytes, in-tree implementation (`src/util/sha256.cpp`), tested
  against the FIPS 180-4 vectors ("", "abc", the 448-bit message, one million "a").
- Games table: digest of the table file. Equal to the pinned
  `e0cb7fab32128854d72b158178225ae586c2567a6b79bdfb4bc504390038d7a7` → `games::kTable`;
  otherwise `custom@<first 12 hex>`.
- Openings: each of `a.tsv` … `e.tsv` equal to its pin in `tools/fetch_assets.sh`
  (`OPENINGS_SHA256`) → `openings::kDataset`; otherwise `custom@<first 12 hex of the
  SHA-256 over the five files' bytes concatenated in a..e order>`.
- The pins move into the headers as constants; `fetch_assets.sh` keeps its own copy and
  a unit test compares the two.

### 4.6 Semantic checks (G3)

`tools/validate_report.py SCHEMA FILE...` validates the schema, then checks each report
with python-chess. A failure prints the JSON pointer and the broken relation. Checks:

1. `verdict` follows `loss_units` and the frozen thresholds (60 / 260 / 540).
2. `loss_units` = max(0, es_units(candidates[0]) − es_units(move)).
3. `eval`: exactly one of `cp`, `mate` is set (schema `if/then`).
4. `consequence.mate_in` set iff `cls` is `mate` (schema `if/then`).
5. `refutation` set iff `verdict` ≠ `fine`.
6. `parries_threat`: source `engine` iff test `searched`; `threat_move_illegal` iff the
   threat's first move is illegal after the move.
7. `urgency.more_urgent` null iff rule `quiet`.
8. `position.terminal` set ⇒ no candidates, no asked-move refutation search fields.
9. FEN side to move = `side_to_move`; `in_check` = the board is in check;
   `legal_moves` = the board's legal move count.
10. Every `san`/`uci` pair and every `line_san` is legal from its root.
11. An asked move that is a candidate equals that candidate field by field.
12. `creates_threat_skipped` follows from the board (check, game over, no legal move).
13. The tactic invariants of PostM2_EM1 §9 (moved here from `engine_fixture_test.cpp`).
14. `restriction` set iff the tag is one of the four; `restricted` non-empty;
    `restricted` and `free` disjoint and their union is h's moves on By.

Acceptance: 14 hand-made mutants of F reports (one per check, made by a throwaway
script) each fail with the right pointer; every F report passes.

### 4.7 Not in scope

Engine binary SHA and ChessWhy revision in the report; a budget stability table;
re-sorting candidates; new motif tags; prose (M4); the recall-miss scope proposals.

## 5. Report v5

- `schema_version` 5; `schema/report.v5.schema.json`; every field gets a
  `description`.
- `position.legal_moves` (int ≥ 0, rule).
- `MoveAssessment.depth` (int ≥ 1, engine): depth of the search line that gave `eval`
  (the MultiPV line for candidates, search C for an asked move outside A).
- `MoveAssessment.walked_plies` (int ≥ 0, rule).
- `MoveAssessment.creates_threat_skipped` (nullable enum, rule) per §4.4.
- `Motif.restriction` (nullable, after `threat`): `{"piece": {"square": "e5", "piece":
  "n"}, "restricted": ["e5c6", "e5d7"], "free": []}`, set iff the tag is one of the four
  restricted-movement tags. Part of no identity.
- `games.by_source[].table` and `opening.dataset` computed per §4.5.
- Descriptions that change meaning: `fine_truncated` = "true iff every candidate is
  fine; false does not mean the unlisted moves are not fine"; `gain` = "static exchange
  (SEE) value in pawn units"; `parries_threat.parries` = "the opponent's threat no
  longer wins against the pass baseline"; `reproducible` = "same configuration (budget,
  threads, hash, nodes) gives the same report".

## 6. Headers

```cpp
// report.hpp
struct MotifRestriction {                         // PostM3_EM2 §4.1; source rule
    PieceOnSquare piece;                          // the held piece h, as on P
    std::vector<std::string> restricted;          // UCI, ascending; non-empty
    std::vector<std::string> free;                // UCI, ascending
};
// Motif:          std::optional<MotifRestriction> restriction; // set iff one of the four tags
enum class CreatesThreatSkip : std::uint8_t { GivesCheck, GameOver, NoLegalMove };
[[nodiscard]] std::string to_string(CreatesThreatSkip skip); // "gives_check", ...
// MoveAssessment: int depth = 0; int walked_plies = 0;
//                 std::optional<CreatesThreatSkip> creates_threat_skipped; // null iff creates_threat
// PositionInfo:   int legal_moves = 0;
// kSchemaVersion = 5

// tactics.hpp (pure, provenance rule)
// §4.1: h's denied moves on `board` (Y to move); empty for tags that deny nothing.
[[nodiscard]] std::vector<chess::Move> denied_moves(const chess::Board& board,
                                                    const Motif& motif);
// §4.2: whether `reply` (a denied move on `board`, Y to move) answers the X move that
// produced `board`, whose destination is `landing`.
[[nodiscard]] bool answers(const chess::Board& board, const Motif& motif,
                           const chess::Move& reply, std::uint8_t landing);

// digest.hpp
[[nodiscard]] std::string sha256_hex(std::span<const std::byte> bytes);
// games.hpp:    kTableSha256 (full hex); openings.hpp: kDatasetSha256[5] (a..e)
```

The contract commit's headers are authoritative; the block above is a summary.

## 7. Puzzle check (not gating)

`tactics_puzzle_eval` re-records the PostM2_EM1 §7 numbers (direct share per theme,
`removal_of_defender` recall and agreement) and adds per tag: untagged firing rate
(share of puzzles without the theme where the tag fires; today `hanging_piece` 3,531 /
5,821 = 60.7 %, `pin_absolute`/`pin_relative` for `pin` 2,459 / 5,551 = 44.3 %). Printed in
the receipt; never gating.

## 8. Calibration and targets

None. M2 recall targets stay; a miss caused by the existence condition goes to
checkpoint 2.

## 9. Fixtures

- **C fixtures** (`tests/fixtures/tactics.json`): new C17 (Re8#, line `e1e8`), C18
  (Qxg7#, line `g4g7`), each listing every X-side motif with its contribution, the
  `back_rank_weakness` rows as in §4.2. Hand labels by WP5 from the §4.2 plain meaning,
  one-line reason each, without reading WP1's code; margin measured at `standard`.
  Existing C rows change only at checkpoint 2.
- **T fixtures** (`tests/fixtures/motifs.json`): includes and excludes take an optional
  `restriction` `{restricted, free}` (matched when given). T46 gains
  `{restricted: [g5g4], free: []}`; T47's include moves to its excludes; new T57 (README
  root) excludes every `pin_tactical` with p = h8; new T58, a `pin_relative` with both
  lists non-empty; new T59, a pawn pinned on its file with no capture (excluded).
- **F fixtures**: every existing expectation that does not depend on §4.1–4.3 stays.
  New `equals` rows: `/position/legal_moves` on F01; `/candidates/0/depth` presence via
  the semantic checker only (depth varies by budget); `creates_threat_skipped` on a
  checking candidate; `text_contains` rows for the §10 lines.
- **Mutants**: 14 per §4.6, generated by a throwaway script under `build/`; the
  receipt records each check's failure line.
- **P boards** (G13): `positional_oracle` also compares every board of
  `tests/fixtures/positional.json` (features and lines). The 10 book P rows (P01–P03,
  P05, P08–P10, P58, P59, P70) get a yes/no book-support note per label in their
  FIXTURES Comments; no excerpts, no book names.

## 10. Template

| Item | Before | After |
|---|---|---|
| Header | none | `Engine: Stockfish 19 (Threads=1, Hash=64 MB) [rule]`, then `Budget: quick, nodes A=…, B=…, C=…, D=…, E=…, R=… [rule]` |
| Candidate count | none | `Candidates: 5 of 20 legal moves (engine's top 5); unlisted moves are not judged and may also be fine [rule]` |
| Candidate row | `Candidate 1: g6 (g7g6), fine, loss=0.0% [engine]` | `Candidate 1: g6 (g7g6), fine, es=48.0%, loss=0.0%, depth=16 [engine]` |
| Created threat | `Candidate 1 creates threat line: …` | `Candidate 1 creates threat line (if White passes after g6): …`; when null: `Candidate 1 creates threat: not searched (gives check) [rule]` |
| Deferred parry | `… parries threat: false (deferred_by_check) [rule]` | `… parries threat: false (deferred_by_check: the move checks, so the threat only waits) [rule]` |
| Walk | none | `Candidate 1 facts read to ply 6 of 10 [rule]` |
| Restricted movement | `restricted movement (absolute pin): R e1 → n e5, k e8 (ply 0) [rule]` | `restricted movement (absolute pin): R e1 → n e5, k e8; restricted: Ne5-c6, Ne5-d7; free: none (ply 0) [rule]` |
| Gain | `hanging_piece: n h6, gain 3 (ply 2) [rule]` | `hanging_piece: n h6, SEE 3 (ply 2) [rule]` |

Moves in restriction lists: piece letter in upper case (none for a pawn), from square,
`-`, to square, `=` and the promotion letter. Wording pinned by golden tests.

## 11. Work packages

Batch 1 after checkpoint 1: WP1–WP8. Milestone end: WP9.

| WP | Owned files | Done when | Non-goals |
|---|---|---|---|
| WP1 tactic rules | `src/tactics/tactic.cpp`, `tests/tactics/tactics_unit_tests.cpp` | `denied_moves`, `answers`, rule 3′ and §4.3 continuity; unit tests at each boundary (check vs no check, the pin-line attack ignored but the actor's own check counted, j = k for the move that made the board, back-rank removal, moved motif piece); TAC-5 near miss; C harness reads C17, C18 | detectors |
| WP2 restriction | `src/tactics/motifs.cpp`, `src/tactics/pin_tactical.cpp`, `tests/tactics/tactics_unit_tests.cpp` (restriction cases only; coordinate with WP1 via message) | `restriction` on the four tags per §4.1 (calls `denied_moves`); existence condition; T harness matches `restriction` | contribution |
| WP3 reference | `tests/tactics/oracle/*`, `tests/tactics/motif_oracle.cpp`, `tests/tactics/puzzle_eval.cpp` | independent §4.1 restriction and existence condition written from this plan without reading WP1/WP2; oracle compares `restriction`; §7 numbers printed; zero mismatches | detectors |
| WP4 analysis + report | `src/analysis/*`, `src/engine/info_line.cpp`, `src/report/*` (except the contract's `to_string`), `tests/analysis/*`, `tests/report/*` | `legal_moves`, `depth`, `walked_plies`, `creates_threat_skipped`, `restriction` in JSON; examples validate against v5 | rules, render |
| WP5 fixtures | `tests/fixtures/tactics.json`, `tests/fixtures/motifs.json`, `tests/fixtures/positions.json` (proposals) | §9 rows with reasons; then checkpoint 2 | code |
| WP6 render | `src/render/*`, `tests/render/*` and goldens | §10 lines in golden tests | analysis |
| WP7 semantic checker + CTests | `tools/validate_report.py`, `tests/cli/engine_fixture_test.cpp`, `tests/cli/CMakeLists.txt`, `tests/report/CMakeLists.txt` | §4.6 checks; invariant check removed from the C++ harness; `report_schema*` use v5; `RESOURCE_LOCK engine` on `cli_fixture_*` and `cli_rerun_identity`; 14 mutants fail | report code |
| WP8 digests + P boards | `src/util/sha256.cpp`, `src/util/CMakeLists.txt`, `src/games/games.cpp`, `src/openings/openings.cpp`, their tests, `tests/positional/oracle/positional_oracle.cpp` | §4.5 with FIPS vectors and pin-copy test; custom files report `custom@…`; P boards in the oracle, zero mismatches | template |
| WP9 docs | `README.md`, `third_party/DEPENDENCIES.md` | §13 | — |

## 12. Captain's checkpoints

1. Before batch 1: approve the (D) designs: rule 3′ including the interposition effect
   on C01 and the move that made the motif's board counting (C12, C14 ply 1); the
   existence condition for all four tags (C05, C09, C10, C12 and C01 rows drop, §4.1)
   or `pin_tactical` only;
   `back_rank_weakness` without a `restriction` field; the new fields (`legal_moves`,
   `depth`, `walked_plies`, `creates_threat_skipped`, `restriction`); the
   `custom@<12 hex>` ID format and the in-tree SHA-256.
2. After batch 1: every C, T or F fixture whose result changed, and any recall target
   missed, with fixture, motif, both values and the reason. The Captain decides per
   mismatch (rule or label; §15 row). The same invariant failing twice → design note.
3. Milestone end: try the smoke command and approve merging `postm3-em2`.

## 13. Credits and documentation

- Vision (in the contract commit): §2 provenance of verdicts (frozen thresholds over
  engine scores); §3 fine set narrowed, position only; §5.2 item 4 residual class; §5.4
  threat after the opponent's null move; §5.5 restricted movement, contribution meaning,
  `removal_of_defender` continuity; §7.2 fact selection; §7.3, §7.4 claim units and the
  relational test set; §8 M5 checks; §11 PostM3_EM2 row, M4 and M5 acceptance.
- DEFINITIONS (contract commit): new terms, the drift rows found by the audit
  (§9 lead thresholds, report version, `creates_threat` null causes, §8 built items,
  first appearance), glosses for `exchange_up` ("X has more rooks than Y and Y has more
  knights plus bishops than X"; fires while X is behind in material on 17.9 % of boards)
  and `outpost` ("no Y pawn on an adjacent file in front of s, on today's board only";
  a capture can change files, `7k/2p5/3B4/4N3/3P4/8/8/K7 w - - 0 1`).
- `report.hpp` comment on `parries_threat`: null also when the side to move is in check.
- M3a §9 "never a book" means a P fixture's `source` names the game, never the book it
  was transcribed from (the ten book P rows do so); no change, noted in FIXTURES §8.
- `README.md`: restricted and free moves, the semantic checker, data digests, fine set
  scope; `third_party/DEPENDENCIES.md`: SHA-256 is in-tree (no new dependency).
- `Planning/CURRENT_STATE.md`, receipt `Planning/receipts/PostM3_EM2.md`, DBC pass.

## 14. Acceptance

1. Release and ASan/UBSan builds warning-free; all `unit`, `engine` and `games` CTests
   pass in both trees.
2. §4.1: every T fixture passes with its `restriction`; T47 excluded, T46 restricted
   `[g5g4]`; `tactics_oracle` zero mismatches including `restriction`.
3. §4.2: C fixtures pass after checkpoint 2, including C17 indirect and C18 none.
4. §4.3: the TAC-5 near miss is not labelled; C05–C09, C11, C15 keep their labels.
5. §4.4, §5: every F report validates against `report.v5.schema.json` and passes the
   §4.6 semantic checks; the 14 mutants fail.
6. §4.5: FIPS vectors pass; the shipped table and openings report the shipped IDs; a
   one-byte change reports `custom@…`.
7. `tactics_puzzles` M2 targets met; §7 numbers in the receipt; `positional_oracle`
   zero mismatches including P boards.
8. Rerun identity byte-identical; `standard` ≤ 12 s per F fixture under the engine lock.
9. Facts first: every new field carries its provenance; the template prints only report
   fields (golden tests).
10. Gates: clang-format and clang-tidy on changed C++; Ruff and basedpyright on changed
    Python; ShellCheck on changed scripts; `python3 tools/check_dbc.py`.
11. Docs (§13) updated.

Smoke command: `build/release/chesswhy --fen '6k1/5ppp/8/8/8/1q6/5PPP/4R1K1 w - - 0 1'
--budget standard --no-llm`. The text shows the engine and budget header, the candidate
count line, `Re8#` with `es`, `depth`, and the `back_rank_weakness` row; the JSON shows
`back_rank_weakness` with contribution `indirect` and `position.legal_moves`.

## 15. Contract changes

| Date | Change | Reason |
|---|---|---|
| 2026-10-22 | Plan frozen (G1–G15) | Captain's approval of the audit decisions, with 5 and 10 changed |
