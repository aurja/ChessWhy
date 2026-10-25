# PostM4_EM4 — contract (the conceptual critique and the move report text)

Branch `postm4-em4` (from `postm4-em3` at `92dd918`). Scope as closed (Captain, 2026-10-25,
R4): part 1 (motif rows), part 1b/1c (position walk) and the conceptual critique of all
definitions and concepts of 2026-10-24 (`Planning/critiques/2026-10-24/`, findings only).
The pure reply definition "forced and gains nothing" and the fixes the critique calls for
are PostM4_EM5 (`Planning/milestones/PostM4_EM5.md`); there is no part 2.

## 1. Captain's decision

| # | Topic | Decision |
|---|---|---|
| R1 | Motif rows in move reports (2026-10-24) | Option 3: merge repeated restricted-movement motifs in the move report, and print restricted-movement rows compactly in the move report text. Measured before the decision on the 53 F fixture runs' move reports: 132 motif rows, 125 after the merge (V2: 27 → 22); most of the length is inside each row (the restricted and free move lists) |
| R2 | Move report text layout (2026-10-24) | The default output (no `--full-report`) is structured per move as a walk through the line: the engine and budget rows (always), the opening rows (always), then for each move its starting position as a FEN with that position's facts, then each ply's move, the resulting position as a FEN and that position's facts; tempo and initiative facts sit in the ply they belong to. Horizontal lines and blank lines separate the chunks. Superseded in layout by R3 |
| R3 | Position walk (2026-10-25) | "It should be position-by-position, because the position comes first, and the move comes from it." Each block is one position: `Position: FEN`, then the engine evaluation, the move played from it and its side, the facts tagged as uses, the other facts. No separate start block. No tempo rows in the move report text for now ("the tempo model is a bit broken right now"). Evaluation of the positions after the move: the line's engine score, labelled as such (Captain's choice of option "line score" in the brief of 2026-10-25; the other options were a new search per position and an evaluation row only where measured) |
| R4 | Milestone scope (2026-10-25) | Close PostM4_EM4 as the critique and move report text milestone: parts 1, 1b, 1c and the critique of 2026-10-24. The planned part 2 (pure reply and the critique's fixes) becomes the milestone PostM4_EM5 |

## 2. Rules

### 2.1 Motif merge (relevance filter)

Applies to every motif list of a move report, i.e. every list `relevant_motifs` produces
(`motifs`, `root_motifs`, and the motifs a projected threat keeps), after the contribution
filter (contribution direct or indirect). The full report is unchanged.

- A **restricted-movement motif** has tag `pin_absolute`, `pin_relative`,
  `overloaded_defender` or `pin_tactical`.
- Two restricted-movement motifs of one list are **repeats** when they agree on `side`,
  `ply`, `actor`, `targets`, `restriction` and `contribution`; they differ only in `tag` and
  `threat`.
- Each group of repeats becomes one entry at the position of its first member, which keeps
  that member's fields. Its `also` lists `{tag, threat}` of the other members in list order.
  Every other entry has `also: []`. List order is otherwise unchanged.
- `impact_order` is unaffected (the motifs group is non-empty iff it was before).

### 2.2 Report v8

`kSchemaVersion` 8. `schema/report.v8.schema.json` = v7 with the version (no other change);
`schema/moves.v8.schema.json` = v7 plus, on `$defs/motif`, the required property
`also`: array of `{tag, threat}` (`additionalProperties: false`; `tag` one of the four
restricted-movement tags; `threat` a `motif_threat` iff `tag` is `pin_tactical`, else
null). v7 files are renamed, not kept.

### 2.3 Compact row (move report text)

Only in the move report text (without `--full-report`); the full report text keeps today's
rows. A restricted-movement motif prints:

```text
Uses: restricted movement (<label>[; <label>…]): <actor> → <targets>; <held to> (ply <k>, <contribution>) [rule]
```

`<label>` is today's inner label (`relative pin`, `absolute pin`, `overloaded defender`,
`tactic: Ba7-f2 capture`), first the entry's own, then each `also` in order. `<held to>` is
today's held-to phrase. The `restricted:` and `free:` lists are not printed. Other motif rows
are unchanged. V2, Bxa4, ply 3:

```text
Uses: restricted movement (relative pin; tactic: Ba7-f2 capture): b a7 → B c5, R f2; along diagonal a7-g1 (ply 3, direct) [rule]
```

### 2.4 Checker

`tools/validate_report.py` re-implements the merge in the projection check independently
(expected list = merge(filter(full list))), and validates against the v8 schemas.

