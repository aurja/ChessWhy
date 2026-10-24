# PostM4_EM4 — contract (part 1: motif rows; part 1b: ply walk)

Branch `postm4-em4` (from `postm4-em3` at `92dd918`). The rest of EM4's scope (the pure reply
definition "forced and gains nothing" and whatever the conceptual critique of all
definitions and concepts finds) is frozen later, after the critique; this file then gains
part 2.

## 1. Captain's decision

| # | Topic | Decision |
|---|---|---|
| R1 | Motif rows in move reports (2026-10-24) | Option 3: merge repeated restricted-movement motifs in the move report, and print restricted-movement rows compactly in the move report text. Measured before the decision on the 53 F fixture runs' move reports: 132 motif rows, 125 after the merge (V2: 27 → 22); most of the length is inside each row (the restricted and free move lists) |
| R2 | Move report text layout (2026-10-24) | The default output (no `--full-report`) is structured per move as a walk through the line: the engine and budget rows (always), the opening rows (always), then for each move its starting position as a FEN with that position's facts, then each ply's move, the resulting position as a FEN and that position's facts; tempo and initiative facts sit in the ply they belong to. Horizontal lines and blank lines separate the chunks |

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

### 2.5 Ply walk (move report text, part 1b)

Only the move report text (no `--full-report`) changes; the full report text and the full
report JSON are unchanged. Layout:

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

Start: 1br3k1/5pp1/p3p2p/1bBn4/N7/1P3PP1/5R1P/2R3K1 b - - 0 1
  Position: Black to move, 37 legal moves; evaluation 24.8% for White (cp -100) [engine]
  Tempo value: 29.6% for Black (tempo-dependent position) [engine]
  Uses: restricted movement (relative pin; tactic: Rc8-c1 capture): r c8 → B c5, R c1; only to squares a3, e3 (indirect) [rule]

Ply 1: Bxa4 (Black)
Position: <FEN after ply 1>
  Tempo value: 24.8% for White (tempo-dependent position) [engine]
  - outpost: white: N a4 (net change to ply 7) [rule]

Ply 2: bxa4 (White)
Position: <FEN after ply 2>
  …
```

- Separator: 60 × `─` (U+2500) on its own line before each move; one blank line before it,
  after the move-level rows, and between blocks.
- **Move-level rows**, in today's order: the move row (`N. ` or `Asked: `), `Forced:` if
  any, `Line:`, `Tactic:`, `Opportunity:`, the refutation with its opportunity, tactic
  labels and motifs (these keep `(ply k, …)`: they are plies of the refutation line), then
  the move's games row.
- **Start block**: `Start: <position.fen>`, then the indented `Position:` and root
  `Tempo value:` rows (today's header rows, moved here and repeated per move), then the
  ply-0 facts.
- **Ply block** for k = 1 … `walked_plies`, printed even without facts: `Ply k: <line_san[k-1]>
  (<mover>)`, the mover being the side to move at the start for odd k and the other side
  for even k; `Position: <line_fens[k-1]>`; then the ply-k facts.
- Fact rows are indented two spaces. Within a block: the move's tactic labels of that ply
  first, then the groups in `impact_order`, each group's rows in today's order.
- **Where a fact goes**: parried threat and created threat → ply 1; motif and root motif →
  its `ply` (root motifs are ply 0); tactic label → its `ply`; tempo value after the move →
  ply 1; tempo gained → its `ply`; initiative run → its `start_ply`; positional change → its
  `ply`.
- **Row changes** (only inside blocks): motif and tactic-label rows drop `ply k, ` / `(ply k)`
  (a motif ends `(direct) [rule]`); the after-move tempo row reads `Tempo value: …` without
  `After the move: `; the tempo-gained row drops ` (ply k)`; a positional row drops the
  `By ply N: ` prefix and its `(ply k…)` part and ends ` (net change to ply N) [rule]` with N
  = `walked_plies`. Every other row is unchanged.
- No move shown (empty selection, terminal position): one start block without ply-0
  facts after the selection row, no separator.

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
| M3 | C++ (part 1b) | `include/chesswhy/move_report.hpp` (`MoveReport::line_fens`), `src/report/move_report.cpp` (§2.6), `src/render/render.cpp` (§2.5), `schema/moves.v8.schema.json`, `tests/report/move_report_tests.cpp`, `tests/render/render_unit_tests.cpp`, `tests/render/golden/*.txt` (regenerated), `README.md` (the layout) | Unit tests for: `line_fens` after two plies of a known line; a move report text whose facts sit at plies 0, 1, 3 and whose positional change sits at ply 2 prints them in those blocks with the §2.5 row forms; tempo gained and initiative in their plies; empty selection prints one start block; full report text unchanged (its goldens unchanged) |
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

## 5. Contract changes

| Date | Change | By |
|---|---|---|
