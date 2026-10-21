# ChessWhy — Definitions

Project-specific meanings of the words used in the plans, receipts and agent reports.
Where a word has an ordinary meaning, an ordinary chess meaning and a project meaning,
this file gives the project meaning. The plans stay authoritative; the last column
points to the section that defines or decides each term.

One of the five DBC files (§7): the concepts that use these terms are drawn in
[CONCEPTS.md](CONCEPTS.md) and placed in the source by [CODE.md](CODE.md); fixtures and
tests are listed in [FIXTURES.md](FIXTURES.md) and [TESTS.md](TESTS.md).

Status: updated 2026-10-20 at M2 close. At checkpoint 2b the Captain split motifs from
tactics (§4.5): M2 measures motif recognition only; tactics are milestone PostM2_EM1.
Updated 2026-10-20 with the frozen PostM2_EM1 plan (§3.2, §4.5, §5.8, §6).
Moved 2026-10-21 from `Planning/DEFINITIONS.md` (title "Definitions") to
`Planning/definitions/TERMINOLOGY.md`, beside `FIXTURES.md` and `TESTS.md` (§6.1).
Moved again 2026-10-21 to `Planning/DBC/DEFINITIONS.md` (title "Definitions"), beside
`CONCEPTS.md` and `CODE.md` (§7). Section headings no longer name milestones; the
"Defined in" columns still point to the plan sections that decided each term.

## 1. Words that mean several things

Read these first. Each word is used in two or more senses in the plans.

| Word | Senses in this project |
|---|---|
| target | (1) **Motif target**: a piece the motif acts on, e.g. the king and queen in a fork (§4.3). (2) **Metric target**: the frozen minimum value a measured number must reach for a test to pass, e.g. "recall target 0.95" (§5.6) |
| precision | Used in four senses over time. (1) Until 2026-10-19: share of our puzzle predictions that Lichess also named, now called **Lichess agreement** (§5.4). (2) 2026-10-19 to 2026-10-20: **engine-confirmed precision**, share of reported motifs that Stockfish turns into a material win (§5.5); it measured whether a tactic *works*, not whether the pattern is *on the board*, and left M2 with the motif/tactic split. (3) Since 2026-10-20, for motifs: **motif recognition**, exact agreement with the definitions, checked against the reference implementation (§5.7). (4) The everyday sense, "how often a reported fact is right". The plans avoid the bare word |
| line | A sequence of moves from a stated position. Engine lines are Stockfish's principal variations; a puzzle's line is its solution moves (§3.1) |
| threat | Not the everyday "something attacks something". A threat is the line a side would play **if it were its move**, taken from an engine search, with its consequence (§3.3) |
| gain | (1) A motif's `gain` field: an SEE value in pawns, only on `hanging_piece` and `trapped_piece` (§4.3). (2) The `material` of a consequence (§3.3). (3) A tactic's gain: its **only-move margin**, how much better in ES its line is than the best alternative move (§4.5). (4) "Hit-no-gain" in the former engine confirmation (§5.5) |
| forced | A **forced** move is the only legal move of its position: a rule fact in the report, never judged as a tactic (§4.5). Not the loose chess sense "the only good move"; that is a tactic or a lone fine move |
| fine | A verdict: the move loses at most 0.03 expected score against the best move (§3.5). Not "good" or "best" |
| hanging | Our `hanging_piece` is an SEE rule (X wins at least 3 pawns on the square), not the everyday "undefended" (§4.4) |
| side | The side a fact is about. In analysis: `S` side to move, `O` opponent. In motifs: `X` the side that benefits, `Y` the other (§2) |
| frozen | Changed only through a contract change approved by the Captain (§6.1) |
| gate | A check that must pass for a batch or milestone to be accepted (§6.3) |
| level | (1) **Audience level**: beginner, club or advanced, chosen by the user; changes only the wording of the explanation, never the analysis (§3.1). (2) **Diagram level**: L1, L2 or L3 in CONCEPTS.md (§7) |

## 2. Chess notation in the plans

| Term | Meaning | Defined in |
|---|---|---|
| FEN | Text form of one position (placement, side to move, castling, en passant, counters) | standard |
| SAN / UCI move | `Nf3` / `g1f3`. The CLI accepts both; engine lines are UCI internally, SAN in reports | M1 §5 |
| UCI (protocol) | Universal Chess Interface: the text protocol ChessWhy uses to talk to Stockfish, which runs as a separate process and is never linked | vision §4 |
| PGN | Text form of a game. ChessWhy reads the mainline only; comments, NAGs and variations are skipped | M1 §5 |
| ply | One half-move. Ply 0 is the position itself; ply k is the position after k moves of a line | M2 §5 |
| `S`, `O` | Side to move in the analysed position, and its opponent | vision §5 |
| `X`, `Y` | For a motif or tactic label: the side it benefits (its `side` field), and the other side. For a judged line (PostM2_EM1): X is the side playing it, the **tactic side** (`S` for the root line, `O` for a refutation) | M2 §4, PostM2_EM1 §4 |
| piece values | P 1, N 3, B 3, R 5, Q 9 pawn units; the king is never exchanged. One definition: `tactics::piece_value` | M1 §4.3, M2 §3 |
| SEE | Static exchange evaluation: net pawns X wins by the best capture sequence on one square, without search, both sides free to stop; handles x-rays, pins and king captures. **Ignores check and every other square** | M2 §4 |
| null move | Pass the move to the other side without moving (same placement, en passant cleared). Used to ask "what would the opponent do if it were their move" | vision §5.1 |
| WDL, ES | Stockfish's win/draw/loss permille. Expected score `ES = W + D/2`, 0–1, from a stated side's view. All thresholds use ES, never centipawns | vision §5 |
| es_units | ES stored as the integer `2W + D` (0–2000) so thresholds compare exactly; JSON prints es_units / 2000 | M1 §4 |
| PV | Principal variation: the line Stockfish thinks best play follows | standard |
| MultiPV K | Stockfish reports its K best first moves, each with a PV. K = 5 in search A | M1 §4.2 |
| nodes | Positions Stockfish examines. Searches stop at a fixed node count, not a time, so results repeat | M1 §3 |
| terminal | Checkmate or stalemate on the board: no search runs, and the board has no state motifs (at ply 0 or inside a line) | M1 §4.1, M2 §5 |