### 2.5 Position walk (move report text, part 1c; replaces the part 1b ply walk)

Only the move report text (no `--full-report`) changes; the full report text and all JSON
are unchanged (tempo facts stay in both). Layout:

```text
Engine: … [rule]                         (as today)
Budget: … [rule]                         (as today)
Opening dataset: … [book]                (opening rows as today)
Showing … [rule]                         (selection row as today)

────────────────────────────────────────────────────────────
1. Bxa4 (b5a4): best, es=75.2%, loss=0.0%, depth=17 [engine]
Line: Bxa4 bxa4 Ba7 … ; facts read to ply 7 of 10 [engine]
Tactic: … / Opportunity: … / Refuted by: … with its rows   (move-level rows, as today)
Played in … games …                                          (games row, as today)

Position: 1br3k1/5pp1/p3p2p/1bBn4/N7/1P3PP1/5R1P/2R3K1 b - - 0 1
  Evaluation: 24.8% for White (cp -100) [engine]
  Move: Bxa4 (Black)
  Uses: restricted movement (relative pin; tactic: Rc8-c1 capture): r c8 → B c5, R c1; only to squares a3, e3 (indirect) [rule]

Position: <line_fens[0]>
  Evaluation: 24.8% for White (cp -100; line score) [engine]
  Move: bxa4 (White)
  - outpost: white: N a4 (net change to ply 7) [rule]

…

Position: <line_fens[6]>
  Evaluation: 24.8% for White (cp -100; line score) [engine]
  Uses: …
```

- Separator: 60 × `─` (U+2500) on its own line before each move; one blank line before it,
  after the move-level rows, and between position blocks.
- **Move-level rows**, unchanged from part 1b: the move row (`N. ` or `Asked: `), `Forced:`
  if any, `Line:`, `Tactic:`, `Opportunity:`, the refutation with its opportunity, tactic
  labels and motifs (these keep `(ply k, …)`: they are plies of the refutation line), then
  the move's games row.
- **Position block** k for k = 0 … W, W = `walked_plies`, printed even without facts:
  1. `Position: <FEN>`: `position.fen` for k = 0, `line_fens[k-1]` otherwise.
  2. `  Evaluation: …`: for k = 0 the root evaluation, worded as today's `Position:` row
     evaluation (`24.8% for White (cp -100) [engine]`, `(mate N)`, `(cp N, mate N)`;
     `unavailable [rule]` when `eval_white` is null). For k ≥ 1 the move's `eval` (the line's
     engine score) turned to White's view (ES 1 − es when Black moved; `cp` and `mate`
     negated), with `line score` inside the parentheses: `(cp -100; line score)`,
     `(mate 3; line score)`.
  3. `  Move: <line_san[k]> (<side>)` for k < W, and for k = 0 always (the move itself);
     the side is the side to move at the start for even k, the other side for odd k. No
     `Move:` row for k = W ≥ 1: the walk ends there (the rest of the line is in `Line:`).
  4. `  Uses:` rows: the motifs (and, at k = 0, the root motifs) with `ply` = k.
  5. The tactic-label rows of ply k.
  6. The other groups in `impact_order`, the `tempo` group skipped: parried and created
     threat at k = 1, positional changes with `ply` = k.
- **Row forms** inside blocks are those of part 1b: motif and tactic-label rows without
  `ply k`; a positional row ends ` (net change to ply N) [rule]`, N = W.
- **No tempo in the move report text:** no `Tempo value:`, tempo-gained or `Initiative:`
  row, at the root or in any block.
