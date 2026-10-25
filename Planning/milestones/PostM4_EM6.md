# PostM4_EM6 — plan (search phase and explanation phase)

Branch `postm4-em6`, from `postm4-em5` after PostM4_EM5 part 1 closed (`902682b`; that branch
is not merged into `main`). This milestone runs before PostM4_EM5 parts 2 and 3. Source: the
Captain's position F47 (`2brr3/1p4k1/2p1P3/p4P2/P4pN1/2P4B/5K2/6R1 b - - 0 1`), where
ChessWhy says `Rd3` is best at every budget and Stockfish, run longer or as a single line,
says `Rh8`. Terms are in `Planning/DBC/DEFINITIONS.md` §3.9.

## 1. Captain's decisions (2026-10-25)

| # | Topic | Decision |
|---|---|---|
| H1 | Audience level | Remove `--level` and `input.level`. Whether M5 has audience levels becomes an open question (vision §11) |
| H2 | Budgets | Replace `--budget quick\|standard\|deep` and the node table by two depths the user may set: `--search-budget N` and `--explanation-budget N` ("the depth setting should be controllable if the user wants different settings") |
| H3 | Search shape | Step 1a, search A-best: the best line alone at the search budget. Step 1b, search A-rest: the other requested lines at the same depth, without A-best's move. Step 2, the explanation phase: every explanation search at the explanation budget |
| H4 | Defaults | Search budget 18, explanation budget 14 |
| H5 | Tie rule | An A-rest line whose engine score is at least A-best's is verdict `best` with loss 0 |
| H6 | Timing | No time limit (checkpoint 2, 2026-10-25): `cli_timing` measures each F fixture's default run and fails only when a run fails; speed is deferred until the tool works correctly ("make it correct > make it fast > make it nice"). Frozen before: `cli_timing` 25 s per F fixture; F32 smoke `--moves playable` ≤ 60 s, `--moves all` ≤ 150 s |
| H7 | Milestone shape | Own milestone, before PostM4_EM5 parts 2–3; `--no-llm` stays |

## 2. Evidence at freeze

Stockfish 19 from `assets/`, Threads 1, Hash 64, `ucinewgame` before each search, i7-8565U,
the 41 F fixture roots (`/tmp/calib.log`, Deputy, 2026-10-25).

| Measurement | Median | Max |
|---|---|---|
| Search A today (MultiPV 5, 1M nodes at standard) | 2.35 s | — |
| A-best at depth 18 | 0.74 s | 3.71 s |
| A-best at depth 20 | 1.77 s | 25.7 s (F08) |
| A-best + A-rest (MultiPV 4) at depth 18 | 3.5 s | 11.3 s (F08) |
| Null-move search at depth 14 (B, E) | 0.07 s | — |
| MultiPV 5 at depth 14 (forced test) | 0.81 s | 2.41 s |
| MultiPV 5 at depth 12 | 0.34 s | 0.94 s |
| MultiPV 5 at 30k nodes (forced test today) | 0.075 s | 0.14 s |

- Next slowest A-best + A-rest at depth 18: F12 9.9 s, F03 9.6, F36 9.6, F05 9.5, F37 9.5, F01 9.4,
  F35 9.3.
- F32 whole-board keyword (A-rest over every other move): depth 16 22 s, depth 18 42 s, depth 20 94 s.
- F47 at depth 18: A-best `Rh8` −15; A-rest `Rd3` −3, `Bxe6` −30, `Rxe6` −141, `b5` −227 (4.2 s).
  At depth 20: A-best `Rh8` −13; A-rest `Rd3` −11, `Bxe6` −29, `Rxe6` −147, `Rd2+` −268. MultiPV 5 at
  depth 20 and 22 puts `Rd3` first: the width of a search moves its top line.
- Timing risk: the forced test runs MultiPV 5; at depth 14 it costs about ten times today's
  30k-node search. Reports today hold up to 9 forced tests (F34; F46 8; F11, F38 6) and F32
  `--moves all` 22. Checkpoint 2 measures it.

