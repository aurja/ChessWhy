# 01 — Motifs

Date: 2026-10-22. Baseline: `23bf1a52d856060556147f73c1bd5f90c67c8d5d` (report v4).
Concept nodes: `StateMotif`, `MoveMotif`, `TacticalPin`, `FirstAppearance`,
`StaticExchange`, `PositionMotifs`, `LineMotifs`.

## Conclusion

Keep the separation between presence and tactic (D11): a motif is a deterministic
pattern of one board, it never changes verdicts or urgency, and gain is judged by
tactics. The preliminary criticism that "presence" rules mix geometry with SEE and
removal is mostly a naming problem: that mixture is documented and deliberate. The
real weaknesses are about *which board* a predicate reads and *what the reader is told
it means*:

1. Hanging pieces at null-move roots lose their ply-0 witness (MOT-1).
2. `pin_tactical` proves a duty, but its wording claims the piece cannot move (MOT-2).
3. "gain" and "can win" overstate static-exchange numbers whose precision is no longer
   measured (MOT-3).
4. "First appearance" compares squares and turns, so unchanged facts are listed again
   (MOT-4).

A fork already on the board is unnamed by explicit design (MOT-5). Lichess recall and
the reference implementation establish recall and fidelity to the text, not
ordinary-chess precision.

## What the approach does

[SOURCE]

- **Two motif kinds.** State motifs are computed for both sides on one board
  (`state_motifs`, `append_pin_tactical`). Move motifs need the producing move and
  exist only at ply ≥ 1 (M2 §4.2).
- **Where they appear.** `position.motifs` holds ply 0 of the analysed board. Each line
  lists its move motifs, plus the state motifs that hold after ply k but not after
  ply k−1. "Ply 0 is not repeated in lines" (M2 §5; `line_motifs`, `same_identity`).
- **Static exchange and removal.** Eight of the fourteen tags read SEE. `pin_relative`,
  `overloaded_defender`, `skewer` and `pin_tactical` also delete a piece and then
  re-run SEE or move generation.
- **Turn dependence.** `hanging_piece` is evaluated only when X is to move.
- **Null-move roots.** The threat lines of searches B and E start from null-move boards
  (`make_threat`, `assess_move`, `analyse`).

## Findings

### MOT-1 — Null-move-root hanging pieces lose their ply-0 witness

**Label:** contract-level concern; observed.

**Root cause [SOURCE].** Three reasonable rules compose badly:

- `hanging_piece` needs X to move (M2 §4.1).
- `line_motifs` computes ply 0 only as a comparison baseline and never emits it (M2 §5).
- B and E lines start from a null-move board, which no other report section shows.

The hanging piece at ply 0 of such a line is the threat's content, so that witness is
dropped. `position.motifs` cannot show it either, because on the analysed board the
threatening side is not to move. It can reappear later only if it remains hanging when
that side next moves; it did not in the observed cases. "Ply 0 is not repeated" assumes
that ply 0 appears elsewhere, which is false for null-move roots. I found no statement
of this consequence in M2, PostM2_EM1, DEFINITIONS or CURRENT_STATE.

**Evidence.**

- [OBSERVED, parent-run] README run
  (`build/release/chesswhy --fen "r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 3 3" --budget standard --move Nf6 --json`).
  The `creates_threat` lines of Nf6 (`Nxh5`, material 9), g6 (`gxh5`, material 9) and d5
  (`dxc4`, material 3) contain no `hanging_piece`.
- [OBSERVED] The same board as g6's E root, analysed as a root:
  `build/release/chesswhy --fen "r1bqkbnr/pppp1p1p/2n3p1/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4" --budget quick --json`
  (R2). Its `position.motifs` lists `hanging_piece`, black, Qh5, gain 9. Same board,
  opposite result; only the placement differs.
- [OBSERVED] `build/release/chesswhy --fen "r5k1/5ppp/8/8/8/2n5/P4PPP/1R1Q2K1 w - - 0 1" --budget standard --json`
  (R1). `threats.black` is `Nxd1 Rxd1 …` with material 6. Its motifs start at ply 2, and
  no `hanging_piece` for Qd1 appears anywhere in the report.

**Impact [INFERENCE].** The commonest beginner explanation, "your queen is attacked and
undefended", has an engine consequence but no motif. M4 would have to infer the
mechanism.

**Strongest defense.** The consequence already states what is lost. The X-to-move form
gives recall 1.000 on puzzles where the solver is to move.

**[PROPOSED] Recommendation.** Amend M2 §5: a line whose root is a null-move board lists
the ply-0 state motifs that are absent from the corresponding visible board. That board
is `position.motifs` for B, and the move line's ply 1 for E. Detectors are unchanged.

**[PROPOSED] Acceptance.** The four cases above gain a ply-0 `hanging_piece` (Qh5 9,
Qh5 9, Bc4 3, Qd1 6). `position.motifs` and candidate lines stay unchanged, and
`tactics_oracle` stays at zero mismatches once the reference follows the text.

### MOT-2 — `pin_tactical` proves a duty, not an inability to move