- No move shown (empty selection, terminal position): one block after the selection row,
  no separator: `Position: <FEN>`, then `  To move: <Side>, N legal moves [rule]`, then the
  `Evaluation:` row as for k = 0, or for a terminal position `; checkmate`/`; stalemate`
  on the `To move:` row and no `Evaluation:` row (today's terminal wording).

### 2.6 `line_fens` (report v8, part 1b)

Each move report gains `line_fens`: the FEN after each walked ply, `line_fens[k-1]` after
`line_san[k-1]`, length `walked_plies`; serialised right after `walked_plies`; required in
`schema/moves.v8.schema.json` (array of strings). Computed in `move_reports` by replaying
`line_san` from `position.fen` with chess-library (`getFen()`). Not a chess claim: the
position the line reaches. v8 is amended in place (unreleased; the branch is unmerged).

### 2.7 Checker (part 1b)

The projection check replays `line_san` from `position.fen` with python-chess and requires
`len(line_fens) == walked_plies` and each FEN to equal the replayed position, both sides
normalised through `chess.Board(fen).fen(en_passant="legal")`.

## 3. Work packages

| WP | Owner | Files | Done when |
|---|---|---|---|
| M1 | C++ | `include/chesswhy/report.hpp` (`kSchemaVersion`; `Motif::also` with `MotifLabel {tag, threat}`, empty in the full report and not serialised there), `src/report/move_report.cpp` (merge in `relevant_motifs`, serialise `also`), `src/render/render.cpp` (§2.3), `schema/*.v8.schema.json` (git mv from v7 + §2.2), `tests/report/move_report_tests.cpp`, `tests/report/report_unit_tests.cpp`, `tests/render/render_unit_tests.cpp`, `tests/report/CMakeLists.txt` and `tests/cli/CMakeLists.txt` (schema paths), `README.md` (v7 → v8, the compact row), `include/chesswhy/move_report.hpp` (comment) | Unit tests for: a relative pin and a tactical pin with equal fields merge (order kept, `also` filled); a differing `restricted` keeps two entries; a differing `contribution` keeps two entries; non-restricted motifs get `also: []`; the compact row text for a merged and an unmerged entry; full report text unchanged |
| M2 | Python | `tools/validate_report.py`, `tests/checker/unit_tests.py` | v8 schema names; projection merge (§2.4); mutants that fail: an unmerged repeat, a dropped `also` entry, an `also` in the wrong order |
| M3 | C++ (part 1b) | `include/chesswhy/move_report.hpp` (`MoveReport::line_fens`), `src/report/move_report.cpp` (§2.6), `src/render/render.cpp` (§2.5), `schema/moves.v8.schema.json`, `tests/report/move_report_tests.cpp`, `tests/render/render_unit_tests.cpp`, `tests/render/golden/*.txt` (regenerated), `README.md` (the layout) | Done (part 1b ply walk, `4a5beed`) |
| M5 | C++ (part 1c) | `src/render/render.cpp` (§2.5), `tests/render/render_unit_tests.cpp`, `tests/render/golden/move_*.txt` (regenerated), `README.md` (the layout) | Unit tests for: a move report whose facts sit at positions 0, 1, 3 and whose positional change sits at position 2 prints them in those blocks, Uses rows before the other facts; evaluation rows at k = 0 (root) and k ≥ 1 (line score, Black mover turned to White's view); `Move:` rows for k < W only; no tempo row although the move report has tempo facts; empty selection prints one block with `To move:`; full report text unchanged (its goldens unchanged) |
| M4 | Python (part 1b) | `tools/validate_report.py`, `tests/checker/unit_tests.py` | §2.7; mutants that fail: a wrong FEN, a missing FEN |

## 4. Acceptance

1. Release: affected unit tests, `report_schema`, F41 and the F tests whose move reports
   contain restricted-movement motifs pass; `check_dbc` passes.
2. V2 smoke (`--moves Bxa4,Ba7`): no two restricted-movement rows of one move report differ
   only in their label; no `restricted:`/`free:` list in the move report text; the full
   report text still lists them.
3. Full report JSON identical to `92dd918` except `schema_version`.
4. Part 1b, V2 smoke (`--moves Bxa4,Ba7`): engine, budget and opening rows first; each move
   in its own chunk with a start block and ply blocks 1 … 7 carrying FENs; the tempo and
   initiative rows in their ply blocks; `--full-report` text unchanged.
5. Part 1b, Release: affected unit tests, `report_schema` and the `engine` label pass;
   `check_dbc` passes.
6. Part 1c, V2 smoke (`--moves Bxa4,Ba7`): per move, position blocks 0 … 7, each `Position:`
   with a FEN, then `Evaluation:`, then `Move:` (blocks 0 … 6); no `Start:`, `Ply k:`,
   `Tempo value:`, `Tempo:` or `Initiative:` row; `--full-report` text unchanged; Release
   `unit` label and `check_dbc` pass.

## 5. Contract changes

| Date | Change | By |
|---|---|---|
| 2026-10-25 | Part 1c (R3): the ply walk becomes the position walk (§2.5 rewritten; before, a start block and ply blocks `Ply k: <move>` / `Position: <FEN>`, with tempo rows); WP M5, acceptance 6 | Captain |
| 2026-10-25 | R4: part 2 moved to PostM4_EM5; EM4 closes with parts 1, 1b, 1c and the critique | Captain |
