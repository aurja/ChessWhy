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
Updated 2026-10-22 with the PostM3_EM2 plan (G1–G15) and the drift the conceptual audit
of that day found; rewritten rows name their old meaning.

## 1. Words that mean several things

Read these first. Each word is used in two or more senses in the plans.

| Word | Senses in this project |
|---|---|
| target | (1) **Motif target**: a piece the motif acts on, e.g. the king and queen in a fork (§4.3). (2) **Metric target**: the frozen minimum value a measured number must reach for a test to pass, e.g. "recall target 0.95" (§5.6) |
| precision | Used in four senses over time. (1) Until 2026-10-19: share of our puzzle predictions that Lichess also named, now called **Lichess agreement** (§5.4). (2) 2026-10-19 to 2026-10-20: **engine-confirmed precision**, share of reported motifs that Stockfish turns into a material win (§5.5); it measured whether a tactic *works*, not whether the pattern is *on the board*, and left M2 with the motif/tactic split. (3) Since 2026-10-20, for motifs: **motif recognition**, exact agreement with the definitions, checked against the reference implementation (§5.7). (4) The everyday sense, "how often a reported fact is right". The plans avoid the bare word |
| line | A sequence of moves from a stated position. Engine lines are Stockfish's principal variations; a puzzle's line is its solution moves (§3.1) |
| threat | Not the everyday "something attacks something". A threat is the line a side would play **if it were its move**, taken from an engine search, with its consequence (§3.3) |
| gain | (1) A motif's `gain` field: an SEE value in pawns, only on `hanging_piece` and `trapped_piece` (§4.3). (2) The `material` of a consequence (§3.3). (3) A tactic's gain: its **only-move margin**, how much better in ES its line is than the best alternative move (§4.5). (4) "Hit-no-gain" in the former engine confirmation (§5.5) |
| forced | A **forced** move is the only legal move of its position: a rule fact in the report, never judged as a tactic or, from M4, an opportunity (§4.5). Not the loose chess sense "the only good move"; that is an opportunity or a lone playable move |
| fine | Retired in M4 (Captain, 2026-10-23): a verdict of M1–PostM3_EM2 meaning loss ≤ 0.03 expected score against the best move (§3.5). From M4 the playable bucket (best, okay; loss ≤ 0.05) replaces it. Never "good" or "best" |
| hanging | Our `hanging_piece` is an SEE rule (X wins at least 3 pawns on the square), not the everyday "undefended" (§4.4) |
| side | The side a fact is about. In analysis: `S` side to move, `O` opponent. In motifs: `X` the side that benefits, `Y` the other (§2) |
| frozen | Changed only through a contract change approved by the Captain (§6.1) |
| gate | A check that must pass for a batch or milestone to be accepted (§6.3) |
| level | (1) **Audience level**: beginner, club or advanced, chosen by the user; changes only the wording of the explanation, never the analysis (§3.1). (2) **Diagram level**: L1, L2 or L3 in CONCEPTS.md (§7) |
| tempo | One move, the unit of time. Senses decided for PostM4_EM3 (Captain, 2026-10-24; §3.6, not yet built): (1) **tempo gained**: a threatening move answered by a pure reply, so its side has made one more move of its own than the other side. (2) **Tempo value**: what having the move is worth in a position, the loss of passing; negative in zugzwang, which covers the endgame sense of a spare tempo (opposition, triangulation). Not computed: the **development tempo** of opening theory (moves spent on development; `development_lead` is the nearest built fact) |

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
| report | The JSON document one query produces; everything the tool claims is a field of it. Versioned by `schema_version` (2 at M2; 3 from PostM2_EM1, adding `tactic` and `engine.nodes.R`; 4 from M3a/M3b, adding positional features and changes and `games`; 5 from PostM3_EM2, adding `legal_moves`, `depth`, `walked_plies`, `creates_threat_skipped` and `restriction`; 6 from M4: the new verdict scale, `opportunity` replacing `tactic`, `is_tactic`, `allows_tactic`, `detailed`, `root_motifs`, `tactic_labels`, `input.selection`, `asked_moves`, `playable_truncated`, `document`) with a JSON Schema in `schema/`. From M4 it comes in two documents: the **full report** (`document: "full"`) and the **move report document** (planned) | vision §6, M2 §5, PostM2_EM1 §5, PostM3_EM2 §5, M4 §5 |
| provenance | Every fact's source: `engine` (Stockfish search), `rule` (deterministic board rule), `book` (reference material, opening names), `games` (counts from game collections). Verdicts and losses are `engine`: engine scores read through frozen thresholds (the cut-offs are ChessWhy's) | vision §2 |
| facts first | Chess content is computed in C++ and stored in the report; the language model (M5) only puts report fields into words | vision §2 |
| budget | `quick`, `standard` (default) or `deep`: fixed node counts per search kind | M1 §3 |
| reproducible | Threads = 1, Hash 64 MB, fixed nodes, `ucinewgame` before each search: the same input gives a byte-identical report. Means the same configuration only: the report does not identify the engine binary or the ChessWhy revision (stated 2026-10-22) | M1 §3, PostM3_EM2 G11 |
| rerun identity | The acceptance check that two fresh runs give byte-identical JSON | M1 §14 |
| asked move | A move the user names in the `--moves` list: the user's "why (not) this move?". From M4 (Captain, 2026-10-23) a request may name several; before, the single move given with `--move`. An asked move among A's lines is that line; one outside A is searched by C and listed in the full report's `asked_moves`, in typed order (before M4, the single asked move was copied into `asked_move`) | M1 §4.7, M4 §4.1, §5 |
| candidates | A's K best moves with their verdicts, in A's MultiPV order (not re-sorted by score). An internal analysis term: from M4 the output never calls a move a "candidate", because chess literature uses the word for moves *considered*, many of them rejected; the output shows **playable moves** instead (Captain, 2026-10-23) | M1 §4.7, PostM3_EM2 G9 |
| playable move | From M4 (Captain, 2026-10-23): a move the engine judges worth playing: loss ≤ 0.05 ES against the best move, i.e. verdict best or okay (see verdict). Not a claim about which moves a human would consider | — |
| `--playable` | Retired before it was built (Captain, 2026-10-23): `--moves` replaces it. Was: how many playable moves to report (`N` or `all`), an input error together with `--move` | M4 §1 P2, §15 |
| `--moves` | From M4 (Captain, 2026-10-23): the one flag that names the moves to report; replaces `--move` and the never-built `--playable`. Its value is a comma-separated list without spaces; each item is an asked move (SAN or UCI) or a keyword: `best` (A's line 1 only, even when other moves tie), `topN` (A's first N lines whatever their verdict, e.g. `top3`), `playable`, `inaccuracies`, `mistakes`, `blunders`, `unplayable` (inaccuracies, mistakes and blunders), `all` (every legal move). Items combine as a union, each move reported once. No flag means `--moves best`. `--help` warns that every whole-board keyword is slow | M4 §1 P2, §4.1 |
| whole-board keyword | From M4 (2026-10-23): a `--moves` keyword that needs a verdict for every legal move: `playable`, `inaccuracies`, `mistakes`, `blunders`, `unplayable`, `all`. Any of them sets A's MultiPV K to the number of legal moves, so the run takes several times the default time | M4 §4.1 |
| selection | From M4 (2026-10-23): the moves that get a move report: the union of what each `--moves` item selects, in A's MultiPV order, then asked moves outside A in the order typed. A keyword that matches no move selects nothing. Recorded as `input.selection` (`keywords`, the largest `top`, asked `moves` in UCI). Until the `--moves` revision of 2026-10-23 the plan was one request: best, top N playable, all playable, or one asked move | M4 §4.1 |
| detailed line | From M4 (2026-10-23): an A line that gets the per-move searches D, E and R: A's first five lines and every selected move. Other lines (`detailed: false`) keep verdict, line, consequence, motifs and the refutation line, without parry, created threat or the refutation's opportunity judgement | M4 §4.1, R4 |
| move report | From M4 (Captain, 2026-10-23): the output for one selected move: its verdict, line, consequence, tactic and opportunity, refutation, and only the parried threat, created threat, motifs, root motifs and positional changes the relevance filter keeps, each with its provenance; the JSON and text outputs are built from move reports | M4 §1 P8, §4.6 |
| relevance filter | From M4 (Captain, 2026-10-23, D3): the pure rule that turns one move of the full report into a move report. Keeps verdict, line and refutation; `O`'s threat only when the move parries it; a created threat only when its class is mate, material or promotion; line and root motifs only with contribution direct or indirect; positional facts only as the net change over the walk (changes that hold at the end, the last per feature). Drops other moves, `S`'s own threat, urgency and threat-line motifs | M4 §4.6 |
| move report document | From M4 (2026-10-23): the move reports as JSON, printed by `--json` (`document: "moves"`, schema `moves.v6.schema.json`): input, engine, position summary, selection with lines searched, playable count and `playable_truncated`, opening, game source, and `moves`, one move report per selected move | M4 §4.6, §5 |
| full report | From M4 (2026-10-23): the report with every computed fact (`document: "full"`, schema `report.v6.schema.json`); `--full-report` prints it as text (full report text), `--json --full-report` as JSON, which the fixtures and the semantic checker read. Called simply "report" before M4 | M4 §1 P9, §5 |
| `--full-report` | From M4 (revised by the Captain 2026-10-23): the content flag: print every computed fact (the full report) instead of the move reports. Independent of the format flag `--json`: alone it prints the full report text, with `--json` the full report as JSON. Before the revision it meant the full report as JSON, and with `--json` an input error | M4 §1 P9, §4.1 |
| full report text | From M4 (2026-10-23): the human-readable form of the full report, printed by `--full-report` without `--json`: the template rendering of the whole report on the v6 scale, the A lines headed `Engine lines`; stays template text when model prose arrives | M4 §10.2 |
| projection check | From M4 (2026-10-23): a mode of the semantic checker (`validate_report.py --projection FULL MOVES SCHEMA`): given a full report and the move report document of the same position and `--moves`, every move report fact equals the full-report field it came from, with the relevance filter re-implemented independently | M4 §1 R7 |
| line depth | The search depth the engine reached on a candidate's line (or on search C for an asked move outside A); JSON `MoveAssessment.depth`, `engine` provenance. Lines of one MultiPV search can end at different depths | PostM3_EM2 §5 |
| `legal_moves` | The number of legal moves in the analysed position (`position.legal_moves`, `rule`), printed beside the candidate count so the reader sees how many moves were not judged | PostM3_EM2 §5 |
| level | `beginner`, `club`, `advanced`: audience of the M5 prose; no effect on analysis | vision §3 |
| command line | `chesswhy` with flags (`--fen` or `--pgn` with `--game`/`--ply`, `--budget`, `--level`, `--json`, `--no-llm`; `--move` until M4, then `--moves` and `--full-report`): how a question arrives today | M1 §6, M4 §4.1 |
| native apps | Planned presentation layer: apps on Linux and Windows, possibly Android, so players need no technical setup; design and scope open with the Captain (M7; M6 before the 2026-10-23 renumbering). Called "local HTTP API" before 2026-10-22, when it meant the CLI inputs as JSON over HTTP on `127.0.0.1`; that design is dropped | vision §9 |
| analysed position | The one position a report is about: the FEN, or the PGN game's position after `--ply` half-moves (default: its final position). Position-only: the moves that led to it are not an input to the searches or rules, so repetitions in the game do not change a verdict | M1 §5, PostM3_EM2 G15 |
| game mainline | For PGN input: the game's positions from its start to the analysed position, used only to find the last named position | M1 §4.8, §5 |
| input error | Input that cannot be used (bad flag, invalid FEN, illegal or ambiguous move, missing game or ply): one `chesswhy: error:` line, exit 2, no report | M1 §5, §6 |
| fact | One claim about the position, stored as a report field with its provenance before any words are written | vision §2 |
| report schema | The versioned JSON Schema in `schema/` that every report must satisfy; `schema_version` names it. From v5: a description on every field and if/then rules for local relations. From v6 (M4, 2026-10-23): two schemas of one version, `report.v6.schema.json` (full report) and `moves.v6.schema.json` (move report document) | vision §6, PostM3_EM2 §5, M4 §1 R6 |
| semantic check | Checks of relations between report fields that a schema cannot express, run by `tools/validate_report.py` with python-chess after the schema check (verdict against loss, side to move against the FEN, legal lines, the asked-move copy, tactic invariants, restricted moves and others; 14 checks). M4 adds the v6 verdict, tactic and opportunity relations and the projection check | PostM3_EM2 §4.6, M4 §9 |
| JSON output | The report printed as JSON (`--json`), for programs and for checking; without `--json` the output is text. From M4 (2026-10-23) `--json` chooses only the format: the move report document, or with `--full-report` the full report | M1 §6, M4 §1 P9 |

