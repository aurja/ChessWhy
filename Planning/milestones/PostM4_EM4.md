# PostM4_EM4 — contract (part 1: motif rows)

Branch `postm4-em4` (from `postm4-em3` at `92dd918`). The rest of EM4's scope (the pure reply
definition "forced and gains nothing" and whatever the conceptual critique of all
definitions and concepts finds) is frozen later, after the critique; this file then gains
part 2.

## 1. Captain's decision

| # | Topic | Decision |
|---|---|---|
| R1 | Motif rows in move reports (2026-10-24) | Option 3: merge repeated restricted-movement motifs in the move report, and print restricted-movement rows compactly in the move report text. Measured before the decision on the 53 F fixture runs' move reports: 132 motif rows, 125 after the merge (V2: 27 → 22); most of the length is inside each row (the restricted and free move lists) |

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

## 3. Work packages

| WP | Owner | Files | Done when |
|---|---|---|---|
| M1 | C++ | `include/chesswhy/report.hpp` (`kSchemaVersion`; `Motif::also` with `MotifLabel {tag, threat}`, empty in the full report and not serialised there), `src/report/move_report.cpp` (merge in `relevant_motifs`, serialise `also`), `src/render/render.cpp` (§2.3), `schema/*.v8.schema.json` (git mv from v7 + §2.2), `tests/report/move_report_tests.cpp`, `tests/report/report_unit_tests.cpp`, `tests/render/render_unit_tests.cpp`, `tests/report/CMakeLists.txt` and `tests/cli/CMakeLists.txt` (schema paths), `README.md` (v7 → v8, the compact row), `include/chesswhy/move_report.hpp` (comment) | Unit tests for: a relative pin and a tactical pin with equal fields merge (order kept, `also` filled); a differing `restricted` keeps two entries; a differing `contribution` keeps two entries; non-restricted motifs get `also: []`; the compact row text for a merged and an unmerged entry; full report text unchanged |
| M2 | Python | `tools/validate_report.py`, `tests/checker/unit_tests.py` | v8 schema names; projection merge (§2.4); mutants that fail: an unmerged repeat, a dropped `also` entry, an `also` in the wrong order |

## 4. Acceptance

1. Release: affected unit tests, `report_schema`, F41 and the F tests whose move reports
   contain restricted-movement motifs pass; `check_dbc` passes.
2. V2 smoke (`--moves Bxa4,Ba7`): no two restricted-movement rows of one move report differ
   only in their label; no `restricted:`/`free:` list in the move report text; the full
   report text still lists them.
3. Full report JSON identical to `92dd918` except `schema_version`.

## 5. Contract changes

| Date | Change | By |
|---|---|---|