## 3. Contract (frozen 2026-10-25)

### 3.1 Notation and scope

- `S` is the side to move in the analysed position; `K = multipv_lines(selection, legal)`
  (unchanged: 5, `N` for `topN` with `N > 5`, every legal move for a whole-board keyword).
- Search A splits into A-best and A-rest (not A1/A2: A1–A5 name the PostM4_EM3 tempo
  decisions). Search phase: A-best, A-rest, C. Explanation phase: B, D, E, R and every L search (threat, reply,
  forced test, reply per acceptable alternative).
- Not in scope: the forced test's cost (PostM4_EM5 part 2), VER2-1's verdict of a C move
  (PostM4_EM5 part 3, R2: it will use N6's score order), audience levels (M5), multi-threaded
  or time-limited searches.

### 3.2 Rules

| # | Rule |
|---|---|
| N1 | **Budgets.** `--search-budget N` and `--explanation-budget N`, integers 1–60, defaults 18 and 14; the value is a Stockfish depth. Any other value: `invalid integer for --search-budget: X` (or `--explanation-budget`), exit 2; a repeated flag: the existing duplicate-flag error. `--budget` and `--level` are unknown flags: `unknown flag --budget`, `unknown flag --level`, exit 2 |
| N2 | **Depth searches.** Every search is `go depth D` with no node or time limit: `D` is the search budget for A-best, A-rest and C and the explanation budget for B, D, E, R and L. Threads, Hash, `ucinewgame` per search and `searchmoves` as today |
| N3 | **A-best.** MultiPV 1 over every legal move. Its line is candidate line 1: the best move, `eval_white` and `S`'s threat source as today. No line: engine error `Stockfish search A-best returned no principal variation` |
| N4 | **A-rest.** When the position has at least two legal moves: MultiPV `K − 1`, `searchmoves` every legal move except A-best's first move. Its lines, in A-rest's order and never re-sorted, are candidate lines 2 onward. No line: engine error `Stockfish search A-rest returned no principal variation`. One legal move: no A-rest |
| N5 | **Loss and verdict.** Loss = max(0, ES(A-best) − ES(m)), unchanged. A candidate is `best` iff it is playable and its engine score is at least A-best's in the score order (N6); otherwise playable is `okay`; the inaccuracy, mistake and blunder cutoffs and the allows-tactic rule are unchanged. An asked move outside the candidates (C) is at most `okay`, unchanged until PostM4_EM5 R2 |
| N6 | **Score order.** From `S`'s view: a mate for `S` ranks above every cp score, a shorter one higher; cp scores rank by value; a mate against `S` ranks below every cp score, a longer one higher. Equal cp or equal mate is a tie, so a line tied with A-best stays `best` as before |
| N7 | **Root opportunity.** The alternative is A-rest's first line; margin = ES(A-best) − ES(A-rest line 1), not clamped (may be negative: no opportunity). One legal move: forced, as today |
| N8 | **Playable truncated.** `true` iff A-rest returned all `K − 1` lines, `K` is below the number of legal moves, and every candidate is playable |
| N9 | **Hang guard.** A search waits for `bestmove` without a total deadline; it fails with `engine search timed out` only when the engine prints nothing for 120 s. Startup and `isready` keep their deadlines |
| N10 | **Search order.** A-best, A-rest, B, then per candidate and asked move the explanation searches in today's order, with C before its asked move's explanation searches |
| N11 | **Width independence of the best move.** A-best's request depends only on the position and the search budget, so line 1 is the same for every `--moves` value at the same budgets |

### 3.3 Report v10

`schema/report.v10.schema.json` and `schema/moves.v10.schema.json` replace v9;
`kSchemaVersion = 10`.

| Field | v10 | Source |
|---|---|---|
| `input.level` | removed | — |
| `input.budget` | removed | — |
| `input.search_budget` | integer 1–60 | rule |
| `input.explanation_budget` | integer 1–60 | rule |
| `engine.nodes` | removed (`A`…`R`) | — |
| `engine.multipv` | `K`: A-best's line plus the `K − 1` asked of A-rest (1 with one legal move) | rule |
| `engine.nodes_searched` | unchanged: sum of every search's `nodes` | engine |
| `engine.reproducible` | unchanged; same configuration now means the same budgets, threads and hash | rule |
| `candidates[].depth`, line `depth` | unchanged; equals the budget of the search that produced it, or more | engine |

The moves document's copies of `input` and `engine` change the same way.

### 3.4 Headers

```cpp
// report.hpp — removed: enum Budget, enum Level, NodeBudget, to_string(Budget), to_string(Level)
inline constexpr int kSchemaVersion = 10;
struct InputInfo {
    // ... unchanged fields up to selection ...
    int search_budget = 18;      // depth of A-best, A-rest, C
    int explanation_budget = 14; // depth of B, D, E, R, L
};
struct EngineInfo {
    std::string name;
    int threads = 1;
    int hash_mb = 64;
    int multipv = 5;                  // K
    bool reproducible = true;         // false iff threads != 1
    std::uint64_t nodes_searched = 0;
};

// engine.hpp
struct SearchRequest {
    std::string fen;
    int depth = 0;                            // go depth; >= 1
    int multipv = 1;
    std::vector<std::string> searchmoves_uci; // empty: all legal moves
};

// analysis.hpp — removed: node_budget, search_a_nodes, same_score
inline constexpr int kDefaultSearchBudget = 18;
inline constexpr int kDefaultExplanationBudget = 14;
inline constexpr int kMaxBudget = 60;
struct Request {
    chess::Board board;
    Selection selection;
    std::vector<chess::Move> asked;
    int search_budget = kDefaultSearchBudget;
    int explanation_budget = kDefaultExplanationBudget;
};
// N6: true iff `move` ranks at least as high as `best` for the side the scores belong to.
[[nodiscard]] bool at_least_score(const Eval& move, const Eval& best);
[[nodiscard]] Verdict verdict(int loss_units, bool at_least_best_score, bool allows_tactic);

// cli.hpp Options — `level` and `budget` replaced by:
int search_budget = analysis::kDefaultSearchBudget;
int explanation_budget = analysis::kDefaultExplanationBudget;
```

`src/cli/cli.cpp` `parse_integer` gains a maximum; `--help` lists the two flags with their
range and defaults and drops `--level` and `--budget`.

### 3.5 Text

Both texts replace the header row `Budget: standard, nodes A=… [rule]` by
`Budgets: search depth 18, explanation depth 14 [rule]` (the values in the report). No other
text change.

### 3.6 Fixtures

| Fixture | Change |
|---|---|
| F47 (new) | `2brr3/1p4k1/2p1P3/p4P2/P4pN1/2P4B/5K2/6R1 b - - 0 1`, `--moves best,Rd3` (checkpoint 2; `--moves playable` before, where A-rest's 21 lines score Rd3 cp −18, verdict `okay`, loss 0.0005: width dependence): candidate 1 `Rh8` (`e8h8`); `Rd3` (`d8d3`) a candidate with verdict `best` and loss 0 (N5, N6) |
| F32 (changed) | Also asserts N11: candidate 1 is the same move under `best`, `top6` and `playable` |
| F29, F30 | `--budget quick` becomes `--search-budget 12 --explanation-budget 10` |
| E14 | Expects `unknown flag --level` |
| E23–E26 (new) | `--budget quick` → `unknown flag --budget`; `--search-budget 0`, `--explanation-budget 61`, `--search-budget x` → `invalid integer for …`; all exit 2 |
| Every F fixture | Re-blessed at the default budgets. Best-move changes seen at depth 20 during calibration: F28 `e1g1`, F31 `e2d1`, F32 `f8c5` (`g8f6` at depth 18), F44 `e1g1`, F46 `c6d4`; WP4 lists old and new values for checkpoint 2 |

Unit tests that pin the node table (`analysis node budgets match the frozen search table`,
`selection sets Search A width and nodes`) are rewritten to the new contract or removed;
the same-score unit test becomes an N6 score-order test (cp, mate for, mate against, ties).

### 3.7 Work packages

| WP | Owner files | Builds | Depends on |
|---|---|---|---|
| WP1 analysis, engine, report, CLI | `include/chesswhy/{report,engine,analysis,cli}.hpp`, `src/engine/*`, `src/analysis/*`, `src/report/report.cpp`, `src/cli/*`, `schema/report.v10.schema.json` (rename from v9), `tests/analysis/*`, `tests/engine/*`, `tests/report/report_unit_tests.cpp`, `tests/report/example_reports.hpp`, `tests/cli/unit_tests.cpp`; schema-version mentions anywhere | N1–N4, N7–N11; §3.3–§3.4 | — (commits headers first) |
| WP2 move report and render | `src/report/move_report.cpp`, `schema/moves.v10.schema.json`, `tests/report/move_report_tests.cpp`, `src/render/*`, `tests/render/*` and goldens | §3.3 moves document, §3.5 | WP1 headers |
| WP3 checker | `tools/validate_report.py`, `tests/checker/*` | N5–N8 in the oracle: `at_least_score` for `best`, v10 keys and ranges, `playable_truncated` under N8, margin unclamped | — |
| WP4 fixtures and timing | `tests/fixtures/positions.json`, `tests/cli/*` except `unit_tests.cpp`; the `CHESSWHY_CHECK_TIMING` description in `CMakeLists.txt` | §3.6, H6 (`run_timing` measures, no limit) | WP1–WP3 |
| WP5 docs | `README.md` | flags, defaults, ranges, `cli_timing` as a measurement, report v10 fields | WP4 |

### 3.8 Checkpoints

1. Before spawning: the Captain sees the changed diagrams (`Searches`, `SearchA` L3,
   `Verdict` L3, `Question`).
2. After WP4: the re-bless list (old and new best move and verdicts per F fixture), F47's
   report, `cli_timing` per fixture with F08, F12 and F34 named, and the F32 smoke times.
   Any breach of H6's limits goes to the Captain as a decision brief; budgets and limits never
   change silently.

### 3.9 Acceptance

- [ ] `chesswhy --fen <F47> --moves best,Rd3 --full-report --json`: candidate 1 `e8h8`;
      `d8d3` a candidate with verdict `best`, loss 0.
- [ ] F32 candidate 1 is identical under `--moves best`, `top6` and `playable` (N11).
- [ ] `--budget` and `--level` are rejected as unknown flags; out-of-range and non-integer
      budgets give `invalid integer for …`, exit 2 (E14, E23–E26).
- [ ] Reports are v10: `input.search_budget` and `input.explanation_budget` present, no
      `level`, `budget` or `engine.nodes`; both schemas validate every fixture report.
- [ ] The checker applies N6's at-least test and N8 and passes on every fixture report.
- [ ] Two builds of the same report are byte-identical (`cli_rerun_identity`).
- [ ] `cli_timing` passes with no time limit (H6, checkpoint 2); the receipt records each
      fixture's time and the F32 `--moves playable` and `--moves all` smoke times.
- [ ] Milestone-end gate per AGENTS.md; DBC files current; `tools/check_dbc.py` passes.

## 4. Contract changes

| Date | Change | By |
|---|---|---|
| 2026-10-25 | Checkpoint 1 passed: diagrams approved. Forced-test timing risk (MultiPV 5 at depth 14, about 10× today's cost): build as frozen and measure at checkpoint 2 (option 1) | Captain |
| 2026-10-25 | Checkpoint 2 passed. Re-bless list approved (best-move changes F15, F28, F31, F44, F46; verdict changes F05, F12, F32). H6 time limits removed; `cli_timing` stays as a measurement (measured at 18/14: 14 default runs over 25 s, slowest F01 47.2 s; total 821 s). F47 asks `--moves best,Rd3` instead of `--moves playable`. Defaults stay 18/14. Speed is deferred, not dropped: a later milestone makes it fast (the explanation phase at depth 14 costs 16–30 s on F01, F08, F12; the search phase 14–16 s) | Captain |