### 3.2 Searches A–E, R

| Search | From | Answers |
|---|---|---|
| A | current position, MultiPV K (5; from M4 more for `topN` and every legal move for a whole-board keyword) | best moves, `S`'s threat, `eval_white` |
| B | null move (`O` to move) | `O`'s threat and its first move `t` |
| C | current position, `searchmoves m` | each asked move that is not among A's lines |
| D(m) | after `m`, `searchmoves t` | does `m` parry `O`'s threat? |
| E(m) | after `m`, null move | which threat `m` creates |
| L | from PostM4_EM3 (contracted 2026-10-24): the tempo ledger searches, MultiPV 1 each: the threat search after each ply from ply 2 on (null move) and the reply search after the answer to each threatening move; node budget `L` (40k standard, 10k quick, 160k deep) | which plies are threatening moves and which replies stop the threat (§3.6) |
| R(m) | after a move `m` that is not fine, `searchmoves` all of `O`'s legal moves except the refutation's first | `O`'s best alternative to the refutation, for the refutation's tactic judgement (§4.5) |

Node counts per budget: M1 §3; R (same size as D and E): PostM2_EM1 §3. Order: A, B, per
candidate D, E, R; then C, D, E, R for each asked move outside A.

### 3.3 Threats and consequences

| Term | Meaning | Defined in |
|---|---|---|
| threat | The line a side would play if it were its move: `S`'s threat is A's best line, `O`'s threat is B's line. "White's biggest problem" is Black's threat | vision §5.2, M1 §4.2 |
| consequence | What a line achieves for the side playing it, computed by rule from the engine line: `class`, `mate_in`, `material`, `promotes`, `plies` | M1 §4.3 |
| walked plies | How far the consequence rule follows a line: 6 plies, then on while the next move is a capture, up to 10. `walked_plies` in `analysis.hpp`. Motifs and positional changes of a line cover the same plies | M1 §4.3, M2 §6 |
| `walked_plies` | The walked plies of a candidate's or the asked move's line, as a report field (`MoveAssessment.walked_plies`, `rule`), so the text can say up to which ply facts were read | PostM3_EM2 §4.4 |
| consequence class | `mate` (engine mate score for the side), else `material` (net gain ≥ 1 pawn over the walked plies), else `promotion`, else `positional` | M1 §4.3 |
| rank | Class as a number: mate 4, material ≥ 3 → 3, material ≥ 1 → 2, promotion 1, positional 0 | M1 §4.3 |
| `null_move_reliable` | False when `S` has at most one piece besides king and pawns (zugzwang-prone: passing would be an advantage, so B misleads); null when `S` is in check | M1 §4.4 |
| parry test | Whether `m` stops `O`'s threat: by rule when `t` becomes illegal, otherwise by search D (the threat still works iff `O`'s ES stays within 0.03 of B's). "Parries" means the threat no longer wins against the pass baseline B, not that it was prevented; a move that gives check only defers the threat (`deferred_by_check`) | M1 §4.5, PostM3_EM2 G2 |
| `parries_threat` | Result of the parry test for one move; null when `O`'s threat has rank < 2 or `S` is in check (no threat B) | M1 §4.5 |
| `creates_threat` | The threat `S` would have after `m` if `O` passed (search E, a premise). Null when `m` gives check, `m` ends the game, or `S` has no legal move after `O`'s null move (before 2026-10-22 this row named only the check) | M1 §4.7 |
| `creates_threat_skipped` | Why `creates_threat` is null: `gives_check`, `game_over` or `no_legal_move`; null iff `creates_threat` is set; `rule` | PostM3_EM2 §4.4 |
| conditions | What a fact depends on, printed beside it by the template: a threat line's null-move premise ("if White passes"), a parry deferred by check, the reason a created threat was not searched, the walked plies, the engine and budget, and how many legal moves were not judged | PostM3_EM2 G2, §10 |
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
| verdict | From M4 (Captain, 2026-10-23): two buckets. *Playable* (loss ≤ 0.05): **best** = the engine's top line, and every other A line with exactly the same engine score (same centipawns or same mate distance, e.g. two mates in 1); **okay** = any other playable move. *Not playable* (loss > 0.05): **inaccuracy** = 0.05 < loss ≤ 0.10; **blunder** = loss > 0.10 and the move allows a tactic; **mistake** = loss > 0.10 otherwise. An asked move outside A is at most okay (R2). Before 2026-10-23: fine ≤ 0.03 < inaccuracy < 0.13 ≤ mistake < 0.27 ≤ blunder, from the loss alone (loss in ES, scaled to Stockfish 19) | M1 §4.7, M4 §4.2 |
| allows a tactic | From M4 (R1, 2026-10-23): along the move's own line (the move, then best play, from the analysed position, over its walked plies) `S` loses at least 1 pawn net, or the engine scores a mate against `S`. Counted from before the move, so an even trade (`Nxd5 exd5`) allows nothing. The refutation of such a move is a tactic. Reported as `allows_tactic` | M4 §4.3 |
| fine set | Retired in M4 (Captain, 2026-10-23); the playable moves among A's lines replace it. Was: the fine moves among the candidates; ChessWhy judges the engine's top K moves only, so unlisted moves are not judged and may also be fine. Before 2026-10-22: "all moves with verdict fine", which the tool never computed | vision §3, §5.4, PostM3_EM2 G8 |
| `fine_truncated` | Renamed `playable_truncated` in M4 (2026-10-23). Was: true iff all K of A's lines are fine, so more fine moves may exist than are listed. False does not mean the unlisted moves are not fine | M1 §4.7, PostM3_EM2 §5 |
| `playable_truncated` | From M4 (2026-10-23): true iff A returned all K lines, K is less than the legal moves, and every line is playable, so more playable moves may exist than A listed. Always false with a whole-board keyword | M4 §4.1 |