## 3. Analysis

### 3.1 Inputs, settings, provenance

| Term | Meaning | Defined in |
|---|---|---|
| report | The JSON document one query produces; everything the tool claims is a field of it. Versioned by `schema_version` (2 at M2; 3 from PostM2_EM1, adding `tactic` and `engine.nodes.R`) with a JSON Schema in `schema/` | vision §6, M2 §5, PostM2_EM1 §5 |
| provenance | Every fact's source: `engine` (Stockfish search), `rule` (deterministic board rule), `book` (reference material, opening names), `games` (counts from game collections) | vision §2 |
| facts first | Chess content is computed in C++ and stored in the report; the language model (M4) only puts report fields into words | vision §2 |
| budget | `quick`, `standard` (default) or `deep`: fixed node counts per search kind | M1 §3 |
| reproducible | Threads = 1, Hash 64 MB, fixed nodes, `ucinewgame` before each search: the same input gives a byte-identical report | M1 §3 |
| rerun identity | The acceptance check that two fresh runs give byte-identical JSON | M1 §14 |
| asked move | The move given with `--move`: the user's "why (not) this move?" | M1 §4.7 |
| candidates | A's K best moves with their verdicts | M1 §4.7 |
| level | `beginner`, `club`, `advanced`: audience of the M4 prose; no effect on analysis | vision §3 |
| command line | `chesswhy` with flags (`--fen` or `--pgn` with `--game`/`--ply`, `--move`, `--budget`, `--level`, `--json`, `--no-llm`): how a question arrives today | M1 §6 |
| local HTTP API | Planned: the CLI inputs as JSON over HTTP, bound to `127.0.0.1` only, with engine and model kept loaded between requests. Consumer details are an open decision | vision §9 |
| analysed position | The one position a report is about: the FEN, or the PGN game's position after `--ply` half-moves (default: its final position) | M1 §5 |
| game mainline | For PGN input: the game's positions from its start to the analysed position, used only to find the last named position | M1 §4.8, §5 |
| input error | Input that cannot be used (bad flag, invalid FEN, illegal or ambiguous move, missing game or ply): one `chesswhy: error:` line, exit 2, no report | M1 §5, §6 |
| fact | One claim about the position, stored as a report field with its provenance before any words are written | vision §2 |
| report schema | The versioned JSON Schema in `schema/` that every report must satisfy; `schema_version` names it | vision §6 |
| JSON output | The report printed as JSON (`--json`), for programs and for checking; without `--json` the output is template rendering | M1 §6 |

### 3.2 Searches A–E, R

| Search | From | Answers |
|---|---|---|
| A | current position, MultiPV 5 | best moves, `S`'s threat, `eval_white` |
| B | null move (`O` to move) | `O`'s threat and its first move `t` |
| C | current position, `searchmoves m` | the asked move when it is not among A's lines |
| D(m) | after `m`, `searchmoves t` | does `m` parry `O`'s threat? |
| E(m) | after `m`, null move | which threat `m` creates |
| R(m) | after a move `m` that is not fine, `searchmoves` all of `O`'s legal moves except the refutation's first | `O`'s best alternative to the refutation, for the refutation's tactic judgement (§4.5) |

Node counts per budget: M1 §3; R (same size as D and E): PostM2_EM1 §3. Order: A, B, per
candidate D, E, R; then C, D, E, R for an asked move outside A.

### 3.3 Threats and consequences

| Term | Meaning | Defined in |
|---|---|---|
| threat | The line a side would play if it were its move: `S`'s threat is A's best line, `O`'s threat is B's line. "White's biggest problem" is Black's threat | vision §5.2, M1 §4.2 |
| consequence | What a line achieves for the side playing it, computed by rule from the engine line: `class`, `mate_in`, `material`, `promotes`, `plies` | M1 §4.3 |
| walked plies | How far the consequence rule follows a line: 6 plies, then on while the next move is a capture, up to 10. `walked_plies` in `analysis.hpp` | M1 §4.3, M2 §6 |
| consequence class | `mate` (engine mate score for the side), else `material` (net gain ≥ 1 pawn over the walked plies), else `promotion`, else `positional` | M1 §4.3 |
| rank | Class as a number: mate 4, material ≥ 3 → 3, material ≥ 1 → 2, promotion 1, positional 0 | M1 §4.3 |
| `null_move_reliable` | False when `S` has at most one piece besides king and pawns (zugzwang-prone: passing would be an advantage, so B misleads); null when `S` is in check | M1 §4.4 |
| parry test | Whether `m` stops `O`'s threat: by rule when `t` becomes illegal, otherwise by search D (the threat still works iff `O`'s ES stays within 0.03 of B's) | M1 §4.5 |
| `parries_threat` | Result of the parry test for one move; null when `O`'s threat has rank < 2 | M1 §4.5 |
| `creates_threat` | The threat `m` creates (search E); null when `m` gives check | M1 §4.7 |
| refutation | For a move that is not fine: `O`'s best reply line after it, as a threat by `O`. From PostM2_EM1 it carries a tactic judgement (§4.5) | M1 §4.7 |