**Label:** contract-level concern; observed.

**Root cause [SOURCE].** PostM2_EM1 §4.5 deletes p (`board_without`) and gives X the
move. The wording claims more. DEFINITIONS says p is "held in place by a threat", and
the renderer prints "restricted movement (tactic: …)". Deletion never examines p's legal
moves: p may keep its duty while moving, may have no move at all, or may capture the
threatening piece. Threat kinds are also compared tag by tag, so a move that already
mates on Bx can still "newly enable" a fork.

**Evidence.**

- [OBSERVED] In R2, `pin_tactical` reports: white, actor Qh5, targets g6 and e8, threat
  `checkmate` on f7. Yet …gxh5 is legal and is `candidates[0]`, and Black's threat line
  `gxh5 …` gains material 9. Five of the six `pin_tactical` rows on this board name Qh5
  as the actor.
- [OBSERVED, parent-run] At the README root, Rh8 is "restricted" by forks Qh5-f7 and
  Bc4-f7 and by the capture Qh5-h7. Boxed in by Ng8 and h7, the rook has no legal move.
  On the null-move board `Qxf7#` is already mate (B's line), so the "fork" adds nothing.
- [SOURCE] Fixture T47 requires pawn a4 to be reported as restricted by a skewer on a1,
  although its only move, a3, keeps the a-file blocked. T46, where g4 abandons f4, is the
  genuine case.

**Impact [INFERENCE].** A reader told that a piece's movement is restricted looks for a
pin, while the right move may be that very piece (gxh5).

**Strongest defense.** Deletion is cheap and deterministic and answers the "removal of
the defender" question. DEFINITIONS already marks the name as not a chess-literature
term. "Restricted" can be read as "some moves are bad".

**[PROPOSED] Recommendation.** Word the fact as a guard or block duty. Optionally add a
flag: generate p's moves with Y to move, as `trapped_piece` already does, and record
whether some move leaves m illegal or without its tag, or captures the actor. When m
already mates on Bx, treat weaker kinds as not newly enabled.

**[PROPOSED] Acceptance.** R2's g6 row, the README Rh8 rows and T47 are flagged; T46 is
not. Report the flagged share on the oracle's `pin_tactical` puzzle subset.

### MOT-3 — "Gain" claims what D11 disowned

**Label:** contract-level concern; observed and historical.

**Evidence and root cause [SOURCE].** M2 §4 says a motif "claims no gain", yet:

- `gain` holds the SEE value;
- DEFINITIONS §4.4 says X "can win at least 3 pawns";
- `write_motifs` prints ", gain N".

The material conditions entered in the WP6 calibration against engine-confirmed
precision (M2 §15, 2026-10-19/20). Checkpoint 2b removed that measure but kept the
conditions (M2 §8.1). Its last values were `hanging_piece` .700, `pin_relative` .270 and
`discovered_attack` .395 ([M2 receipt](../../receipts/M2.md), "History: engine-confirmed
precision"). The vision's risk table still names "precision targets" as the guard
against overfiring.

**[OBSERVED, parent-run] Example.** The README refutation of Nh6 prints
"hanging_piece: n h6, gain 3 (ply 4)". But White's Qh5 is attacked by g6, so Bxh6 would
be met by gxh5. The engine plays Qg5, and the line's consequence is material 0. SEE is
square-local: the number is right as SEE and wrong as gain.

**Strongest defense.** D11 left gain to contributions. Lichess agreement (.048) is not
precision either: an earlier measurement found 92% of `hanging_piece` "false positives"
were pieces the solver does capture (M2 §15, 2026-10-19).

**[PROPOSED] Recommendation.** Print the figure as a static-exchange value, and align the
plain-language gloss and the vision's risk row with D11. Optionally rerun the historical
confirmation method as a table of how often best play realises the motif. It would not
gate and would not be called motif precision.

**[PROPOSED] Acceptance.** Count the `hanging_piece` instances on `test.csv` background
rows where Y can win some X piece by SEE for at least the reported gain (the Nh6
pattern). The Nh6 row must no longer read as a win.

### MOT-4 — First appearance tracks squares and turns, not facts

**Label:** contract-level concern; observed.

**Root cause [SOURCE].** `line_motifs` compares only with ply k−1. Identity is tag,
side, squares and pieces, plus `threat`. A predicate behind a gate therefore vanishes for
one ply and is listed again. The gates are `hanging_piece`'s turn, the check gates of
`trapped_piece` and `pin_tactical`, and terminal boards. A slider moving along the same
ray also gets a new identity. The unit test covers only a persisting pin that does not
depend on the turn.

**Evidence [OBSERVED].** An engine-free dump with the existing `tactics_puzzle_eval`
(`pin_tactical` off; exit 1 comes from its recall gate on three rows, and the rows are
written normally). CSV rows (`PuzzleId,FEN,Moves,Themes,Stratum`):

```
A,7k/5ppp/8/3n4/8/8/5PPP/3R2K1 b - - 0 1,h8g8 h2h3 g8f8 d1d5,,background
B,r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3,d7d6 b5a4 g8f6 e1g1,,background
```

- Row A: `hanging_piece:white:0:d5>;back_rank_weakness:white:0:g8>;hanging_piece:white:2:d5>`.
  The untouched knight is listed twice; the turn-independent back-rank weakness is not.
- Row B: `pin_absolute:white:0:b5>c6,e8;pin_absolute:white:1:a4>c6,e8`.
- The README refutation of Qf6 likewise lists its relative pin again after Qh5-g5.

**Strongest defense.** The rule is followed literally, and both implementations agree.
Volume is approved; at issue is the meaning of "new", which M4 and contributions read as
an event.

**Scope of the converse error [INFERENCE].** It is narrow. One move cannot put a
different piece of the same letter onto an unchanged identity. A changed `gain` or a
changed attacker set, however, is not listed again.

**[PROPOSED] Recommendation.** Compare a gated predicate with the last ply where it was
evaluable, and try identity that tracks pieces as an experiment.

**[PROPOSED] Acceptance.** Row A lists d5 once; measure the change in the number of
re-listings on `test.csv`.

### MOT-5 — A fork already on the board is unnamed

**Label:** accepted limitation; scope proposal.

**[SOURCE]** Move motifs exist only at ply ≥ 1 (M2 §4.2). This is explicit design, not
detector noncompliance.

**[OBSERVED]** In R1, Nc3 already attacks Qd1 and Rb1, yet no black Nc3 `fork` is
reported; the only reported `fork` is White's reply Qf3 (targets nc3 and ra8).
The engine-free row
`C,r5k1/5ppp/8/3n4/8/8/P4PPP/1R1Q2K1 b - - 0 1,d5c3 d1d4 c3b1,fork,fork` gives
`hanging_piece:black:1:b1>;back_rank_weakness:black:2:g1>`.

This finding is not one of the open M2 recall-miss proposals: forks by a capturable
piece, skewers with a pawn behind, discovered attacks on pawns or defended equal pieces
([CURRENT_STATE](../../CURRENT_STATE.md)). Those concern narrow definitions; this one
concerns timing.

**[PROPOSED] Recommendation.** When PGN input supplies the last move, record its move
motifs as ply-0 context.

**[PROPOSED] Experiment.** On `test.csv`, count the puzzles where
`line_motifs(P0, [Moves[0]])` gives the opponent a fork, skewer or discovered attack.

## Strengths to retain

- Motifs never move verdicts or urgency (D7/D11).
- The definitions are written out, and an independent reference implementation found
  real bugs: 74 mismatches fell to 0 after fixing tied SEE attackers and trapped-piece
  pseudo-captures.
- Near-miss fixtures are careful: T39, T40, T43–T46, T55, T56.
- SEE handles x-rays, pinned capturers and safe king recaptures.
- `hanging_piece` handles check and trades in progress.
- The recall audit classed 22 of 27 misses as definitions narrower than the chess term.
- R1 correctly detects White's Qf3 fork.

## Verification receipt and limits

- **Run by me, serially:** R1 (standard) and R2 (quick), both exit 0 with no stderr on
  Stockfish 19, plus the engine-free dump above.
- **Run by the parent:** the README JSON and text at `standard`.
- **Not run:** test suites, the oracle and every [PROPOSED] experiment.
- **Unmeasured:** how often each issue occurs. The historical precision figures predate
  later fixes.
- **Elsewhere:** contribution causality is covered in [02 — Tactics](02-tactics.md),
  null-move wording in [04 — Threats](04-threats.md).

## Sources

- [`src/tactics/motifs.cpp`](../../../src/tactics/motifs.cpp): `state_motifs`,
  `same_identity`, `append_fork`, `append_pin_tactical`, `board_without`, `line_motifs`.
- [`src/analysis/analyse.cpp`](../../../src/analysis/analyse.cpp): `make_threat`,
  `assess_move`, `analyse`.
- [`src/render/render.cpp`](../../../src/render/render.cpp): `write_motif_label`,
  `write_motifs`.
- Tests: [`tests/tactics/oracle/reference.cpp`](../../../tests/tactics/oracle/reference.cpp)
  (same turn gate and deletion),
  [`tests/tactics/tactics_unit_tests.cpp`](../../../tests/tactics/tactics_unit_tests.cpp),
  [`tests/tactics/puzzle_eval.cpp`](../../../tests/tactics/puzzle_eval.cpp).
- Milestones: [M2](../../milestones/M2.md) §4–§5, §7.3–§8.3, §15;
  [PostM2_EM1](../../milestones/PostM2_EM1.md) §4.5–§4.6.
- DBC: [DEFINITIONS](../../DBC/DEFINITIONS.md) §4.1, §4.4;
  [FIXTURES](../../DBC/FIXTURES.md) T39–T56.
- Receipts and planning: [M2 receipt](../../receipts/M2.md);
  [PROJECT_VISION](../../PROJECT_VISION.md) §5.5 and risks;
  [CURRENT_STATE](../../CURRENT_STATE.md).