### 3.6 Tempo and impact (PostM4_EM3)

Proposed by the Deputy on 2026-10-24 at the PostM4_EM3 kickoff (tempo value, tempo-dependent position, counter-threat and initiative added the same day after reading three of the Captain's books). The tempo and initiative terms were decided by the Captain the same day (PostM4_EM3 §1, A1–A5); the impact rating terms were decided the same day after reading four more of her books (B1–B3). The rows marked "Contracted" were made exact in the contract draft (PostM4_EM3 §6) on 2026-10-24 and wait for the Captain's approval at checkpoint 1. None is built yet.

| Term | Meaning | Defined in |
|---|---|---|
| threatening move | Decided (A1, A2). A move after which its side's threat (the opponent passes, as in search E) has rank ≥ 2, read over the walked plies from the board after the move; a check is a threatening move by rule, since the opponent cannot pass. A capture counts only for what it threatens next, not for the piece it took, so an even trade is not a threatening move | PostM4_EM3 §1 A1, A2 |
| pure reply | Decided (A1). The move after a threatening move when, after it, the threatening side's best line no longer reaches rank ≥ 2, and it is not itself a threatening move | PostM4_EM3 §1 A1 |
| tempo gained | Decided (A1). A threatening move whose next move is a pure reply. A reply that stops the threat and threatens back is a counter-threat and returns the tempo; a reply that does not stop it lets the threat run (a tactic, not a tempo) | PostM4_EM3 §1 A1 |
| tempo ledger | Decided (A3). For one line: each move marked threatening, pure reply, counter-threat or neither, and per side the count of tempi gained. Covers every walked ply of every detailed line, from a threat search on every ply and a reply search after each threatening move only (Captain, 2026-10-24); not built when `null_move_reliable` is false | PostM4_EM3 §1 A3, §2 |
| threat search | Contracted (2026-10-24; proposed the same day). The ledger search (L) on the board after ply `k ≥ 2` of a detailed line with the opponent passing, i.e. the mover to move again; the ply is a threatening move iff the consequence rank of its line is ≥ 2. On ply 1 search E answers it; a ply that gives check needs no search | PostM4_EM3 §6.2 |
| reply search | Contracted (2026-10-24; proposed the same day). The ledger search (L) on the board after the answer to a threatening move, the threatening side to move; the answer stops the threat iff the consequence rank of that line is < 2, or the answer ends the game. Runs only after a threatening move | PostM4_EM3 §6.2 |
| ply label | Contracted (2026-10-24). The label of one ply in the tempo ledger: **threatening**, **pure reply**, **counter-threat** or **neither**. A ply that answers a threatening move and stops it is a pure reply, or a counter-threat when it is itself a threatening move; any other ply is threatening or neither. Ply 1 answers nothing | PostM4_EM3 §6.2 |
| `tempo_ledger_skipped` | Contracted (2026-10-24). Why a move has no tempo ledger, set iff `tempo_ledger` is null: `in_check` (`S` is in check), `null_move_unreliable` (`null_move_reliable` is false), `not_detailed` (the line is not detailed) | PostM4_EM3 §6.2 |
| counter-threat | Decided (A5). A reply to a threatening move that stops the threat and is itself a threatening move. It returns the tempo; a run of counter-threats is a struggle for the initiative | PostM4_EM3 §1 A5 |
| initiative | Decided (A5). A side holds the initiative over the plies of a line where it gains tempi one after another: each of its moves is a threatening move and each reply a pure reply. It ends when the holder makes a move that does not threaten (**spent**), the other side answers with a counter-threat (**contested**), or the reply does not stop the threat, so the line turns into a tactic (**runs**; proposed by the Deputy and accepted by the Captain, 2026-10-24). At its end it is **converted** when the holder keeps a gain at the line's end (material, or a positional fact that holds at the end) and **evaporated** otherwise. Derived from the tempo ledger, no extra search. Made exact as initiative run, run ending, converted and evaporated (contracted 2026-10-24) | PostM4_EM3 §1 A5 |
| initiative run | Contracted (2026-10-24). One stretch of initiative in a tempo ledger (`InitiativeRun`, `rule`): holder, first and last ply, tempi gained by the holder, run ending and outcome. It starts at a tempo gained that lies in no earlier run and goes on while the holder's next move gains a tempo too; the next run may start at the ply where one ends | PostM4_EM3 §6.3 |
| run ending | Contracted (2026-10-24). How an initiative run ends, from the holder's first move `e` after its last tempo: **spent** (`e` is not a threatening move), **runs** (`e` mates, or the answer to `e` does not stop the threat), **contested** (the answer to `e` is a counter-threat) or **unfinished** (new, proposed for checkpoint 1: the walked plies end before `e` or its answer, so the ending is unknown) | PostM4_EM3 §6.3 |
| converted, evaporated | Contracted (2026-10-24), refining the initiative row for checkpoint 1. A run is **converted** iff the holder's material count (pawn 1, knight 3, bishop 3, rook 5, queen 9) minus the other side's grew by at least 1 from the board before the run's first ply to the end of the tempo ledger, or a net change in use made at a ply of the run has the holder as favoured side; **evaporated** otherwise | PostM4_EM3 §6.3 |
| tempo value | Decided (A4). For the side to move `T`: `ES_T(T moves) − ES_T(T passes)`, the loss of passing on the verdict scale (§3.5). Reported at the root (from searches A and B) and after each candidate `m` (from A's line and search E); not along lines, where the reply search runs only after threatening moves. Null when `T` is in check | PostM4_EM3 §1 A4, §2 |
| tempo-dependent position | Decided (A4). A position whose tempo value exceeds 0.10, so passing would be a mistake: every tempo counts and threats decide. Below 0.05 it is a non-tempo position, where time matters little; below 0 it is zugzwang: `T` would rather pass. From 0.05 to 0.10 the position has no label (contracted 2026-10-24). When the root's tempo value is null, the position counts as not tempo-dependent | PostM4_EM3 §1 A4, §6.1 |
| non-tempo position | Decided (A4). A position whose tempo value is at least 0 and below 0.05 (`non_tempo`) | PostM4_EM3 §1 A4, §6.1 |
| zugzwang | Decided (A4). A position whose tempo value is below 0: the side to move would rather pass (`zugzwang`). Includes the endgame sense of a spare tempo (opposition, triangulation) | PostM4_EM3 §1 A4, §6.1 |
| impact rating | Decided (B1–B3, 2026-10-24). The order and cut of the facts of a move report for the text and the fact sheet: first the rank (§3.3) of the consequence the fact bears on, then, among positional facts, the place on the static scale, dynamic facts ahead of static facts in a tempo-dependent position and static facts first otherwise; positional changes that fail the use test leave the move report, and the full report keeps them. The core claims (verdict, line, refutation, conditions) are not rated; they always come first. Proposed before the second reading as the consequence rank alone. Made exact as `impact_order` (contracted 2026-10-24) | PostM4_EM3 §1 B1–B3, §6.7 |
| `impact_order` | Contracted (2026-10-24). On a move report: the rated fact groups that are not empty, in the order the text prints them: parried threat, created threat, motifs, tempo, dynamic positional facts, static positional facts, sorted by the rank each bears on (parried and created threat: the threat's rank; motifs: the move's consequence rank; the rest 0), ties in the base order, which puts tempo after the motifs and dynamic before static facts in a tempo-dependent position and tempo last and static before dynamic otherwise | PostM4_EM3 §6.7 |
| break cost | Open. For a restricted-movement motif, measured by the engine on the board with the held side to move: the best score over the moves that are not restricted moves minus the best score over the restricted moves. Zero or less means the restriction costs the held side nothing | proposal 2026-10-24 |
| static scale | Decided (B1). The order of importance of positional facts, from a book's "regressive scale": king safety, then material (including the bishop pair and the exchange), then pawn structure, then pieces (rook files, seventh rank, outpost, bad bishop), then space, mobility and development. Used only to order facts of equal consequence rank | PostM4_EM3 §1 B1 |
| static fact, dynamic fact | Decided (B1). A static fact lasts until the structure changes (pawn structure, material, bishop pair, a damaged king position); a dynamic fact fades with every tempo unless it is used (threat, tempo, initiative, development lead, mobility lead, a king attacked in its zone, castling rights). In a tempo-dependent position dynamic facts come first, otherwise static facts do | PostM4_EM3 §1 B1 |
| use test | Decided (B2). The rule that keeps a positional change in the move report: the move or its reply (plies 1–2) makes the change, or a later move of the walked line uses it (attacks or captures a weak pawn, advances a passed pawn, moves a rook along its file, checks or captures in a weakened king zone). Leads count only in a tempo-dependent position; pawn islands and pawn chains stay in the full report only. Material, bishop pair, exchange, rook on the seventh, outpost and bad bishop always pass. Exact rules per tag: the PostM4_EM3 contract §6.6; it adds three rules for checkpoint 1: no change passes when the walk ends in mate, a superseded change fails, and a king-file change of a king that can still castle passes only when the king is pressed | PostM4_EM3 §1 B2, §6.6 |
| passing change | Decided (B3). A positional change that the relevance filter kept up to M4 although the root and the end of the walk agree: the feature appears and disappears inside the walk (or the reverse), and its last change holds at the end. Example: F25 `Bxc6` reports `material_ahead` (White) "disappears" after `dxc6`, though White was never ahead at the root. From PostM4_EM3 the filter compares root with end, so passing changes leave the move report | PostM4_EM3 §1 B3 |
| net change | Contracted (2026-10-24). The last change of a feature identity along a walk when the feature is there at the root and not at the end of the walk, or the reverse; every other change is a passing change | PostM4_EM3 §6.5 |
| `net_change`, `in_use` | Contracted (2026-10-24). Fields on every positional change of the full report: `net_change` is true iff it is a net change; `in_use` is set iff `net_change` is true, and true iff the change passes the use test. The move report keeps exactly the changes with both true | PostM4_EM3 §6.5, §6.6 |
| superseded change | Contracted (2026-10-24). A net change that only records something else: castling rights lost by castling, `material_ahead` that disappeared while the material count is not level at the end of the walk, a king zone attacker that left, or a king-file fact of a king that stands on another square at the end. Fails the use test | PostM4_EM3 §6.6 |
| king pressed | Contracted (2026-10-24). For a positional change at ply `k` owned by side `X`: from ply `k` to the end of the walk the other side gives check or captures on a square of `X`'s king zone (the king's square and the squares next to it, before that move), or at the end at least two of its pieces other than pawns and the king attack that zone | PostM4_EM3 §6.6 |
| favoured side | Contracted (2026-10-24). The side a positional change helps. An appearing isolated, doubled, tripled or backward pawn, bad bishop, gap or open or half-open file next to the king or king zone attacker helps the other side; any other appearing feature helps its owner; a disappearance helps the side the appearance would not; `pawn_islands` and `pawn_chain` help nobody | PostM4_EM3 §6.4 |

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
| first-appearance rule | Inside a line, a state motif is listed at ply k only if it holds after ply k and did not hold after ply k−1 (so it can be listed again after a gap; "first holds" in the concept map before 2026-10-22 meant this) | M2 §5 |
| line relevance | Former extra condition on `trapped_piece` (a later move of the line had to move or capture the piece). Removed 2026-10-20: it read later moves, which judges a line, not a board | M2 §8.1, §15 |
| trade in progress | Exclusion for `hanging_piece` inside a line: the "hanging" piece just captured something worth at least as much, so it is a recapture, not a gift | M2 §4.1 |

### 4.3 Fields of a motif

| Field | Meaning |
|---|---|
| `side` | X, the side that benefits |
| `ply` | Where in the line it holds (0 = the current position) |
| `actor` | The piece that does it: the forking knight, the pinner, the hanging piece itself, the overloaded defender |
| `targets` | The pieces it acts on, in a fixed order (M2 §5) |
| `gain` | SEE value in pawns, only for `hanging_piece` and `trapped_piece`; null otherwise. From PostM3_EM2 the text prints it as `SEE N` (before 2026-10-22: `gain N`); the JSON key stays `gain` |
| `threat` | Only on `pin_tactical`: `{kind, square}`, the kind of threat that removing the restricted piece would enable and the square b where X's threatening move lands. For `capture` that is the captured piece's square, except en passant, where b is the landing square and the captured pawn (a target) stands behind it. Before the 2026-10-21 milestone review this row equated the two. Null otherwise (§4.4, `pin_tactical`) |
| `restriction` | On `pin_absolute`, `pin_relative`, `overloaded_defender` and `pin_tactical` only: `{piece, restricted, free, held_to}`, the held piece, its restricted and free moves in UCI, and how it is held (§4.5, restricted moves, held to); null otherwise; `rule` |
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
| `pin_tactical` | A Y piece p is held in place by a threat: if p were gone, an X move would make a fork, skewer, discovered check or attack, double check, back-rank mate, promotion or checkmate that it does not make now, or X would win a Y piece by SEE that it cannot win now. Includes p guarding an empty square. Not computed while either king is in check (only Y's before 2026-10-21's gate), and not for a p whose removal would check either king. Reported only when at least one move of p still lets the threat happen (existence condition, from 2026-10-22). Actor: the X piece that would move; targets: p, then the threat's targets; `threat` names the kind and square. Called "tactical pin" in conversation (not a chess-literature term) | fixture-only |
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
| tactic | From M4 (Captain, 2026-10-23): a line whose consequence class is `mate` or `material`: with best replies from the opponent, the side playing it mates or wins at least 1 pawn net over the walked plies. A sacrifice is a tactic only when the line mates or wins back more than it gave within the walked plies; a sacrifice for an attack or positional compensation is at most an opportunity. Material won beyond the walked plies is not seen. Counted from the analysed position (M4 R1, R5: a recapture that nets ≥ 1 pawn from the analysed board is a tactic, since history is not an input). Called before M4 (PostM2_EM1 to M4 batch 1): a line whose only-move margin is ≥ 0.13 ES (`kTacticMarginUnits` = `kMistakeUnits`), measured in evaluation, so a sacrifice could be a tactic; that meaning is now **opportunity** | PostM2_EM1 §4.1, E2, M4 §4.3 |
| opportunity | From M4 (Captain, 2026-10-23): the tactic rule of PostM2_EM1 under a new name: a judged line whose only-move margin is ≥ 0.13 ES (`kOpportunityMarginUnits`, no longer tied to a verdict), so every alternative is clearly worse; it may gain material, mate, or only a positional advantage. A line can be an opportunity, a tactic, both or neither (two ways to win a hanging queen: tactic, not opportunity) | M4 §4.4 |
| only-move margin | Line ES minus the best alternative's ES, from the line side's view, in es_units; may be negative. Root line: `candidates[0]` minus `candidates[1]` (search A). Refutation: (1 − ES of the refuted move) minus search R's line. Before 2026-10-20 defined for the root only ("best minus second-best of A") | PostM2_EM1 §4.1 |
| judged line | A line that gets an opportunity judgement (a tactic judgement before M4): the root's best line (`candidates[0]`) and every refutation (from M4: of a detailed line). Other candidates, B's and E's threats and lines from C are not judged | PostM2_EM1 §4.1, E3–E4, M4 §4.4 |
| tactic judgement | The `tactic` object of a judged line: `forced`, `alternative`, `margin`, `is_tactic`, `labels`. Renamed **opportunity judgement** in M4 (2026-10-23) | PostM2_EM1 §5 |
| opportunity judgement | From M4 (2026-10-23): the `opportunity` object of a judged line: `forced`, `alternative`, `margin`, `is_opportunity`; the labels move to the line's `tactic_labels`. Called tactic judgement before M4 | M4 §5 |
| tactic line | A judged line with `is_tactic`. Only tactic lines get contributions and tactic labels. From M4 (Captain, 2026-10-23): a tactic line is any line that is an opportunity or a tactic; contributions and tactic labels go to every such line | PostM2_EM1 §4.2, M4 §4.5 |
| root motifs | From M4 (2026-10-23): `position.motifs` copied onto a move whose line is a tactic line, with contributions judged against that move's line (before M4, `position.motifs` itself carried the contributions for A's line 1). The move report keeps the direct and indirect ones | M4 §4.5 |
| best alternative | The move a tactic (from M4: an opportunity) is compared with: A's second line at the root, R's line for a refutation | PostM2_EM1 §4.1 |
| tactic sequence | A tactic line cut to its walked plies (§3.3): the moves contributions and labels read | PostM2_EM1 §4 |
| forced (in the report) | `tactic.forced` (`opportunity.forced` from M4): the judged line's root has exactly one legal move. No alternative and no margin; never an opportunity; source `rule` | PostM2_EM1 §4.1, E5 |
| saturation limit | ES flattens near 0 and 1, so in an already decided position (mate against a second-best at 0.98) no line is a tactic (from M4: an opportunity; the M4 tactic rule reads material and mate, so the line can still be a tactic). Known, accepted | E2, M4 §4.4 |
| two-winners limit | With two equally winning moves the margin is near 0, so neither is a tactic (from M4: an opportunity; each can still be a tactic). Known, accepted | E4, M4 §4.4 |
| width dependence | From M4 (2026-10-23): A's MultiPV width K follows the selection (5 by default, N for `topN`, every legal move for a whole-board keyword), and with a fixed node budget per line a different width gives a different search. The same move can therefore get a different rank, score and verdict in different requests: on F32, Bc5 is `okay` under `--moves top2` but `best` under `top6`; h6 is a `mistake` under `best,h6` but an `inaccuracy` under `all`. Each request stays reproducible. Accepted as intended (Captain, 2026-10-23, checkpoint 2): a wider search may judge differently | M4 §1 P7, §9, §15 |
| motif recognition | Is the pattern on the board, and do we report it exactly when it is? Must be exact against the written definitions; checked by the T fixtures and the reference implementation (§5.7) | M2 §7.4 |
| tactic recognition | Does the line gain against the best alternative, and do we label it a tactic exactly when it does? Mechanical from the engine, so exact by construction; tests check the computation (invariant check, §6.2) | PostM2_EM1 §14 |
| contribution judgement | For each motif on a tactic line, by rule: motifs of the other side → **none**; **direct** if it is a move motif or a later move of the tactic side captures one of its pieces of the other side; **indirect** if not direct and, after a tactic-side move (the one that made the motif's board, or a later one; not castling; before any motif piece has moved or been captured), one of the motif's denied moves would answer that move; else **none**. Plain sense (Captain, 2026-10-22): direct = the tactic's moves make it or capture its pieces; indirect = it takes away an answer the defender would have (a recapture, an escape, an interposition, a capture of the checker); none = the tactic works without it. Rule 3′, approved at checkpoint 1 and built on 2026-10-22 (PostM3_EM2 §4.2). Before 2026-10-22: **indirect** if a piece of the other side in the motif attacks (pins ignored) the destination of a later move of the tactic side, and none of the motif's pieces has moved or been captured by then; that rule called a motif "none" even when the defender would gain an escape without it (Re8# back-rank weakness). Before 2026-10-21, any later move by either side that moved or captured a motif piece made it direct, and any motif piece (including the tactic side's own) attacking a destination made it indirect. "Indirect" was open before 2026-10-20. Measured against hand-labelled C fixtures (§6.2) | PostM2_EM1 §4.3, §15; PostM3_EM2 §4.2 |
| denied move | A move the motif takes away from the defender, on a board with the defender to move: for the two pins, overloaded defender and tactical pin, the held piece's restricted moves; for `back_rank_weakness`, the king's steps onto a front or diagonal-front square held by its own piece, as if that piece were gone. `hanging_piece` and `trapped_piece` deny nothing | PostM3_EM2 §4.1 |
| answer | A denied move answers a tactic-side move when, with the defender in check, the king is no longer attacked after it, covering escape, capture of the checker and interposition (for an absolute pin, only the pinner's attack through the vacated square is ignored); otherwise when it captures the piece that just moved | PostM3_EM2 §4.2 |
| tactic label | A rule fact about a tactic line that reads later moves, so it is not a motif. Its fields match a motif's (`tag`, `side`, `ply`, `actor`, `targets`), without `gain` and `contribution`. One tag: `removal_of_defender` | PostM2_EM1 §4.4 |
| `removal_of_defender` | Tactic label: an X capture takes a Y piece d that alone kept another Y piece t safe (SEE 0 with d, ≥ 1 without), and a later X capture on t's square wins it; t must still stand on its square at that capture: no move in between starts or ends there. Before 2026-10-22 any later capture on t's square counted, even of a different piece. Actor: the capturing piece; targets: d, t. Outside the motif reference implementation | PostM2_EM1 §4.4, PostM3_EM2 §4.3 |
| restricted movement | Report wording (Captain, 2026-10-21) shared by the four tags that hold a Y piece in place: "restricted movement (absolute pin)", "(relative pin)", "(overloaded defender)" and "(tactic: Nd5-f4 fork)" for `pin_tactical` (actor, threat square, threat kind). Wording only: the JSON keeps the tags. Meaning (Captain, 2026-10-22): restricted does not mean the piece cannot move at all; it means it cannot move in all the ways it otherwise could. From 2026-10-22 the row says how the piece is held ("at square e5", "along file e") and names the restricted and free moves. An open proposal before 2026-10-21's freeze | PostM2_EM1 §10, PostM3_EM2 §4.1, §10 |
| held piece | The piece a restricted-movement motif holds in place: the pinned piece (targets[0]) for the two pins and `pin_tactical`, the defender (actor) for `overloaded_defender` | PostM3_EM2 §4.1 |
| restricted moves | The held piece's moves that the motif makes illegal (absolute pin) or costly: after it, X wins the rear piece (relative pin), wins a guarded piece (overloaded defender) or can still make the threat (tactical pin). Computed with the held side to move, over the held piece's move set (from 2026-10-22, Captain at checkpoint 2; before: its legal moves only, so a pawn whose only push stayed on the pin line had none) | PostM3_EM2 §4.1 |
| free moves | The legal moves of the held piece's move set that are not restricted moves; may be empty | PostM3_EM2 §4.1 |
| move set | (Captain, checkpoint 2, 2026-10-22) The moves a held piece is judged by: its legal moves (pseudo-legal for an absolute pin's restricted moves), plus, for a pawn, a hypothetical capture onto each square diagonally in front of it. Reason: a pawn's capture is a way it can move that only an occupied square unlocks, so "this pawn is pinned, so I can go there" is a valid thought. Other pieces have no hypothetical moves. Never a move that captures a king (2026-10-23: the board with the held side to move can leave the other king in check) | PostM3_EM2 §4.1 |
| hypothetical capture | (Captain, checkpoint 2, 2026-10-22) A pawn's capture onto a square diagonally in front of it, counted whatever stands there: empty, its own piece (an X capture there would put something there), or an X piece (then it is a real capture). Judged on the board after it: the pawn on that square, the previous occupant gone; legal when its own king is not attacked there. A capture onto the last rank is four promotion moves. Not added where the pawn already has a move to that square, a real capture or en passant (2026-10-23, review): the real move stands in | PostM3_EM2 §4.1 |
| held to | (Captain, checkpoint 1, 2026-10-22) How a restricted-movement motif restricts its held piece, read from the free moves' destinations: "at square w" (no free move; w is the held piece's square), "along file x", "along rank y" or "along diagonal z" (every free destination on that line through the held piece's square; a diagonal is named by its two edge squares, e.g. a4-e8), otherwise "only to squares a2, d1, g8" (the free destinations; Captain, 2026-10-22, option B). Always present. JSON `restriction.held_to` `{kind, name}` | PostM3_EM2 §4.1 |
| existence condition | (Captain, checkpoint 1, 2026-10-22) A pin, overloaded defender or tactical pin is reported only when its held piece has at least one restricted move, over its move set (from checkpoint 2, 2026-10-22: a pawn's hypothetical captures count). The alternative (for `pin_tactical` only, keeping the M2 pins as they were) was not chosen | PostM3_EM2 §4.1 |
| claim tag / presence tag / rule fact | Former split of the tags (2026-10-19/20) by whether the tag's name claims a material win. Retired 2026-10-20: every motif is a presence fact | M2 §15 |
| PostM2_EM1 | The milestone after M2 (Captain's name): tactic recognition, motif contribution, `removal_of_defender` as a tactic label, `pin_tactical`. Plan `Planning/milestones/PostM2_EM1.md`, receipt `Planning/receipts/PostM2_EM1.md`; complete, merged into `main` 2026-10-21 (branch `postm2-em1`). Seeds: `5cc9582^` (former detector), `2e4104e^` (fixtures T33, T34) | vision §11 |
| PostM3_EM2 | The milestone after M3a/M3b (Captain's name, 2026-10-22): fixes from the conceptual audit of 2026-10-22 (`Planning/critiques/`) before M4: restricted and free moves, contribution rule 3′, `removal_of_defender` continuity, conditions in the template, report v5 with semantic checks, data digests; at checkpoint 3 also `tripled_pawns` and T60. Plan `Planning/milestones/PostM3_EM2.md`, receipt `Planning/receipts/PostM3_EM2.md`; complete, merged into `main` 2026-10-23 (branch `postm3-em2`) | vision §11 |
| PostM4_EM3 | The milestone after M4 (Captain's name, 2026-10-24): tempo, initiative and an impact rating for facts (§3.6). Plan `Planning/milestones/PostM4_EM3.md`: decisions A1–A5 and B1–B3 frozen 2026-10-24; contract drafted the same day, checkpoint 1 pending | vision §11 |

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
| recall | On a theme's stratum: share of puzzles Lichess labels with it where we also predict it. "When Lichess says *fork*, do we find a fork for the solver?" Gated for the 10 measured themes. A Lichess theme names the motif behind the puzzle's tactic (a direct contribution, §4.5), so recall checks that our definitions catch what chess players mean by the word. It is coverage of Lichess-tagged puzzles, not precision: it says nothing about how often a tag fires where Lichess does not name it (stated 2026-10-22; see untagged firing rate) | M2 §7.3, PostM3_EM2 §7 |

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
| untagged firing rate | Per measured tag, the share of background puzzles without its theme where the solver gets the tag (test split, 2026-10-22, after the existence condition with hypothetical captures: `hanging_piece` 3,531/5,821 = 60.7%, pin 2,386/5,551 = 43.0%; pin was 2,459/5,551 = 44.3% before the existence condition and 1,723/5,551 = 31.0% with legal moves only). Printed beside recall so it is not read as precision; never gated | PostM3_EM2 §7 |

## 6. Process and testing

### 6.1 Planning and roles

| Term | Meaning | Defined in |
|---|---|---|
| vision | `Planning/PROJECT_VISION.md`: the source of truth for scope, method and decisions | AGENTS.md |
| milestone (M0–M7, PostM2_EM1, M3a, M3b, PostM3_EM2, PostM4_EM3) | A delivery step with its own plan `Planning/milestones/<name>.md` and frozen acceptance criteria. PostM2_EM1 sat between M2 and M3 (§4.5). M3 is split (Captain, 2026-10-21) into **M3a** (positional facts) and **M3b** (opening statistics), planned and built in parallel. PostM3_EM2 sits between M3b and M4 (Captain, 2026-10-22); PostM4_EM3 after M4 (Captain, 2026-10-24). Renumbered by the Captain on 2026-10-23: **M4** = playable moves and move reports (new), M5 = libllama prose (M4 before), M6 = books (M5 before), M7 = native apps (M6 before) | vision §11 |
| contract | The frozen parts of a milestone plan: headers, report schema, fixtures, acceptance, owned files. Workers implement against it and do not change it | AGENTS.md |
| contract change | Any change to a frozen item after freezing; recorded as a row in the plan's last section ("§15 row" in M2) with date, change and reason | M2 §15 |
| D1–D13 | The Captain's numbered decisions in M2: D1–D10 at the start, D11–D13 at checkpoint 2b (motif/tactic split, reference implementation, `removal_of_defender` removed) | M2 §1 |
| E1–E7 | The Captain's numbered decisions in PostM2_EM1 (briefed as P1–P5 on 2026-10-20): order, threshold, judged lines and search R, best move only, forced moves, contribution rule, `removal_of_defender` as a tactic label | PostM2_EM1 §1 |
| G1–G15 | The Captain's decisions on the 2026-10-22 audit findings, frozen in PostM3_EM2 §1 (the Deputy's 15 recommendations, with the Captain's changes to G5 contribution "none" and G10 restricted movement) | PostM3_EM2 §1 |
| A1–A5 | The Captain's tempo and initiative decisions in PostM4_EM3 (2026-10-24): threat test, trades, ledger scope with the 15 s timing check, tempo value, initiative. **B1–B3** are her impact rating decisions of the same day: basis (static scale), use (use test), net change (root against end) | PostM4_EM3 §1 |
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
| F fixtures | Engine fixtures in `tests/fixtures/positions.json` (F01–F20, F16 dropped; PostM2_EM1 adds F21 onward; M4 adds F32–F39 for selection, verdicts, tactic and opportunity): world-championship positions with expected threats, urgency, verdicts and, from PostM2_EM1, tactic judgements; pinned to Stockfish 19 at `standard`, robust at all budgets | M1 §9, PostM2_EM1 §9, M4 §9 |
| U, E, parse fixtures | No engine judgement: U = opening names and PGN/ply input, E = error exit codes and messages, parse = FEN/move parsing | M1 §9 |
| T fixtures | Motif unit fixtures in `tests/fixtures/motifs.json` (T01–T41 from M2; T42 onward for `pin_tactical`, PostM2_EM1): a position and line with instances that must (`includes`) or must not (`excludes`) be reported. A `pin_tactical` include also states its `threat` | M2 §9, PostM2_EM1 §9 |
| C fixtures | Contribution unit fixtures C01–Cnn in `tests/fixtures/tactics.json`: a tactic sequence with the hand-labelled contribution of every motif of the tactic side (except `pin_tactical`, listed only for chosen instances, Captain 2026-10-21), plus tactic labels that must or must not appear. Hand labels come from the plain meaning, not from running the rule; the Captain reviews them | PostM2_EM1 §9 |
| hand label | A fixture's expected value set by a person from the chess meaning, with a one-line reason, before the rule's output is seen. Where rule and hand label differ, the Captain decides which changes | PostM2_EM1 §9, §12 |
| evaluation position | (Captain, 2026-10-24) A position the Captain gives to test whether ChessWhy's output supports the explanation a reputed trainer gives, with the language model taken out: the Deputy explains the move from ChessWhy's printed text only. Ids V1, V2, …; stored in `tests/fixtures/evaluation.json` (FEN, game, the trainer's move) and listed in `FIXTURES.md` §10 with the trainer's point, what ChessWhy printed and the Deputy's reading of it. Not a CTest fixture: no expected values, no test reads it; findings are test results, not scope proposals | FIXTURES §10 |
| invariant check | Part of the F harness from PostM2_EM1: on every F report, recomputes each tactic judgement from the report's own evals (margin, threshold, forced, which lists carry contributions). From PostM3_EM2 the semantic check also covers it, on every report | PostM2_EM1 §9, PostM3_EM2 §4.6 |
| mutant | A hand-corrupted copy of a valid report that breaks one semantic check; the checker must reject each one with that check's name. PostM3_EM2 had 14; from M4 there are 21 on report v6 and the move report document (the 14 updated plus one per new relation, including the projection check) | PostM3_EM2 §4.6, M4 §9 |
| positive / near miss | A T fixture where the motif holds / a deliberately similar position where it must not (e.g. a fork whose forking piece can be taken) | M2 §9 |
| constructed | A fixture position made up by hand rather than taken from a game | M2 §9 |
| `motifs_include` | Key on an F fixture: listed motif instances must appear in the report (extras allowed). From PostM2_EM1 an entry may also state its `contribution` | M2 §9, PostM2_EM1 §9 |
| `positional_include` | Key on an F fixture from M3a: listed positional features or changes must appear in the report (extras allowed); partial objects match like `motifs_include` | M3a §8 |
| `text_contains` | Key on an F fixture from M3b: the text output of the plain run must contain the given string. Used for the rendered games row (F29) and, from PostM3_EM2, for the conditions and restriction lines. From M4 (2026-10-23) the plain run prints the move report text, so the strings are move report rows (e.g. F02 "Forced: only legal move") | M3b §9, PostM3_EM2 §10, M4 §9 |
| `full_text_contains` | Key on an F fixture from M4 (2026-10-23): the full report text (`--full-report` without `--json`) must contain the given string; holds the former `text_contains` strings the relevance filter drops (F04, F05, F08, F21, F31) | M4 §9 |
| `playable_includes` | Key on an F fixture from M4 (2026-10-23): the listed moves must be playable in the full report. Called `fine_includes` before 2026-10-23 (the listed moves had to be fine) | M4 §9 |
| `runs` | Key on an F fixture from M4 (2026-10-23): extra runs of the same position, each with its own arguments (usually another `--moves`) and its own expectations. Expectations on the move report document use the `moves_` keys (`moves_count`, `moves_san`, `moves_equals`, `moves_motifs_include`); `candidate` pins fields of a named move in the full report | M4 §9 |
| P fixtures | Positional unit fixtures in `tests/fixtures/positional.json` (M3a): `features` entries P01… (a position with features that must or must not be reported) and `lines` entries PL01… (a line with positional changes that must or must not be reported). Hand-labelled from the plain definitions; at least 10 are positions from the Captain's books, cited by game (a fixture's `source` never names the book). From PostM3_EM2 every board also goes through the positional reference (`positional_oracle`). They check the rule on positions from books; they do not show the books support the rule. From PostM4_EM3 the file also holds the use-test fixtures | M3a §8, PostM3_EM2 §9, PostM4_EM3 §11 |
| use-test fixture | Contracted (2026-10-24). A `uses` entry PU01… in `tests/fixtures/positional.json`: a root FEN, a line in UCI, whether the position counts as tempo-dependent, and the changes that are kept (net change in use), cut (net change not in use) and passing; checks net change and the use test without the engine | PostM4_EM3 §11 |
| re-blessing | Re-deriving engine fixture expectations for a new Stockfish version; an explicit milestone task | AGENTS.md |

### 6.3 Tests, gates, builds

| Term | Meaning | Defined in |
|---|---|---|
| CTest label | `unit`: no engine or model. `engine`: needs `assets/engines/stockfish` and, from M3b, the game-count table. `games` (from M3b): needs the broadcast month files fetched by `tools/fetch_assets.sh games`. `model`: needs a GGUF model, opt-in. Missing assets fail, never skip | AGENTS.md, M3b §9 |
| `tactics_puzzles` | CTest (`unit`) running `tactics_puzzle_eval` on test: recall and Lichess agreement; `pin_tactical` switched off (no theme maps to it) | M2 §7.3, PostM2_EM1 §9 |
| `tactics_oracle` | CTest (`unit`) running `tactics_motif_oracle`: detectors against the reference implementation on test and the T fixtures (`pin_tactical` on the sample only, see compared positions); zero mismatches required. Replaced `tactics_confirm` (engine-confirmed precision, removed 2026-10-20) | M2 §7.4, PostM2_EM1 §9 |
| `positional_oracle` | CTest (`unit`, M3a): positional features, changes and the mobility, space and development counts against an independent reference on every board of every `test.csv` solution line; zero mismatches required | M3a §8 |
| `games_oracle` | CTest (`games`, M3b): `chesswhy-games` on two pinned months against `tools/count_games.py`, an independent python-chess counter; the two tables must be byte-identical | M3b §9 |
| gate | Required checks. After a batch: build and affected tests. At milestone end: all tests in Release and ASan/UBSan, clang-format, clang-tidy, Ruff, basedpyright, ShellCheck, the smoke command | AGENTS.md |
| Release / ASan build | `build/release` (optimised) and `build/asan` (debug with address and undefined-behaviour sanitisers) | AGENTS.md |
| smoke command | One frozen CLI invocation run by hand at milestone end to see the feature work end to end. M2 and PostM2_EM1: `--fen 'k7/8/8/8/3Q3n/8/2P5/4K3 w - - 0 1' --move c3`, expecting Black's knight fork from f3 (M2), and from PostM2_EM1 a refutation that is a tactic with the fork `direct`, while the root (all lines at ES 1.0) is not a tactic. PostM3_EM2: `--fen '6k1/5ppp/8/8/8/1q6/5PPP/4R1K1 w - - 0 1' --budget standard --no-llm`, expecting Re8# with the back-rank weakness `indirect` | M2 §14, PostM2_EM1 §14, PostM3_EM2 §14 |
| golden test | Compares rendered text with a stored expected file byte for byte | M2 §10 |
| timing check | `standard` must finish within 15 s per F fixture in Release (Captain, 2026-10-24, PostM4_EM3 A3, for the tempo ledger; 12 s before, the target until PostM4_EM3 is built) | M2 §14, PostM4_EM3 §1 |
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

Partly built: positional facts (M3a), opening names and game counts (M3b); the rest is
planned. [CONCEPTS.md](CONCEPTS.md) marks what is built. Before 2026-10-22 several built
rows here still said "Planned".

| Term | Meaning | Defined in |
|---|---|---|
| positional fact | A lasting feature computed by rule (pawn structure, king safety, piece activity, space and development, material balance), reported as a positional feature of the position and as positional changes along a line, and used in the explanation only when tied to a line or verdict. Built in M3a; §9 has the parts. Since 2026-10-21 (Captain's option B): material balance only; before, "material imbalance" (which pieces each side has, beyond the count) and named pawn structures were included, now deferred | vision §5.6, M3a |
| position key | Piece placement, side to move, castling rights and an en-passant square only when a legal capture exists; move counters ignored, so transpositions match | vision §5.7, M1 §4.8 |
| opening name | Name and ECO code of a position in the pinned opening dataset; `book` provenance; never guessed for an unnamed position | vision §5.7, M1 §4.8 |
| last named position | For PGN input: the last position on the game mainline (up to and including the analysed position) that has an opening name, with its ply | M1 §4.8 |
| game-count table | A position → move-count table built offline from pinned game collections, each source and platform kept apart, with a recorded table id. One source since 2026-10-21: Lichess broadcasts, both players 2200+, January 2020 to September 2026, in `assets/data/games/lichess_broadcast_2200.tsv`. Built in M3b | vision §5.7, M3b §4 |
| game counts | Per source and position, the number of games, results and the count for each move played; `games` provenance; every figure names its source and platform and is never phrased as move quality. Built in M3b | vision §2, §5.7, M3b §5 |
| rating floor | The minimum rating both players of a game must have (`WhiteElo` and `BlackElo` tags) for the game to enter the game-count table; games without both tags are left out. 2200 (Captain, 2026-10-21) | vision §5.7, M3b §4.1 |
| broadcast month | One pinned monthly file of the Lichess broadcast database (`lichess_db_broadcast_YYYY-MM.pgn.zst`), checked against the SHA-256 Lichess published on 2026-10-21 | M3b §4.4 |
| excluded game | A game left out of the game-count table, counted under the first failing reason: `variant` (not standard chess), `setup` (custom start position), `rating` (below the rating floor or no rating), `result` (unfinished), `illegal` (an unreadable or illegal move within the ply limit) | M3b §4.1 |
| broken game | A game whose moves cannot be replayed legally, usually a relay error. Excluded only when the error falls within the ply limit (reason `illegal`); later errors are never read | M3b §1 B2 |
| ply limit | How deep into each game positions are counted for the game-count table: positions after 0 to L−1 half-moves, with the move played from them. L = 40 (Captain, 2026-10-21) | M3b §4.2 |
| minimum game count | The number of counted games a position needs to be kept in the game-count table. N = 5 (Captain, 2026-10-21) | M3b §4.2 |
| table id | `games::kTable`: the table's name with the first 12 hex digits of its SHA-256, recorded in every report, like the opening dataset's commit. From PostM3_EM2 the loaded bytes decide which id the report names (data digest); before 2026-10-22 the report named it whatever table was loaded | M3b §4.4, PostM3_EM2 §4.5 |
| data digest | The SHA-256 of the game-count table and of the opening-name files as loaded, computed by an in-tree SHA-256; the report names the shipped id when the digest matches the pin, else a `custom@` id | PostM3_EM2 §4.5 |
| `custom@` id | The id a report gives loaded data that does not match the pin: `custom@` plus the first 12 hex digits of its SHA-256 | PostM3_EM2 §4.5 |
| `chesswhy-games` | The offline tool that reads PGN text on standard input and writes the game-count table (§4 of M3b); run by `tools/fetch_assets.sh games` | M3b §3 |
| book corpus | Planned: the Captain's local books as text, kept out of the repository | vision §8 |
| book index | Planned: a BM25 search index over page or paragraph chunks of the book corpus, kept out of the repository | vision §8 |
| concept tag | Planned: a motif or positional tag from the report, widened by a checked-in keyword map, used as the book query | vision §8 |
| book passage | Planned: one to three short excerpts from the book index, cited by book and page, offered as general advice and never as analysis of the position. Shown only when on topic (it names the report's concept), applicable (its preconditions hold on the board) and with its stance kept (a passage that warns against an idea is not offered as support) | vision §8 |
| explanation | The text a reader gets for one report: template rendering today; later language-model prose that passed the fact check | vision §7 |
| template rendering | Fixed sentences filled from report fields, adding no claim: the readable output today, the `--no-llm` output, and the fallback when prose fails the fact check. From M4 (2026-10-23) it renders the move report document, one block per move report (`Planning/milestones/M4.md` §10.1), and with `--full-report` the full report text (§10.2) | vision §7.3, M4 §10 |
| fact sheet | Planned: a compact text form of the report (not raw JSON) given to the language model with a level-specific prompt | vision §7.2 |
| fact selection | Planned: which report facts go on the fact sheet: the core claims (verdict, refutation, conditions) at every level; tactic and blunder evidence before positional facts; the level changes vocabulary, not which core claims are present | vision §7.2 |
| claim unit | Planned: one fact or one relation in the prose (who threatens what, at which ply, under which condition); the fact check checks units, so correct words in a wrong relation fail | vision §7.3 |
| language model | Planned: a local GGUF model run through llama.cpp (linked as a library) that turns the fact sheet into prose for the chosen level; decides no chess content | vision §7.1 |
| fact check | Planned: the prose split into claim units, each of which must match report fields including its relations; opening names and game counts must match exactly; one regeneration, then template rendering with a notice. Before 2026-10-22: every move, square and piece named in the prose had to be in the report, which let a wrong relation between correct names pass | vision §7.3 |
| relational test set | Planned: M5's prose tests (M4's before the 2026-10-23 renumbering) seeded from the 13 relational errors X1–X13 of the 2026-10-22 audit (threat owner swapped, premise dropped, ply wrong and others); each must fail the fact check | vision §7.4 |

## 9. Positional features (M3a)

Built in M3a. Exact rules are in `Planning/milestones/M3a.md` §4–§5. Before 2026-10-22
this line said "Planned until M3a is built".

| Term | Meaning | Defined in |
|---|---|---|
| positional feature | One feature of §4 of M3a on one board: a tag, an owner, the pieces it concerns, and a file or value where the tag has one; `rule` provenance. `position.positional` lists those of the analysed position | M3a §4 |
| owner | The side a positional feature belongs to: the pawns' or pieces' side, the king's side for king safety, the side ahead for leads and material | M3a §4 |
| feature identity | What makes a feature "the same" on two boards: its tag, owner and, depending on the tag, its file, the square of its piece, its pawns' squares, its value or the colour of its bishop's squares | M3a §4 |
| positional change | A positional feature that `appears` or `disappears` at a ply of a line, judged by feature identity; a feature whose identity persists while its pieces or value change is not a change. Listed in `MoveAssessment.positional` for each candidate and the asked move, over the walked plies | M3a §5 |
| `holds_at_end` | On a positional change: an appearance is still there, or a disappearance is still gone, at the end of the walked plies (Captain's option C, 2026-10-21) | M3a §5 |
| relative rank | A rank counted from a side's own back rank (1) towards the other side (8) | M3a §4 |
| king files | The file of a king and the files next to it | M3a §4.2 |
| mobility | For a side: squares attacked by its knights, bishops, rooks and queens that hold none of its own pieces and are not attacked by an enemy pawn, summed per piece | M3a §4.4 |
| space | For a side: squares on files c–f, its relative ranks 2–4, behind one of its own pawns on the same file, holding none of its pawns and not attacked by an enemy pawn | M3a §4.4 |
| undeveloped | For a side: its knights and bishops still on its back rank | M3a §4.4 |
| lead threshold | The gap a side needs to be given a lead: mobility 10, space 3, undeveloped pieces 2 (approved by the Captain 2026-10-21, M3a P4; this row said "open" until 2026-10-22); set so that a lead appears in about one position in six (mobility, space) of strong broadcast games | M3a §4.4 |

The tags in plain words (owner X, other side Y):

| Tag | Plain words | Group |
|---|---|---|
| `isolated_pawn` | X pawns on a file with no X pawn on either neighbouring file | pawn structure |
| `doubled_pawns` | Two or more X pawns on one file | pawn structure |
| `tripled_pawns` | Three or more X pawns on one file; such a file is also `doubled_pawns` (Captain, checkpoint 3, 2026-10-23) | pawn structure |
| `backward_pawn` | An X pawn left behind its neighbours whose next square is covered by a Y pawn | pawn structure |
| `passed_pawn` | The front X pawn of a file with no Y pawn ahead of it on its own or a neighbouring file | pawn structure |
| `connected_passed_pawns` | X passed pawns on neighbouring files | pawn structure |
| `pawn_islands` | The number of separate groups of X pawns | pawn structure |
| `pawn_chain` | Three or more X pawns on one diagonal, each defending the next | pawn structure |
| `castling_kingside`, `castling_queenside` | X may still castle on that side | king safety |
| `king_shield_gap` | A king file with no X pawn on the two squares in front of the X king (king on its first two ranks) | king safety |
| `open_file_near_king` | A king file with no pawns | king safety |
| `half_open_file_near_king` | A king file with no X pawn and at least one Y pawn | king safety |
| `king_zone_attacker` | A Y piece (not pawn or king) attacking the X king or a square next to it | king safety |
| `bishop_pair` | X has bishops on both colours | pieces |
| `rook_open_file` | An X rook on a file with no pawns | pieces |
| `rook_half_open_file` | An X rook on a file with no X pawn and at least one Y pawn | pieces |
| `rook_seventh` | An X rook on its seventh rank, where the Y king sits behind or Y pawns stand | pieces |
| `outpost` | An X knight or bishop on its ranks 4–6, defended by an X pawn, that no Y pawn on today's board can attack by advancing on its file; a Y pawn that could reach a neighbouring file by a capture is not considered (example: `7k/2p5/3B4/4N3/3P4/8/8/K7 w - - 0 1`, where ...cxd6 would let a pawn attack e5). Before 2026-10-22 this gloss said "can ever attack" | pieces |
| `bad_bishop` | An X bishop whose colour holds most X pawns, at least two of them blocked by Y pawns | pieces |
| `mobility_lead`, `space_lead`, `development_lead` | X leads by at least the lead threshold; the value is the gap | space and development |
| `material_ahead` | X is ahead in material (1/3/3/5/9); the value is the gap in pawns | material |
| `exchange_up` | X has more rooks than Y while Y has more knights plus bishops than X. Not a material lead: it fires while X is behind in material on 17.9% of the boards where it holds (1,373 of 7,662). Before 2026-10-22 this gloss said "an extra rook while Y has an extra knight or bishop" | material |