### 3.4 Urgency

| Term | Meaning | Defined in |
|---|---|---|
| urgency | Which side's threat matters more now (`more_urgent`) and by which rule | M1 §4.6 |
| `in_check` | `S` is in check: answering it is the problem | M1 §4.6 |
| `quiet` | Neither threat reaches rank 2 (material ≥ 1); the explanation leads with positional facts | M1 §4.6 |
| `consequence_class` | Ranks differ: the higher rank wins | M1 §4.6 |
| `counterattack` / `parry` | Equal ranks: if A's best move does not parry `O`'s threat, `S`'s threat is more urgent; otherwise `O`'s | M1 §4.6 |

### 3.5 Verdicts

| Term | Meaning | Defined in |
|---|---|---|
| loss | `ES_S(best) − ES_S(m)`, at least 0 | M1 §4.7 |
| verdict | fine ≤ 0.03 < inaccuracy < 0.13 ≤ mistake < 0.27 ≤ blunder (loss in ES, scaled to Stockfish 19) | M1 §4.7 |
| fine set | All moves with verdict fine: the answer to "what can I play here?" | vision §5.4 |
| `fine_truncated` | All K of A's lines are fine, so more fine moves may exist than are listed | M1 §4.7 |

## 4. Tactical motifs

### 4.1 Kinds of facts

| Term | Meaning | Defined in |
|---|---|---|
| motif | A pattern of one position: the board at one ply of a line, plus the move that produced it where the tag needs it. Found by a deterministic rule; provenance `rule`. States that the pattern is on the board and claims no gain (Captain, 2026-10-20). Motifs never change verdicts, urgency or consequences (D7) | M2 D11, §4 |
| motif tag | The motif's stable snake_case name, e.g. `fork`. 14 tags exist (§4.4): M2's 13, plus `pin_tactical` from PostM2_EM1 | M2 §4, PostM2_EM1 §4.5 |
| motif instance | One concrete occurrence: tag + side + ply + actor + targets (+ gain, + threat, + contribution). One position can hold many. Its **identity** (used by the first-appearance rule) is tag, side, actor, targets and threat (threat added 2026-10-21; before, tag, side, actor, targets) | M2 §5, PostM2_EM1 §4.5 |
| state motif | A pattern of the board alone (hanging piece, pins, back-rank weakness, overloaded defender, trapped piece, tactical pin). Computed for both sides | M2 §4.1, PostM2_EM1 §4.5 |
| move motif | A pattern that also needs the move that produced the board (fork, skewer, discovered check/attack, double check, back-rank mate, promotion). Exists only at ply ≥ 1 | M2 §4.2 |
| not detectors | Mate threat and promotion threat come from the engine's consequence, never from a motif rule | M2 §4.3 |

### 4.2 Where motifs appear in the report

| Term | Meaning | Defined in |
|---|---|---|
| position motifs | `position.motifs`: state motifs of the current position (ply 0), both sides. In the JSON, not printed | M2 §5 |
| line motifs | `motifs` on each threat, `creates_threat`, refutation and move line: motifs at plies 1 to the walked plies of that line | M2 §5 |
| first-appearance rule | Inside a line, a state motif is listed at ply k only if it holds after ply k and did not hold after ply k−1 | M2 §5 |
| line relevance | Former extra condition on `trapped_piece` (a later move of the line had to move or capture the piece). Removed 2026-10-20: it read later moves, which judges a line, not a board | M2 §8.1, §15 |
| trade in progress | Exclusion for `hanging_piece` inside a line: the "hanging" piece just captured something worth at least as much, so it is a recapture, not a gift | M2 §4.1 |

### 4.3 Fields of a motif

| Field | Meaning |
|---|---|
| `side` | X, the side that benefits |
| `ply` | Where in the line it holds (0 = the current position) |
| `actor` | The piece that does it: the forking knight, the pinner, the hanging piece itself, the overloaded defender |
| `targets` | The pieces it acts on, in a fixed order (M2 §5) |
| `gain` | SEE value in pawns, only for `hanging_piece` and `trapped_piece`; null otherwise |
| `threat` | Only on `pin_tactical`: `{kind, square}`, the kind of threat that removing the restricted piece would enable and the square b where X's threatening move lands. For `capture` that is the captured piece's square, except en passant, where b is the landing square and the captured pawn (a target) stands behind it. Before the 2026-10-21 milestone review this row equated the two. Null otherwise (§4.4, `pin_tactical`) |
| `contribution` | The motif's role in the tactic of the line it is listed on: `direct`, `indirect`, `none`, or null = not judged. Always null in M2; from PostM2_EM1 set on every motif of a tactic line, null elsewhere (§4.5) |

