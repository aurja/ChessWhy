# PostM4_EM5 — plan (fixes from the conceptual critique of 2026-10-24)

Branch `postm4-em5`, from `main` after PostM4_EM4 is merged. Source: the critique in
`Planning/critiques/2026-10-24/` (findings 00–08, synthesis 09). This file holds the
Captain's decisions and the assignment of every finding to a part. Each part's contract
(rules, report changes, fixtures, work packages, acceptance) is frozen in this file before
its work starts; parts 2 and 3 do not depend on part 1 and may be frozen while it runs.

## 1. Captain's decisions

| # | Topic | Decision |
|---|---|---|
| R1 | Milestone shape (2026-10-25) | PostM4_EM4 closes as the critique and move report text milestone; the critique's fixes, including the pure reply replacement, form this milestone in three parts: 1 reply evidence, 2 correctness, 3 contract and wording repairs |
| R2 | VER2-1, verdict of an exact best-score tie from search C (2026-10-25) | Option A: the same-score test applies to search C results too, so a move from C with exactly the best score is `best`. Changes vision §5.4's rule that an asked move outside A is at most okay. Rejected: B, keep the rule and redefine okay as "playable, not tied within A" |
| R3 | REP2-1, what a game count counts (2026-10-25) | Option 1: keep the first-visit table and say what it measures: "chosen on the first visit in X of Y games"; zero means "not chosen on the first visit"; the counting policy goes with the table metadata. Rejected: 2, count each distinct (position, move) per game (table rebuild, move counts no longer sum to games) |
| R4 | Pure reply (2026-10-24, from PostM4_EM4) | Replace the definition with "forced and gains nothing" (option C of 2026-10-24) |
| R5 | D1, THR2-1 terminal draws (2026-10-25) | Option B: a terminal draw is an orthogonal field of the consequence (stalemate or insufficient material, with its ply); class and rank stay as measured; a move that ends the game has no refutation. Rejected: a class `draw` of rank 0 (would strip F21's `Nxf6+` of its tactic) |
| R6 | D2, THR2-2 walk settlement (2026-10-25) | Option A: the walk also goes on through checks and the replies to them, and reports whether it settled. Rejected: B, flag only; C, extend only |
| R7 | D3, forced reply (2026-10-25) | F-A: acceptable replies (within 0.05 of the best reply, MultiPV 5 at `L`); forced iff every acceptable reply stops the threat; unresolved when all five lines are acceptable. Rejected: F-B pass test only; F-C only move |
| R8 | D4, reply gain (2026-10-25) | From the board before the threatening move through the settled exchange after the reply; material, promotion or mate only; unknown when the walk ends unsettled. Rejected: baseline before the reply, end of the walk as endpoint, positional gains |
| R9 | D5, labels and run endings (2026-10-25) | Gaining, unforced and unresolved replies; run endings answered and unresolved; no tempo gained without a pure reply; counter-threat unchanged. Rejected: one catch-all answer label |
| R10 | D6, POS2-1 favoured side (2026-10-25) | A captured weakness helps nobody. Rejected: rename to nominal polarity |
| R11 | D7, POS2-2 king pressed (2026-10-25) | Shelter witness for king-file changes. Rejected: rename to "selected for explanation" |
| R12 | D8, TMP-2 run outcome (2026-10-25) | Conversion evidence: material to the run's settled end, static facts only, unfinished runs `unknown`. Rejected: rename the outcomes |
| R13 | D9, tempo rows in the move report text (2026-10-25) | Return at the end of part 1, in the position blocks (§3.8). Rejected: full report only until M5 |

### 1.1 Open decisions (briefed 2026-10-25)

Briefed to the Captain by the Deputy on 2026-10-25; terms in `DEFINITIONS.md` §3.7 (part 1) and §3.8 (parts 2 and 3). D1–D9
freeze part 1; D10–D14 part 2; D15–D16 and the block approval part 3. Evidence: the Release
build of `main` after the PostM4_EM4 merge (report v8) on the 43 F fixtures (`--full-report
--json`), replayed with python-chess. D1–D9 were decided as recommended on 2026-10-25 (R5–R13);
D10–D16 and the block approval stay open until part 1 is done.

| # | Finding | Question | Deputy's recommendation |
|---|---|---|---|
| D1 | THR2-1 | How a terminal draw enters a consequence | Orthogonal `terminal` field (class and rank unchanged) and no refutation after a move that ends the game; not a new class `draw` of rank 0, which would also strip F21's `Nxf6+` (fork into king and knight against king) of its tactic |
| D2 | THR2-2 | Walk settlement | Settled walk (extend through checks and replies to them) plus `walk_settled`. 88 of 531 F lines walk further, in 18 fixtures; 7 root-move lines change material; F07 `Kg1` goes from material +3 to −6 |
| D3 | 00, R4 | Which alternatives decide a forced reply | Acceptable replies (within 0.05, MultiPV 5 at `L`, a reply search per acceptable alternative); 79 replies to test over the F fixtures, at most 7 per fixture, F12 has 1 |
| D4 | 00, R4 | Reply gain baseline, endpoint, what counts | Before the threatening move; through the settled exchange after the reply; material, promotion, mate only. `...Nxc2` in V2/F41: −3 + 5 = +2, not pure |
| D5 | TMP-4 | Labels and run endings for non-pure stopping replies | Gaining, unforced and unresolved replies; endings answered and unresolved; counter-threat unchanged |
| D6 | POS2-1 | Favoured side of a captured weakness | Captured weakness helps nobody |
| D7 | POS2-2 | King pressed as use | Shelter witness for king-file changes |
| D8 | TMP-2 | Run outcome | Conversion evidence: material to the run's end, static facts only; unfinished runs `unknown` |
| D9 | §2 open item | Tempo rows in the move report text | Return at the end of part 1, one row per position block, layout shown on V2 before building |
| D10 | MOT2-1 | Costly restricted move | Net exposure |
| D11 | MOT2-2 | Checking escape | Legal-capture gate (the hanging-piece gate) for relative pins and overloaded defenders |
| D12 | MOT2-3 | Back rank guarded | Back-rank guard; the motif reference implementation and recall move with it |
| D13 | TAC2-1 | Surviving pin in rule 3′ | Motif revalidation for every tag |
| D14 | REP2-2 | Relevance filter | Mandatory conditions incl. `O`'s rank ≥ 2 threat and the parry result; root urgency back as one row |
| D15 | TMP-3 | Tempo labels | Keep the names; meaning becomes the pass cost only; null tempo value prints unknown |
| D16 | 09 §4 | Vision drift | Vision follows EM4 R1 on printed restrictions; a tempo section in vision §5 after part 1; the other four items mechanical |
| — | TAC2-2, TMP-5, POS2-3, PRF2-1, PRF2-2, POS2-4 | Block approval | Wording fixes as the critiques' first candidates; POS2-4 specifies the legal domain only |

## 2. Findings by part

Group letters G1–G10 are the synthesis's (09 §2).

### Part 1 — reply evidence (in this order)

| Finding | Group | Fix owner |
|---|---|---|
| THR2-1 terminal draws fall outside consequence and refutation | G1 | consequence replay: terminal outcome |
| THR2-2 capture-only extension stops before a forced recovery | G1 | consequence replay: endpoint completeness |
| POS2-1 losing a weak pawn credited to its owner | G3 | favoured side |
| POS2-2 any check certifies use of a shelter change | G3 | use test |
| TMP-2 "converted" while the checking attack goes on | G3 | run outcome |
| 00 pure reply; TMP-4 non-pure and unresolved branches | G2 | reply evidence and the ply label / run ending state space (R4) |

The tempo rows return to the move report text at the end of part 1 (R13, §3.8).

### Part 2 — correctness

| Finding | Group | Fix owner |
|---|---|---|
| MOT2-1 a costly restricted move can collect more than it exposes | G4 | restricted moves |
| MOT2-2 a checking escape restricted by an illegal reply | G4 | restricted moves (relative pin) |
| MOT2-3 being on the back rank is not guarding it | G4 | `back_rank_weakness` |
| TAC2-1 unmoved pieces do not imply a surviving pin | G4 | contribution rule 3′ |
| POS2-4 break cost over unsearchable moves | G4 | specify the legal search domain only; break cost stays an open proposal |
| REP2-2 relevance filter drops deferred-parry conditions | G8 | relevance filter's mandatory conditions |

### Part 3 — contract and wording repairs

| Finding | Group | Fix owner |
|---|---|---|
| VER2-1 exact tie from C graded okay | G6 | verdict (R2) |
| REP2-1 first-visit counts worded as games played | G9 | game counts (R3) |
| TMP-3 low pass cost labelled non-tempo | G5 | tempo label |
| POS2-3 impact and duration from the tag taxonomy | G5 | impact rating, static scale |
| TAC2-2 tactic recognition describes the retired margin test | G7 | glossary |
| TMP-5 even trade vs check rule | G7 | glossary (threatening move) |
| PRF2-1 output volume is not noise | G10 | proof measurements |
| PRF2-2 oracle scope wording hides the sample | G10 | proof wording |
| Vision vs concept map, six items (09 §4) | — | vision and `CONCEPTS.md`, each reconciled with the Captain |

### Not in this milestone

Plans and pawn-break purpose, prophylaxis and defensive resources, favourable
simplification (09 §3): later scope proposals.

## 3. Part 1 contract — reply evidence (frozen 2026-10-25)

Rules, report, headers, fixtures and work packages for R5–R13. Terms are those of
`DEFINITIONS.md`; its §3.7 rows are contracted by this section.

### 3.1 Notation and scope

A line `M_1 … M_len` is played from `board_0` (for a candidate, the analysed position, so
`M_1` is the candidate); `board_i` is the board after `M_i`; `n` is the walked plies.
`material_X(b)` is `X`'s material count on `b` (pawn 1, knight 3, bishop 3, rook 5, queen 9)
minus the other side's; a promotion therefore counts through the promoted piece. A
**continuation move** at ply `i` is `M_i` when it is a capture (en passant included), gives
check, or is played from a `board_{i−1}` in check. A board is **game over** when the side to
move has no legal move (checkmate or stalemate) or it has insufficient material (§3.2 P1).

Not in part 1: repetition and the fifty-move rule; root positions with insufficient material
(the root stays terminal only at checkmate or stalemate); positional gains in the reply gain;
every item of parts 2 and 3.

### 3.2 Rules

**P1 Terminal draw (R5).** Insufficient material is the project's rule, equal to
python-chess `Board.is_insufficient_material()`: no pawns, rooks or queens on the board, and
either at most one knight or bishop in total, or only bishops, all on squares of one colour.
(The vendored `chess.hpp` `isInsufficientMaterial` is not used.) The walk stops at the first
game-over board, that ply included. When it stops at a stalemate or insufficient-material
board at ply `k`, the consequence carries `terminal_draw = {kind, ply: k}`; class, rank,
material and `promotes` stay as the walk measured them. Every "game over" of the report
uses this definition: the walk, `creates_threat_skipped = game_over`, the tempo ledger's stop
and the reply search's "the answer ends the game".

**P2 No refutation after a game-over move (R5).** A move whose `board_1` is game over gets no
refutation, whatever its verdict; `refutation_skipped = game_over`, set iff `refutation` is
null and the verdict is not playable. Search R does not run for it.

**P3 Settled walk (R6).** `n = min(6, len)`; then while `n < min(10, len)`, `board_n` is not
game over and `M_{n+1}` is a continuation move: `n += 1`. The walk stops earlier at a
game-over board inside the first six plies. `settled` is false iff `n = 10` and the engine's
PV has an 11th move that is a continuation move from `board_10`; the walk reads the raw PV,
which is longer than the 10-ply report line. `consequence.settled` carries it for every
consequence (moves, threats, refutations, created threats). Motifs, positional changes and
the tempo ledger cover the new walked plies.

**P4 Settle point.** `settle(k) = j`: start at `j = k`; while `j < n` and `M_{j+1}` is a
continuation move, `j += 1`. The settle point is **settled** iff `j < n` or the walk is
settled.

**P5 Reply gain (R8).** For a ply `k ≥ 2` that answers a threatening ply `k−1`, stops its
threat (`stops_threat = true`) and is not a threatening move, with replier `R` (the side of
`M_k`) and `j = settle(k)`:
`reply_gain = {material: material_R(board_j) − material_R(board_{k−2}), mates: board_j is
checkmate with R to have moved, through_ply: j, settled, gains}`, where `gains` is null iff
the settle point is not settled, otherwise `material ≥ 1 or mates`. On no other ply is
`reply_gain` set.

**P6 Forced test (R7).** Only on a ply whose `reply_gain.gains` is false. On `board_{k−1}`
(`R` to move, `legal` legal moves) search `L` with MultiPV `m = min(5, legal)`. A line is
**acceptable** iff its `es_units ≥` line 1's `− 100` (0.05). For each acceptable line's first
move `a`: `stops_threat` is the ply's own when `a = M_k`; true when the board after `a` is game
over; otherwise one reply search (MultiPV 1, `L`, the threatening side to move) and true iff
its consequence rank is `< 2`. Lines that are not acceptable get no reply search
(`stops_threat` null). `forced` is false if an acceptable line does not stop the threat; else
null if all `m` lines are acceptable and `m < legal`; else true. Clarification confirmed by
the Captain at checkpoint 1 (2026-10-25, option A): "unresolved" is narrower than D3's
wording; a reply is unforced as soon as one acceptable alternative fails to stop the threat,
even when all five lines are acceptable. The played reply need not be among the lines (it is
in 71 of 75 of the probe's tested plies).

**P7 Ply labels and tempo gained (R9).** For ply `k` answering a threatening ply `k−1` with
`stops_threat = true`: `counter_threat` if it is a threatening move; else `gaining_reply` if
`gains` is true; `unresolved_reply` if `gains` is null; otherwise (`gains` false) by
`forced`: true `pure_reply`, false `unforced_reply`, null `unresolved_reply`. Other plies keep
the PostM4_EM3 labels (`threatening`, `neither`). `tempo_gained` on ply `k−1` iff ply `k` is a
`pure_reply`; tempi count tempi gained, as before.

**P8 Run endings (R9).** A run starts and grows as in PostM4_EM3 §6.3. From the holder's first
move `e` after the run's last tempo, first match wins: `e > n` → `unfinished`, end `n`;
`e` is not threatening → `spent`, end `e`; `e` mates → `runs`, end `e`; `e + 1 > n` →
`unfinished`, end `e`; ply `e+1` is `counter_threat` → `contested`; `gaining_reply` or
`unforced_reply` → `answered`; `unresolved_reply` → `unresolved`; `stops_threat` false →
`runs`; these four end at `e + 1`.

**P9 Run outcome (R12).** `unfinished` → `unknown`. Otherwise, with `s` the run's start ply,
`E` its end ply and `j = settle(E)`, the run is `converted` iff (a) the settle point is
settled and `material_H(board_j) − material_H(board_{s−1}) ≥ 1`; or (b) a net change in use
that is not dynamic (`!positional::is_dynamic`), made at a ply in `[s, E]`, has the holder as
favoured side; or (c) `board_E` is checkmate by the holder. Otherwise `unknown` when the
settle point is not settled, else `evaporated`.

**P10 Captured weakness (R10).** `PositionalChange.captured` is true iff the change is a
disappearance at ply `k`, `M_k` is a capture, and the captured piece stood (en passant: the
removed pawn) on a square of the feature's pieces. `favours` returns no side for a captured
change whose tag is a weakness: `isolated_pawn`, `doubled_pawns`, `tripled_pawns`,
`backward_pawn`, `king_shield_gap`, `open_file_near_king`, `half_open_file_near_king`,
`king_zone_attacker`, `bad_bishop`. A captured passed pawn still favours the capturer.

**P11 Shelter witness (R11).** In the use test, the king-file tags (`king_shield_gap`,
`open_file_near_king`, `half_open_file_near_king`) use "king pressed on file `f`" where
PostM4_EM3 §6.6 rules 6 and 10 use "king pressed"; `f` is the feature's `file`; the castling
tags and `king_zone_attacker` keep "king pressed"; rule 7 (shelter change at ply ≤ 2 passes)
is unchanged. `Z_i` is the squares of file `f` in `X`'s king zone on `board_{i−1}`. King
pressed on file `f` from ply `k` holds iff some `Y` ply `i` in `k..n` (i) gives check with a
checking piece on a `Z_i` square, or with a bishop, rook or queen that has a `Z_i` square
strictly between it and `X`'s king; or (ii) captures on a `Z_i` square; or (iii) on `board_n`
at least two distinct `Y` pieces other than pawns and the king attack a square of `Z` taken on
`board_n`.

### 3.3 Evidence at freeze

Release build of `main` after the PostM4_EM4 merge, 43 F fixtures, python-chess replay,
30k nodes, Threads=1, Hash 64. P3 changes 88 of 531 lines in 18 fixtures (F04–F10, F15,
F17, F18, F21, F22, F26, F30, F34, F39, F42, F43); 7 root-move lines change material; F07
`Kg1` goes from +3 to −6. The forced test over the 75 plies labelled `pure_reply` today
(old walk): 25 with all five lines acceptable (14 of them all stopping: `unresolved_reply`
under P6; 11 `unforced_reply`), 36 forced, 14 unforced. F40 `Qxd5` ply 3 `Qd8`: 3 acceptable,
all stop, stays `pure_reply`. F41 `Bxa4` ply 7 `Nxc2`: material −3 + 5 = +2, `gaining_reply`.

### 3.4 Report v9

`schema/report.v9.schema.json` and `schema/moves.v9.schema.json` replace v8 (files renamed,
`kSchemaVersion` 9, every reader moves); every new field has a description and if/then rules
for its local relations.

| Field | Type | Rule |
|---|---|---|
| `consequence.terminal_draw` | `{kind: stalemate\|insufficient_material, ply}` or null | P1; `ply = consequence.plies` |
| `consequence.settled` | bool | P3; true when `plies < 10` |
| `refutation_skipped` (candidate, asked move, move report) | `game_over` or null | P2 |
| `tempo_ledger.plies[].reply_gain` | `{material, mates, through_ply, settled, gains}` or null | P5; `gains` null iff `settled` false |
| `tempo_ledger.plies[].forced_test` | `{legal_moves, lines: [{san, es, acceptable, stops_threat}], forced}` or null | P6; set iff `reply_gain.gains` is false |
| `tempo_ledger.plies[].label` | adds `gaining_reply`, `unforced_reply`, `unresolved_reply` | P7 |
| `runs[].ending`, move report `initiative[].ending` | adds `answered`, `unresolved` | P8 |
| `runs[].outcome`, move report `initiative[].outcome` | adds `unknown` | P9 |
| `positional[].captured` (full and move report) | bool | P10; false on appearances |

Sources: `terminal_draw`, `settled`, `refutation_skipped`, `reply_gain`, `captured`, labels,
endings and outcomes `rule`; `forced_test` `engine`.

### 3.5 Headers

```cpp
// include/chesswhy/report.hpp, kSchemaVersion = 9
enum class DrawKind : std::uint8_t { Stalemate, InsufficientMaterial };
struct TerminalDraw { DrawKind kind = DrawKind::Stalemate; int ply = 1; };
// Consequence: + std::optional<TerminalDraw> terminal_draw;  + bool settled = true;
enum class RefutationSkip : std::uint8_t { GameOver };
// MoveAssessment: + std::optional<RefutationSkip> refutation_skipped;
//                 refutation: "set iff verdict is not playable and refutation_skipped is null"
enum class TempoPlyLabel : std::uint8_t {
    Threatening, PureReply, CounterThreat, Neither, GainingReply, UnforcedReply, UnresolvedReply };
struct ReplyGain { int material = 0; bool mates = false; int through_ply = 1; bool settled = true;
                   std::optional<bool> gains; };
struct ReplyLine { std::string san; int es_units = 0; bool acceptable = false;
                   std::optional<bool> stops_threat; };
struct ForcedTest { int legal_moves = 0; std::vector<ReplyLine> lines; std::optional<bool> forced; };
// TempoPly: + std::optional<ReplyGain> reply_gain;  + std::optional<ForcedTest> forced_test;
enum class InitiativeEnding : std::uint8_t { Spent, Contested, Runs, Unfinished, Answered, Unresolved };
enum class InitiativeOutcome : std::uint8_t { Converted, Evaporated, Unknown };
// PositionalChange: + bool captured = false;

// include/chesswhy/analysis.hpp; walked_plies() and kCaptureExtension are removed
struct Walk { int plies = 0; bool settled = true; std::optional<TerminalDraw> terminal_draw; };
[[nodiscard]] bool insufficient_material(const chess::Board& board);        // P1
[[nodiscard]] bool game_over(const chess::Board& board);                    // §3.1
[[nodiscard]] bool continuation_move(const chess::Board& before, chess::Move move); // §3.1
[[nodiscard]] Walk walk(const chess::Board& root, std::span<const std::string> pv_uci); // P1, P3
[[nodiscard]] int settle_ply(const chess::Board& root, std::span<const chess::Move> line,
                             int k); // P4; line = the walked moves
// label_ledger and initiative_runs read reply_gain, forced_test and the walk's settled flag.

// include/chesswhy/move_report.hpp
// MoveReport: + std::optional<RefutationSkip> refutation_skipped;
```

`positional.hpp` keeps its signatures: `line_changes` sets `captured`, `favours` reads it,
`mark_use` applies P11.

### 3.6 Fixtures

| Id | Input | Expected |
|---|---|---|
| F45 (new) | `8/8/8/8/6q1/6R1/5k2/7K b - - 0 1 --moves Qxg3` | `Qxg3`: `walked_plies` 1, `terminal_draw` `{stalemate, 1}`, material 5, `is_tactic` true, `refutation` null, `refutation_skipped` `game_over`, `creates_threat_skipped` `game_over` |
| F21 (re-bless) | as today | `Nxf6+`: `terminal_draw` `{insufficient_material, 1}`, `walked_plies` 1, material 9, `is_tactic` true |
| F46 (new) | `3r2k1/8/2n5/8/3P4/2N5/8/R5K1 b - - 0 1 --moves Rxd4` | `Rxd4`: `isolated_pawn` (White, d) disappears at ply 1 with `captured` true and favours nobody; Black's run, if the pinned engine line keeps it, `unknown` when unfinished |
| F41 (re-bless) | as today | `Bxa4` ply 7 `Nxc2` `gaining_reply` (`reply_gain.material` 2, `through_ply` 7); `tempo_gained` `[]`, `initiative` `[]` |
| F40 (re-bless) | as today | `Qxd5` ply 3 `Qd8` `pure_reply` with `forced_test.forced` true; ply 2 `Nc3` tempo gained; run White 2–4 `spent`; one candidate ply pinned `unforced_reply` and, in F28 or another fixture, one `unresolved_reply` (WP5 picks from the built report) |
| F07 (re-bless) | as today | `Kg1` line walked through the check sequence, material as the settled walk gives |
| PU19 (new) | `6k1/8/8/8/1n6/8/4BP2/4K2R w K - 0 1`, line `f4 Nd3+ Bxd3 Kf7 Bc4+ Ke7` | White's f-file shelter change is not in use: `Nd3+` stands outside `Z` |
| PU20 (new) | `q5k1/8/8/8/8/8/5PPP/6K1 w - - 0 1`, line `h3 Kh7 f4 Qa7+ Kh2` (WP2 may adjust the board so that the change appears at ply 3) | the f-file shelter change at ply 3 is in use: `Qa7+` checks through `f2` |
| Unit | `6rk/p5p1/8/8/2B5/8/PB6/K3R3 w - - 0 1`, line `a3 a6 a4 a5 Bxg7+ Kxg7 Rg1+ Kh6 Rxg8` | `walk`: plies 9, settled, material +3 for White (`analysis_unit`) |

Every other fixture whose pinned fact changes under P1–P11 is re-blessed by WP5 and listed
with old and new value for checkpoint 2; a change no rule explains is a bug, not a re-bless.

### 3.7 Full report text

- The walk row: `facts read to ply N of M` gains ` (stalemate)`, ` (insufficient material)`
  or ` (not settled)`.
- Wherever a consequence summary is printed, a terminal draw adds `; stalemate at ply k` or
  `; insufficient material at ply k`.
- A skipped refutation prints `Refuted by: none, the move ends the game [rule]`.
- Ledger labels print `gaining reply`, `unforced reply`, `unresolved reply`; a gaining reply
  adds ` (+N)` with its material.
- Initiative rows: ending words `answered` and `unresolved`; the outcome `unknown` prints
  `outcome unknown`.

### 3.8 Move report text (R13)

The tempo group is no longer skipped in the position walk (PostM4_EM4 §2.5). Rows, inside the
position blocks, after the `Move:` row and before the `Uses:` rows:

- Block 0: `Tempo value: …` from the root (`position.tempo_value`), EM3 template.
- Block 1: `Tempo value: …` from the move's `tempo_value`, same template.
- Block `j − 1` for each tempo gained at ply `j`: `Tempo: <Side> gains a tempo [engine]`.
- Block `s − 1` for each initiative run starting at ply `s`: the `Initiative:` row of §3.7.

Expected V2 (F41) `Bxa4` after part 1: block 0 adds `Tempo value: 29.6% for Black
(tempo-dependent position) [engine]`, block 1 adds `Tempo value: 24.8% for White
(tempo-dependent position) [engine]`; no tempo or initiative row. F40 `Qxd5`: block 1 adds
`Tempo value: 17.3% for White (tempo-dependent position) [engine]`, `Tempo: White gains a
tempo [engine]` and `Initiative: White from ply 2, 1 tempo; spent at ply 4; evaporated
[rule]`.

### 3.9 Work packages

| WP | Owns | Builds | Done when |
|---|---|---|---|
| WP1 analysis | `include/chesswhy/report.hpp`, `include/chesswhy/analysis.hpp`, `src/analysis/*`, `src/report/report.cpp`, `schema/report.v9.schema.json`, `tests/analysis/*`, `tests/report/report_unit_tests.cpp`, `tests/report/example_reports.hpp`; v9 version-only edits anywhere | P1–P9, report v9 JSON | headers of §3.5 committed first; unit tests for the walk line of §3.6, insufficient material cases, P4–P9 on hand-built ledgers; `analysis_unit`, `report_unit` pass |
| WP2 positional | `include/chesswhy/positional.hpp`, `src/positional/*`, `tests/positional/*`, `tests/fixtures/positional.json` | P10, P11, PU19, PU20 | starts on WP1's header commit; PU05, PU08–PU10 still kept; positional tests pass |
| WP3 move report and text | `include/chesswhy/move_report.hpp`, `src/report/move_report.cpp`, `schema/moves.v9.schema.json`, `tests/report/move_report_tests.cpp`, `src/render/*`, `tests/render/*` and goldens | §3.7, §3.8, `refutation_skipped` in the move report | render and move report tests pass; goldens changed only where §3.7–§3.8 say |
| WP4 checker | `tools/validate_report.py`, `tests/checker/*` | P1–P11 re-implemented independently, mutants for each | checker unit tests pass; every F fixture's report checks |
| WP5 fixtures | `tests/fixtures/positions.json`, `tests/cli/*` | §3.6 | engine fixtures pass; the re-bless list for checkpoint 2 |
| WP6 docs | `README.md` | the new fields and rows | — |

Batches: WP1 + WP2; then WP3 + WP4; then WP5; WP6 with the milestone-end gate.

### 3.10 Checkpoints

1. Before spawning: the Captain sees the changed diagrams, the P6 clarification and the §3.8
   sample.
2. After WP5: the re-bless list (old and new value per fixture fact) and the timing of F12
   and F32. Any F fixture above 15 s (or F32 `--moves playable` above 30 s) goes to the
   Captain; budgets never change silently.

### 3.11 Acceptance

- [ ] F45, F46, PU19, PU20 and the re-blessed F21, F40, F41, F07 pass with the §3.6 facts.
- [ ] No refutation is built from a game-over board; `judge_refutation` never throws on F45.
- [ ] Every `pure_reply` has `forced_test.forced` true and `reply_gain.gains` false; no
      `tempo_gained` without a pure reply (checker).
- [ ] The checker re-derives `walked_plies`, `settled` (when `plies < 10`), `terminal_draw`,
      `reply_gain`, labels from `forced_test`, endings, outcomes, `captured` and the shelter
      witness, with a mutant per rule.
- [ ] The move report text prints the §3.8 rows; V2 and F40 match §3.8.
- [ ] Gates of AGENTS.md (batch and milestone end); `cli_timing` within limits.
- [ ] `DEFINITIONS.md`, `CONCEPTS.md`, `CODE.md`, `FIXTURES.md`, `TESTS.md` current;
      `check_dbc` passes.

## 4. Contract changes

| Date | Change | By |
|---|---|---|
| 2026-10-25 | Checkpoint 1: diagrams, §3.8 sample and P6 option A (unresolved only when all searched lines are acceptable and stop the threat) approved; batch 1 spawned | Captain |