### 4.4 The tags in plain words

Exact definitions: M2 §4. These sentences only orient.

| Tag | Plain meaning | Status |
|---|---|---|
| `hanging_piece` | X to move can win at least 3 pawns on the square of a Y minor or major piece (by SEE) | measured |
| `pin_absolute` | An X slider, a Y piece, then the Y king on one line: the Y piece may not move off the line | measured (as `pin`) |
| `pin_relative` | As above, but the rear piece is a more valuable Y piece that X would win material on if the front piece moved | measured (as `pin`) |
| `back_rank_weakness` | Y king on its first rank, boxed in by its own pieces or X's attacks, and no Y rook or queen guards the rank | fixture-only |
| `overloaded_defender` | One Y piece is the only reason two or more Y pieces are safe | fixture-only |
| `trapped_piece` | A Y piece X can win on its square, and every move of it loses it too | measured |
| `pin_tactical` | A Y piece p is held in place by a threat: if p were gone, an X move would make a fork, skewer, discovered check or attack, double check, back-rank mate, promotion or checkmate that it does not make now, or X would win a Y piece by SEE that it cannot win now. Includes p guarding an empty square. Not computed while either king is in check (only Y's before 2026-10-21's gate), and not for a p whose removal would check either king. Actor: the X piece that would move; targets: p, then the threat's targets; `threat` names the kind and square. Called "tactical pin" in conversation (not a chess-literature term) | fixture-only |
| `fork` | One X move attacks two or more Y pieces worth attacking, and the moved piece is safe | measured |
| `skewer` | An X slider attacks a Y piece, behind which stands a less valuable Y piece X then wins | measured |
| `discovered_check` | An X move unmasks check from another X piece | measured |
| `discovered_attack` | An X move unmasks an attack from an X slider on a valuable or winnable Y piece (not check) | measured |
| `double_check` | After the move, two X pieces give check | measured |
| `back_rank_mate` | Mate on Y's first rank against a king boxed in by its own pieces | measured |
| (`removal_of_defender`) | Removed from M2 on 2026-10-20: X captures a Y defender, then later in the line wins the piece it defended. It reads later moves, so from PostM2_EM1 it is a **tactic label** on tactic lines, not a motif (§4.5) | tactic label |
| `promotion` | The move promotes a pawn | measured |

"Measured" = has a Lichess theme and is evaluated on the puzzle sample (§5).
"Fixture-only" = no clean Lichess theme, so no recall is measured; checked by T fixtures
(§6.2) and, like every tag, by the reference implementation (§5.7).

### 4.5 Motif, tactic and contribution (Captain, 2026-10-20)

Decided at M2 checkpoint 2b (M2 D11–D13); rules frozen in the PostM2_EM1 plan
(2026-10-20, E1–E7). Until then the threshold and the "indirect" rule were open.

| Term | Meaning | Where |
|---|---|---|
| tactic | A sequence of moves that gains in engine evaluation even when the opponent plays the best replies. Measured in evaluation, not material, so a sacrifice can be a tactic. The gain is measured against the **best alternative move** (the only-move margin): along the engine's own best line the evaluation does not change, so the comparison point is the alternative. A line is a tactic iff its only-move margin is ≥ 0.13 ES (`kTacticMarginUnits` = `kMistakeUnits`): every alternative is at least a mistake. Threshold open before 2026-10-20 | PostM2_EM1 §4.1, E2 |
| only-move margin | Line ES minus the best alternative's ES, from the line side's view, in es_units; may be negative. Root line: `candidates[0]` minus `candidates[1]` (search A). Refutation: (1 − ES of the refuted move) minus search R's line. Before 2026-10-20 defined for the root only ("best minus second-best of A") | PostM2_EM1 §4.1 |
| judged line | A line that gets a tactic judgement: the root's best line (`candidates[0]`) and every refutation. Other candidates, B's and E's threats and lines from C are not judged | PostM2_EM1 §4.1, E3–E4 |
| tactic judgement | The `tactic` object of a judged line: `forced`, `alternative`, `margin`, `is_tactic`, `labels` | PostM2_EM1 §5 |
| tactic line | A judged line with `is_tactic`. Only tactic lines get contributions and tactic labels | PostM2_EM1 §4.2 |
| best alternative | The move a tactic is compared with: A's second line at the root, R's line for a refutation | PostM2_EM1 §4.1 |
| tactic sequence | A judged line cut to its walked plies (§3.3): the moves contributions and labels read | PostM2_EM1 §4 |
| forced (in the report) | `tactic.forced`: the judged line's root has exactly one legal move. No alternative and no margin; never a tactic; source `rule` | PostM2_EM1 §4.1, E5 |
| saturation limit | ES flattens near 0 and 1, so in an already decided position (mate against a second-best at 0.98) no line is a tactic. Known, accepted | E2 |
| two-winners limit | With two equally winning moves the margin is near 0, so neither is a tactic. Known, accepted | E4 |
| motif recognition | Is the pattern on the board, and do we report it exactly when it is? Must be exact against the written definitions; checked by the T fixtures and the reference implementation (§5.7) | M2 §7.4 |
| tactic recognition | Does the line gain against the best alternative, and do we label it a tactic exactly when it does? Mechanical from the engine, so exact by construction; tests check the computation (invariant check, §6.2) | PostM2_EM1 §14 |
| contribution judgement | For each motif on a tactic line, by rule: motifs of the other side → **none**; **direct** if it is a move motif or a later move of the tactic side captures one of its pieces of the other side; **indirect** if not direct, a piece of the other side in the motif attacks (pins ignored) the destination of a later move of the tactic side, and none of the motif's pieces has moved or been captured by then; else **none**. Plain sense: direct = the tactic's moves make it or capture its pieces; indirect = it stops the defender answering a move (a pin that stops a recapture, an overloaded defender); none = the tactic works without it. Since 2026-10-21; before, any later move by either side that moved or captured a motif piece made it direct, and any motif piece (including the tactic side's own) attacking a destination made it indirect. "Indirect" was open before 2026-10-20. Measured against hand-labelled C fixtures (§6.2) | PostM2_EM1 §4.3, §15 |
| tactic label | A rule fact about a tactic line that reads later moves, so it is not a motif. Its fields match a motif's (`tag`, `side`, `ply`, `actor`, `targets`), without `gain` and `contribution`. One tag: `removal_of_defender` | PostM2_EM1 §4.4 |
| `removal_of_defender` | Tactic label: an X capture takes a Y piece d that alone kept another Y piece t safe (SEE 0 with d, ≥ 1 without), and a later X capture on t's square wins it. Actor: the capturing piece; targets: d, t. Outside the motif reference implementation | PostM2_EM1 §4.4 |
| restricted movement | Report wording (Captain, 2026-10-21) shared by the four tags that hold a Y piece in place: "restricted movement (absolute pin)", "(relative pin)", "(overloaded defender)" and "(tactic: Nd5-f4 fork)" for `pin_tactical` (actor, threat square, threat kind). Wording only: the JSON keeps the tags. An open proposal before 2026-10-21's freeze | PostM2_EM1 §10 |
| claim tag / presence tag / rule fact | Former split of the tags (2026-10-19/20) by whether the tag's name claims a material win. Retired 2026-10-20: every motif is a presence fact | M2 §15 |
| PostM2_EM1 | The milestone after M2 (Captain's name): tactic recognition, motif contribution, `removal_of_defender` as a tactic label, `pin_tactical`. Plan `Planning/milestones/PostM2_EM1.md`, receipt `Planning/receipts/PostM2_EM1.md`; complete, merged into `main` 2026-10-21 (branch `postm2-em1`). Seeds: `5cc9582^` (former detector), `2e4104e^` (fixtures T33, T34) | vision §11 |

## 5. Measurement

### 5.1 The puzzle data

| Term | Meaning | Defined in |
|---|---|---|
| Lichess puzzle database | Public CC0 export of Lichess puzzles: FEN, solution moves, themes, rating, plays, popularity. Pinned export of 2026-10-05 | M2 §7.1 |
| theme | Lichess's label for what a puzzle is about (`fork`, `pin`, ...), assigned by lichess-puzzler and the community. A theme names the puzzle's **idea**, not every motif present on the board | M2 §7.3 |
| eligible | Puzzles used for sampling: rating deviation ≤ 80, ≥ 1000 plays, popularity ≥ 80, solution ≤ 6 plies | M2 §7.1 |
| root | The puzzle position after the opponent's first move (`Moves[0]`): where the solver starts | M2 §7.3 |
| solver | The side to move at the root | M2 §7.3 |
| solution line | `Moves[1..]`, the moves the puzzle expects | M2 §7.3 |
| split | Two disjoint samples drawn by hashing puzzle IDs. **dev**: for tuning, local only. **test**: committed in `tests/fixtures/puzzles/test.csv`, measured only after tuning is finished, so its numbers are not fitted to it | M2 §7.2 |
| background | The first 6,000 eligible puzzles of a split, regardless of theme: a fair picture of how often motifs occur | M2 §7.2 |
| stratum | Per measured Lichess theme, 300 further puzzles that carry that theme | M2 §7.2 |
| mapping | Which of our tags count for which Lichess theme (e.g. `pin` ← `pin_absolute` or `pin_relative`) | M2 §7.3 |
| predicted | We "predict" a theme for a puzzle when an instance of a mapped tag with side = solver appears at the root or anywhere in the solution line | M2 §7.3 |

### 5.2 Counting words

| Term | Meaning |
|---|---|
| TP (true positive) | We predict the theme and Lichess names it |
| FP (false positive) | We predict the theme, Lichess does not name it |
| FN (false negative), miss | Lichess names the theme, we do not predict it |

### 5.3 Recall

| Term | Meaning | Defined in |
|---|---|---|
| recall | On a theme's stratum: share of puzzles Lichess labels with it where we also predict it. "When Lichess says *fork*, do we find a fork for the solver?" Gated for the 10 measured themes. A Lichess theme names the motif behind the puzzle's tactic (a direct contribution, §4.5), so recall checks that our definitions catch what chess players mean by the word | M2 §7.3 |

### 5.4 Lichess agreement

| Term | Meaning | Defined in |
|---|---|---|
| Lichess agreement | On background: TP / (TP + FP), the share of our predictions that Lichess also names. Low values mostly mean Lichess names one idea per puzzle while we list every motif (92% of `hanging_piece` "false positives" were pieces the solver does capture). Printed, never gated, never optimised. Called "precision" before 2026-10-19 | M2 §7.3, §15 |

### 5.5 Engine confirmation (removed from M2 on 2026-10-20)

Kept so old receipts and §15 rows stay readable. The evaluator is gone from the tree;
PostM2_EM1 starts from commit `d5b703a`.

| Term | Meaning | Defined in |
|---|---|---|
| engine-confirmed precision | For a claim tag: share of sampled instances that Stockfish confirms. It answered "does this tactic win material in Stockfish's best line?", not "is the pattern on the board?" | former M2 §7.4 |
| sample (N) | Per tag, N instances from background (default 200), chosen by hashing puzzle ID and instance, so the same instances are drawn every run | former M2 §7.4 |
| board B | The position where the instance holds: the root after `ply` line moves | former M2 §7.4 |
| tracked pieces | The instance's actor and targets that are Y pieces on B, followed as Y moves them | former M2 §7.4 |
| hit | In Stockfish's line from B, X captures a tracked piece within the walked plies | former M2 §7.4 |
| confirmed | Hit, and X ends ≥ 1 pawn ahead or mates (by the consequence rule). B already checkmate of Y also counts | former M2 §7.4 |
| no-hit | Stockfish's line never captures a motif piece: the motif is not what the best line uses (it may be unreal, or the engine prefers something else) | former M2 §7.4 |
| hit-no-gain | A motif piece is captured but X is not a pawn ahead: an even trade or the gain is given back | former M2 §7.4 |

### 5.6 Calibration and targets

| Term | Meaning | Defined in |
|---|---|---|
| mean instances per puzzle | Average number of motif instances (all tags, both sides, root plus line) per background puzzle; a noise measure, lower is better at equal recall | M2 §7.3 |
| calibration | Adjusting the conditions inside the §4 definitions on dev, so the report contains exactly what is measured. Tags, sample and mapping stay fixed | M2 §8.1 |
| keep rule | A calibration change stayed only if it improved its tag's recall or confirmed precision without hurting the other by > 0.02, did not raise mean instances, and broke no frozen fixture. Calibration closed 2026-10-20 | M2 §8.1 |
| target (metric) | The frozen minimum for a measured number. Proposed as test value − 0.03, rounded down to a multiple of 0.05. A target records today's level so later changes cannot silently lower it; it is not a judgement that the level is good enough | M2 §8.2 |
| sampling noise | One standard error of a measured share: with 200 instances, a share near 0.7 is uncertain by about ±0.03; with 300 stratum puzzles, recall near 0.9 by about ±0.02. Fixed samples make reruns identical, but not closer to the true value | binomial estimate |
| disagreement | A puzzle where our prediction and Lichess's theme differ (an FP or FN); written by `--disagreements` | M2 §7.3 |
| audit | Hand review of the first 10 recall misses per measured tag, each classified (detector wrong, Lichess label doubtful, unclear). Reported in the receipt, not gating | M2 §8.3 |

### 5.7 Reference implementation (motif recognition)

| Term | Meaning | Defined in |
|---|---|---|
| reference implementation, oracle | A second, deliberately plain implementation of the motif definitions in `tests/tactics/oracle/`, written from the plan text only, without reading the real detectors (`src/tactics/`). If both agree everywhere, the detectors do what the text says | M2 §7.4 |
| mismatch | A position where the detectors' motif list and the reference's differ in any field. Each one is traced to its root: detector wrong, reference wrong, or plan text ambiguous (then the text is clarified) | M2 §7.4 |
| compared positions | Every position of every `test.csv` puzzle (before and after each move, plus the whole line) and every T fixture. `pin_tactical` is compared only on the **sample**: puzzles whose 0-based CSV row index is divisible by 16, and every T fixture (Captain, 2026-10-21: it costs about 18 ms per board). `tactics_oracle` passes only with zero mismatches | M2 §7.4, PostM2_EM1 §9 |

### 5.8 Puzzle check (not gating)

| Term | Meaning | Defined in |
|---|---|---|
| puzzle check | `tactics_puzzle_eval` treats each puzzle's solution line from the root as a tactic sequence and prints the two numbers below. Printed in the receipt; never gated, never tuned against | PostM2_EM1 §7 |
| direct share | Per measured theme, on its stratum, among puzzles where we predict the theme: share where a mapped instance for the solver is judged `direct`. Checks the contribution rule against Lichess's naming of the puzzle's idea | PostM2_EM1 §7 |
| `removal_of_defender` recall | Share of the `capturingDefender` stratum where the solver gets a `removal_of_defender` label; Lichess agreement on background alongside | PostM2_EM1 §7 |

## 6. Process and testing

### 6.1 Planning and roles

| Term | Meaning | Defined in |
|---|---|---|
| vision | `Planning/PROJECT_VISION.md`: the source of truth for scope, method and decisions | AGENTS.md |
| milestone (M0–M6, PostM2_EM1) | A delivery step with its own plan `Planning/milestones/<name>.md` and frozen acceptance criteria. PostM2_EM1 sits between M2 and M3 (§4.5); M3 waits until it closes (E1) | vision §11 |
| contract | The frozen parts of a milestone plan: headers, report schema, fixtures, acceptance, owned files. Workers implement against it and do not change it | AGENTS.md |
| contract change | Any change to a frozen item after freezing; recorded as a row in the plan's last section ("§15 row" in M2) with date, change and reason | M2 §15 |
| D1–D13 | The Captain's numbered decisions in M2: D1–D10 at the start, D11–D13 at checkpoint 2b (motif/tactic split, reference implementation, `removal_of_defender` removed) | M2 §1 |
| E1–E7 | The Captain's numbered decisions in PostM2_EM1 (briefed as P1–P5 on 2026-10-20): order, threshold, judged lines and search R, best move only, forced moves, contribution rule, `removal_of_defender` as a tactic label | PostM2_EM1 §1 |
| checkpoint | A point where the Captain approves something before it is frozen (M2: fixtures; targets; merge. PostM2_EM1: C fixtures and F rows; rule-versus-label mismatches; merge) | M2 §12, PostM2_EM1 §12 |
| work package (WP) | One worker's slice of a milestone: owned files, done-condition, non-goals | M2 §11 |
| batch | WPs run at the same time; integrated and tested together | AGENTS.md |
| orchestrator / Deputy | The main session: freezes contracts, slices work, integrates, runs gates, reports to the Captain | AGENTS.md |
| worker, fixer, reviewer, scout | Subagents: implement one WP; fix one failing gate; review a finished milestone once; read-only search | AGENTS.md |
| receipt | `Planning/receipts/M<N>.md`: what a milestone delivered and the evidence (tables, timings, gate results) | AGENTS.md |
| blocker / scope proposal / nit | Review outcomes: a blocker cites a broken acceptance row or principle with a failing input; a scope proposal is new work for the Captain to decide; a nit is optional | AGENTS.md |

### 6.2 Fixtures

| Term | Meaning | Defined in |
|---|---|---|
| fixture | A stored test case with expected results; all are listed in `FIXTURES.md` | — |
| F fixtures | Engine fixtures in `tests/fixtures/positions.json` (F01–F20, F16 dropped; PostM2_EM1 adds F21 onward): world-championship positions with expected threats, urgency, verdicts and, from PostM2_EM1, tactic judgements; pinned to Stockfish 19 at `standard`, robust at all budgets | M1 §9, PostM2_EM1 §9 |
| U, E, parse fixtures | No engine judgement: U = opening names and PGN/ply input, E = error exit codes and messages, parse = FEN/move parsing | M1 §9 |
| T fixtures | Motif unit fixtures in `tests/fixtures/motifs.json` (T01–T41 from M2; T42 onward for `pin_tactical`, PostM2_EM1): a position and line with instances that must (`includes`) or must not (`excludes`) be reported. A `pin_tactical` include also states its `threat` | M2 §9, PostM2_EM1 §9 |
| C fixtures | Contribution unit fixtures C01–Cnn in `tests/fixtures/tactics.json`: a tactic sequence with the hand-labelled contribution of every motif of the tactic side (except `pin_tactical`, listed only for chosen instances, Captain 2026-10-21), plus tactic labels that must or must not appear. Hand labels come from the plain meaning, not from running the rule; the Captain reviews them | PostM2_EM1 §9 |
| hand label | A fixture's expected value set by a person from the chess meaning, with a one-line reason, before the rule's output is seen. Where rule and hand label differ, the Captain decides which changes | PostM2_EM1 §9, §12 |
| invariant check | Part of the F harness from PostM2_EM1: on every F report, recomputes each tactic judgement from the report's own evals (margin, threshold, forced, which lists carry contributions) | PostM2_EM1 §9 |
| positive / near miss | A T fixture where the motif holds / a deliberately similar position where it must not (e.g. a fork whose forking piece can be taken) | M2 §9 |
| constructed | A fixture position made up by hand rather than taken from a game | M2 §9 |
| `motifs_include` | Key on an F fixture: listed motif instances must appear in the report (extras allowed). From PostM2_EM1 an entry may also state its `contribution` | M2 §9, PostM2_EM1 §9 |
| re-blessing | Re-deriving engine fixture expectations for a new Stockfish version; an explicit milestone task | AGENTS.md |

### 6.3 Tests, gates, builds

| Term | Meaning | Defined in |
|---|---|---|
| CTest label | `unit`: no engine or model. `engine`: needs `assets/engines/stockfish`. `model`: needs a GGUF model, opt-in. Missing assets fail, never skip | AGENTS.md |
| `tactics_puzzles` | CTest (`unit`) running `tactics_puzzle_eval` on test: recall and Lichess agreement; `pin_tactical` switched off (no theme maps to it) | M2 §7.3, PostM2_EM1 §9 |
| `tactics_oracle` | CTest (`unit`) running `tactics_motif_oracle`: detectors against the reference implementation on test and the T fixtures (`pin_tactical` on the sample only, see compared positions); zero mismatches required. Replaced `tactics_confirm` (engine-confirmed precision, removed 2026-10-20) | M2 §7.4, PostM2_EM1 §9 |
| gate | Required checks. After a batch: build and affected tests. At milestone end: all tests in Release and ASan/UBSan, clang-format, clang-tidy, Ruff, basedpyright, ShellCheck, the smoke command | AGENTS.md |
| Release / ASan build | `build/release` (optimised) and `build/asan` (debug with address and undefined-behaviour sanitisers) | AGENTS.md |
| smoke command | One frozen CLI invocation run by hand at milestone end to see the feature work end to end. M2 and PostM2_EM1: `--fen 'k7/8/8/8/3Q3n/8/2P5/4K3 w - - 0 1' --move c3`, expecting Black's knight fork from f3 (M2), and from PostM2_EM1 a refutation that is a tactic with the fork `direct`, while the root (all lines at ES 1.0) is not a tactic | M2 §14, PostM2_EM1 §14 |
| golden test | Compares rendered text with a stored expected file byte for byte | M2 §10 |
| timing check | `standard` must finish within 12 s per F fixture in Release | M2 §14 |
| usage guard | Background watcher that pauses agent work when a model subscription window nears its limit | AGENTS.md |

## 7. Driven by Concepts

| Term | Meaning | Defined in |
|---|---|---|
| DBC (Driven by Concepts) | How the project is led: the Captain decides at the level of concepts in CONCEPTS.md; code, tests and fixtures are mapped to those concepts, and a drift check keeps the map honest. Named by the Captain 2026-10-21 | AGENTS.md |
| DBC files | The five files in `Planning/DBC/`: CONCEPTS.md, CODE.md, DEFINITIONS.md, FIXTURES.md and TESTS.md, each linking to the other four. Orchestrator-owned. The folder was `Planning/definitions/` (glossary, fixtures, tests) before 2026-10-21 | AGENTS.md |
| CONCEPTS.md | The concept map: mermaid diagrams at three diagram levels, each with a node table (meaning, terms, status). Holds no code; the Captain's edits to it are directives | CONCEPTS.md |
| CODE.md | Places every built concept node in the source: files and functions, classes, structs or constants, never line numbers | CODE.md |
| DEFINITIONS.md | This glossary. Called TERMINOLOGY.md (in `Planning/definitions/`) on 2026-10-21 before the DBC move, and `Planning/DEFINITIONS.md` before that | AGENTS.md |
| FIXTURES.md | Every fixture with its board, claim, current result ("Codebase today"), the concept nodes each set checks, and a Comments column where agents raise fixture problems | AGENTS.md |
| TESTS.md | Every CTest with its label, what it checks and the concept nodes it checks | AGENTS.md |
| concept node | One idea in a CONCEPTS.md diagram: a node ID, a meaning, the defined terms it uses and a node status | CONCEPTS.md |
| node ID | The CamelCase name of a concept node, unique in CONCEPTS.md; CODE.md, TESTS.md and FIXTURES.md refer to nodes by it | CONCEPTS.md |
| diagram level | L1: the whole project. L2: one diagram per L1 node. L3: a small diagram for an L2 node that is a rule with steps | CONCEPTS.md |
| node status | `implemented` (built), `partial` (some of its parts built), `planned` (nothing built), `external` (see below) | CONCEPTS.md |
| owned node / external node | Every node ID belongs to exactly one diagram, where it is owned. Another diagram may show it again as an external node, for context only | CONCEPTS.md |
| drift | Any disagreement inside the DBC files or between them and the code: a built node with no code, a code reference that no longer exists, an undefined term, a test or fixture that names no concept, a status that contradicts the diagram below it, a broken link | AGENTS.md |
| drift check | `python3 tools/check_dbc.py`: reports every drift with file and line; run after each batch and at every gate | AGENTS.md |

## 8. Positional facts, reference knowledge and explanation

Mostly planned; [CONCEPTS.md](CONCEPTS.md) marks what is built.

| Term | Meaning | Defined in |
|---|---|---|
| positional fact | Planned: a lasting feature computed by rule (pawn structure, king safety, piece activity, space and development, material imbalance), reported as a before/after change along a line and used only when tied to a line or verdict | vision §5.6 |
| position key | Piece placement, side to move, castling rights and an en-passant square only when a legal capture exists; move counters ignored, so transpositions match | vision §5.7, M1 §4.8 |
| opening name | Name and ECO code of a position in the pinned opening dataset; `book` provenance; never guessed for an unnamed position | vision §5.7, M1 §4.8 |
| last named position | For PGN input: the last position on the game mainline (up to and including the analysed position) that has an opening name, with its ply | M1 §4.8 |
| game-count table | Planned: a position → move-count table built offline from pinned game collections, each source and platform kept apart, with a recorded table id | vision §5.7 |
| game counts | Planned: per source and position, the number of games, results and the count for each move played; `games` provenance; every figure names its source and platform and is never phrased as move quality | vision §2, §5.7 |
| book corpus | Planned: the Captain's local books as text, kept out of the repository | vision §8 |
| book index | Planned: a BM25 search index over page or paragraph chunks of the book corpus, kept out of the repository | vision §8 |
| concept tag | Planned: a motif or positional tag from the report, widened by a checked-in keyword map, used as the book query | vision §8 |
| book passage | Planned: one to three short excerpts from the book index, cited by book and page, offered as general advice and never as analysis of the position | vision §8 |
| explanation | The text a reader gets for one report: template rendering today; later language-model prose that passed the fact check | vision §7 |
| template rendering | Fixed sentences filled from report fields, adding no claim: the readable output today, the `--no-llm` output, and the fallback when prose fails the fact check | vision §7.3 |
| fact sheet | Planned: a compact text form of the report (not raw JSON) given to the language model with a level-specific prompt | vision §7.2 |
| language model | Planned: a local GGUF model run through llama.cpp (linked as a library) that turns the fact sheet into prose for the chosen level; decides no chess content | vision §7.1 |
| fact check | Planned: every move, square and piece in the prose must be in the report, and opening names and game counts must match it exactly; one regeneration, then template rendering with a notice | vision §7.3 |
