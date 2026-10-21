# PostM2_EM1 — Tactic recognition and motif contribution

Status: **frozen** on branch `postm2-em1` (2026-10-20), except two items the Captain
approves at the checkpoints in §12: the C fixture table with the new F rows (§9), and the
resolution of every mismatch between the contribution rule and the hand labels (§4.3,
checkpoint 2). Changes to anything else are contract changes: the orchestrator makes
them, records them in §15 and tells the Captain.

Intent (vision §5.5, §11; M2 D11, D13): label a line a **tactic** when it beats the best
alternative by a frozen margin under best replies; judge each motif's **contribution** to
that tactic; report `removal_of_defender` as a tactic-side label. Acceptance intent:
tactic labels follow mechanically from the engine lines; contribution judgements match
hand-labelled fixtures. Motif recognition (M2) does not change.

## 1. Captain's decisions

Briefed 2026-10-20 as P1–P5; the Captain approved every recommendation.

| # | Topic | Decision |
|---|---|---|
| E1 | Order | PostM2_EM1 before M3. M3 (game statistics) is shelved until this milestone closes |
| E2 | Threshold (P1) | A line is a tactic iff its only-move margin is ≥ 0.13 ES (`kMistakeUnits`, 260 es_units): every alternative is at least a mistake. Reuses the frozen verdict boundary. Known limit: ES flattens near 0 and 1, so in an already decided position (mate against a second-best at 0.98) no line is a tactic |
| E3 | Lines judged (P2) | The analysed position's best line (search A) and every refutation. A refutation gets one extra search R (§3) for its best alternative. Threat lines of B and E are not judged (scope proposal). Existing searches and lines are unchanged, so no M1/M2 engine fixture is re-blessed |
| E4 | Which move (P3) | Only the best move can be a tactic; every other candidate's margin is ≤ 0. Known limit: with two equally winning moves, neither is a tactic |
| E5 | One legal move (P4) | No tactic judgement by margin. The report states the move is **forced** (rule fact). Applies to the root and to a refutation whose side has exactly one legal reply |
| E6 | Contribution (P5) | Structural rule (§4.3), provenance `rule`; measured against hand-labelled C fixtures that the Captain reviews (as M2 D9). Filled only on tactic lines; null elsewhere |
| E7 | `removal_of_defender` | Former M2 definition reinstated (§4.4) as a **tactic label**, not a motif: reported only on tactic lines, provenance `rule`, outside the motif reference implementation |

## 2. Contract files

Created by the orchestrator in the contract commit, before batch 1:

| File | Contents |
|---|---|
| `include/chesswhy/report.hpp` | `TacticLabelTag`, `TacticLabel`, `TacticAlternative`, `TacticJudgement`; `NodeBudget::r`; `MoveAssessment::tactic`; `Threat::tactic`; `kSchemaVersion = 3` (§6) |
| `include/chesswhy/tactics.hpp` | `tactic_labels`, `judge_contributions` (§6) |
| `include/chesswhy/analysis.hpp` | `kTacticMarginUnits`, `is_tactic_margin` (§6) |
| `src/report/report.cpp` | `to_string(TacticLabelTag)` only, so WP1 does not wait for WP3 |
| `schema/report.v3.schema.json` | replaces `report.v2.schema.json` (deleted; git history keeps it) |
| `src/tactics/CMakeLists.txt` | `tactic.cpp` added to `chesswhy_tactics`, guarded while missing; the guard is removed at batch 1 integration |
| `tests/cli/CMakeLists.txt`, `tests/report/CMakeLists.txt` | schema validation points at `report.v3.schema.json`. `tactics_unit_tests` finds `tactics.json` through `CHESSWHY_SOURCE_DIR`, like `motifs.json` (no CMake change) |
| `tests/fixtures/tactics.json` | C fixtures (§9): format in its `comment`, empty list until checkpoint 1 |

Workers implement against these files and do not change them. A header that turns out
to be wrong goes back to the orchestrator with a concrete reason.

## 3. Scope

- One new search kind, **R(m)**: from the position after a move `m` that is not fine
  (`O` to move), MultiPV 1, `searchmoves` = every legal move of `O` except the
  refutation's first move `r1`, in chess-library generation order. It answers: how good is
  `O`'s best alternative to the refutation? R runs iff `m`'s verdict is not fine, the
  refutation has at least one ply, and `O` has at least two legal moves.
- Budgets (M1 §3 plus column R; R uses the size of D and E):

  | Budget | A | B | C | D | E | R | Worst case |
  |---|---|---|---|---|---|---|---|
  | quick | 250k | 50k | 50k | 25k | 25k | 25k | ≈ 0.8 M |
  | standard | 1M | 200k | 200k | 100k | 100k | 100k | ≈ 3.2 M nodes |
  | deep | 4M | 800k | 800k | 400k | 400k | 400k | ≈ 12.8 M |

  Worst case = A + B + C + 6 × (D + E + R). At `standard` R adds at most 0.6 M nodes,
  ≈ 1.6 s [INFERENCE: M1's 2.6 M nodes ≈ 7 s]; the slowest F fixture was 7.98 s in M2.
- Search order (fixed): A, B, then per candidate in MultiPV order D, E, R; then for an
  asked move outside A: C, D, E, R. Every search starts with `ucinewgame`, so R changes
  no other search's result.
- Search A must return at least two lines when the position has at least two legal
  moves; otherwise analysis fails with the existing engine error.
- Tactic judgements, contributions and labels are annotations (as M2 D7): they never
  change verdicts, urgency, `quiet`, consequences, lines or motif lists.
- One new motif tag, `pin_tactical` (§4.5, batch 3, added 2026-10-21); the M2 §4
  definitions are unchanged, T fixtures grow by T42–T53 and the oracle covers the new tag.

## 4. Rules

Notation: a judged line starts at its **root** R₀ with tactic side X to move (X = `S` for
the root line, X = `O` for a refutation). Its **tactic sequence** is the line cut to its
walked plies (M1 §4.3, `walked_plies`): moves mv₁ … mvₙ, boards P₀ = R₀ … Pₙ.

### 4.1 Tactic judgement

| Line | Line ES (X's view) | Best alternative | Source |
|---|---|---|---|
| root: `candidates[0]` | `candidates[0].eval` | `candidates[1]` (A's second line) | engine |
| refutation of `m` | 2000 − `m.eval.es_units` (the line `m` was evaluated with) | R(m)'s line | engine |

- **only-move margin** = line ES − alternative ES, in es_units. May be negative (A's
  MultiPV order is by score; R is a separate search).
- **tactic** iff margin ≥ `kTacticMarginUnits` (= `kMistakeUnits` = 260, 0.13 ES).
- **forced** iff R₀ has exactly one legal move. Then no alternative and no margin; never
  a tactic; source `rule`.
- `candidates[1..]` are not judged (E4). The asked move copies its candidate's judgement
  when it is in A; an asked move from C is not judged. Every assessed move's refutation is
  judged as above, including the asked move's.
- A refutation with no ply (the move's line ends after `m`) is not judged.

### 4.2 Where contributions and labels go

On a **tactic line** (judgement with `is_tactic`), with the same root and tactic sequence:

- root tactic: `contribution` set on every motif of `position.motifs`,
  `candidates[0].motifs` and `threats.<S>.motifs` (the same line as `candidates[0]`);
  `candidates[0].tactic.labels` = `tactic_labels`.
- refutation tactic: `contribution` set on every motif of `refutation.motifs`;
  `refutation.tactic.labels` = `tactic_labels`.
- Everywhere else `contribution` stays null and `labels` empty. A move's own `motifs` stay
  null even when its refutation is a tactic (they belong to `m`'s line, which is not judged).

### 4.3 Contribution rule

For a motif μ at ply k (0 ≤ k ≤ n) with pieces = its actor and targets, standing on their
squares on Pₖ:

1. side(μ) ≠ X → `none`.
2. **direct** iff μ is a move motif (M2 §4.2; produced by mvₖ, a move of the sequence),
   or some X move mvⱼ with k < j ≤ n captures a Y piece of μ (en passant captures the
   passed pawn). Y's moves, and X moves that only move an X piece of μ, do not count.
3. **indirect** iff not direct, and some X move mvⱼ with k < j ≤ n, not castling, has a
   destination square that a **Y** piece of μ attacks on Pⱼ₋₁, while no move
   mvₖ₊₁ … mvⱼ has moved or captured any piece of μ. Attacks are the piece's
   pseudo-attacks with Pⱼ₋₁'s occupancy, pins ignored (a pinned piece still "would
   defend").
4. Otherwise `none`.

Plain meaning (for hand labelling, M2 D11; tightened 2026-10-21, §15): direct = the
tactic's moves make the motif or capture its pieces; indirect = the motif stops a Y piece
from answering an X move (a pin that stops a recapture, an overloaded defender that
cannot cover both squares); none = the tactic would work the same without it. An X
piece of the motif guarding a square is that piece's own work, not the motif's.

### 4.4 `removal_of_defender` (tactic label)

For X only, on the tactic sequence: mvₖ is an X move that captures a Y piece d; on Pₖ₋₁
there is a Y non-king piece t ≠ d with `see(Pₖ₋₁, t, X) = 0` and `see ≥ 1` on the same
board with d removed; an X move mvⱼ, k < j ≤ n, not en passant, captures on t's square,
and `see(Pⱼ₋₁, t's square, X) ≥ 1`. One label per (k, t).

| Field | Value |
|---|---|
| `ply` | k |
| `actor` | the capturing piece on its destination square, as on Pₖ |
| `targets` | d (capture square, piece as on Pₖ₋₁), then t (as on Pₖ₋₁) |

Order: by ply, then t's square. After lichess-puzzler's `capturing_defender` (M2 §13
credit). Seed implementation: `5cc9582^:src/tactics/motifs.cpp`
(`append_removal_of_defender`); seed fixtures: T33, T34 in
`2e4104e^:tests/fixtures/motifs.json`.

### 4.5 `pin_tactical` (state motif)

Added 2026-10-21 (Captain, §15). A Y piece is held in place by what X could do if it
left. State motif, computed for both sides like M2 §4.1; source `rule`; fixture-only (no
Lichess mapping).

On board B (not terminal, Y not in check), let Bx = B with X to move (B itself, or B
after a null move when Y is to move). For each Y non-king piece p, let B−p = Bx with p
removed (a removed rook also clears its castling right). Skip p when the Y king is
attacked on B−p (p is absolutely pinned; `pin_absolute` reports it). p is restricted
when removing it enables at least one **threat**, of these kinds, in this order:

| Kind | Enabled on B−p, not on Bx |
|---|---|
| `fork`, `skewer`, `discovered_check`, `discovered_attack`, `double_check`, `back_rank_mate`, `promotion` | X's legal move m = a→b on B−p produces this M2 §4.2 move motif (side X); on Bx, m is illegal or does not produce a motif of this tag |
| `checkmate` | X's legal move m on B−p gives checkmate; on Bx, m is illegal or does not |
| `capture` | A Y non-king piece t ≠ p with `see(Bx, t, X) ≤ 0` and `see(B−p, t, X) ≥ 1`; m = X's legal capture of t on B−p by the least valuable piece (lowest square among equals); no legal capture, no threat |

A threat counts whether p guarded b, guarded a piece, or blocked a line: empty guarded
squares are included. One motif per (a, p, kind), with the lowest b:

- actor: a, the X piece making m;
- targets: p, then the threat's targets on B−p (the move motif's targets in M2 §5
  order; `checkmate`: the Y king; `capture`: t; `promotion`: none);
- `threat`: `{kind, square: b}`. Set iff the tag is `pin_tactical`.

Identity (M2 §5) adds `threat` for every tag (null equals null). Order: M2 §5, with
`pin_tactical` after `trapped_piece`, then threat kind in the order above, then b.
Reference implementation in `tests/tactics/oracle/reference.cpp`; `tactics_oracle` stays
at zero mismatches.

### 4.6 Not in scope

Threat tactics (B and E lines), tactics by moves other than the best, multi-move
"quiet" tactic detection beyond the margin, tactic names (a tactic is not called a fork
etc.; motifs and their contributions carry that), motif tags other than `pin_tactical`,
trimming motif lists for the reader (comprehensive first, Captain 2026-10-21), the
recall-miss scope proposals from the M2 audit.

## 5. Report v3

- `schema_version` 3; `schema/report.v3.schema.json`. v2 is released on `main`, so the
  new required keys bump the version.
- `engine.nodes.R`: node budget of search R (upper case like `A`–`E`).
- `tactic` (required, nullable) on `MoveAssessment` and on `Threat`. Set only where §4.1
  judges: `candidates[0]` (and the asked move when it copies it) and judged refutations.
  Example (values illustrative):

```json
"tactic": {
  "source": "engine",
  "forced": false,
  "alternative": {"san": "Ng6", "uci": "h4g6",
                  "eval": {"source": "engine", "es": 0.0005, "cp": -950, "mate": null}},
  "margin": 0.4890,
  "is_tactic": true,
  "labels": [
    {"source": "rule", "tag": "removal_of_defender", "side": "black", "ply": 1,
     "actor": {"square": "d5", "piece": "b"},
     "targets": [{"square": "d5", "piece": "N"}, {"square": "e4", "piece": "R"}]}
  ]
}
```

- `source`: `engine`, or `rule` iff `forced`. `forced` → `alternative`, `margin` null,
  `is_tactic` false, `labels` empty. `alternative.eval` is from X's view, in the existing
  `$defs/eval` format. `margin` = margin units / 2000, printed like `es`. `labels` empty
  unless `is_tactic`.
- `Motif.contribution`: filled per §4.2–4.3; format unchanged from v2.
- `Motif.tag` adds `pin_tactical` (§4.5). New required, nullable key `Motif.threat`
  (after `gain`): `{"kind": "fork", "square": "f4"}` iff the tag is `pin_tactical`, null
  otherwise. Kinds: `fork`, `skewer`, `discovered_check`, `discovered_attack`,
  `double_check`, `back_rank_mate`, `promotion`, `checkmate`, `capture`.
- Nothing else changes; every v2 field keeps its meaning.

## 6. Headers

```cpp
// report.hpp
enum class TacticLabelTag : std::uint8_t { RemovalOfDefender };
[[nodiscard]] std::string to_string(TacticLabelTag tag); // "removal_of_defender"
struct TacticLabel {                                     // §4.4; source rule
    TacticLabelTag tag = TacticLabelTag::RemovalOfDefender;
    Side side = Side::White;                             // X
    int ply = 0;
    PieceOnSquare actor;
    std::vector<PieceOnSquare> targets;                  // d, then t
};
struct TacticAlternative {                               // best alternative first move
    std::string san;
    std::string uci;
    Eval eval;                                           // X's point of view
};
struct TacticJudgement {                                 // §4.1
    bool forced = false;                                 // exactly one legal move; source rule
    std::optional<TacticAlternative> alternative;        // null iff forced
    std::optional<int> margin_units;                     // line ES − alternative ES; null iff forced
    bool is_tactic = false;                              // margin_units >= kTacticMarginUnits
    std::vector<TacticLabel> labels;                     // empty unless is_tactic
};
// NodeBudget: std::uint64_t r = 0;   // after m, searchmoves all but r1 (search R)
// Threat:         std::optional<TacticJudgement> tactic;  // set only on judged refutations
// MoveAssessment: std::optional<TacticJudgement> tactic;  // set only on candidates[0] (and its copy)
// kSchemaVersion = 3
// MotifTag: PinTactical after TrappedPiece (§4.5)
enum class ThreatKind : std::uint8_t { Fork, Skewer, DiscoveredCheck, DiscoveredAttack,
                                       DoubleCheck, BackRankMate, Promotion, Checkmate, Capture };
[[nodiscard]] std::string to_string(ThreatKind kind);   // "fork", ..., "capture"
struct MotifThreat { ThreatKind kind; std::uint8_t square; };  // b
// Motif: std::optional<MotifThreat> threat;   // set iff PinTactical; part of identity

// tactics.hpp (pure, provenance rule)
// §4.4 labels for the side to move at `root`; `line` already cut to the walked plies.
[[nodiscard]] std::vector<TacticLabel> tactic_labels(const chess::Board& root,
                                                     std::span<const chess::Move> line);
// §4.3: sets `contribution` on every motif (plies 0..line.size()); X = side to move at `root`.
void judge_contributions(const chess::Board& root, std::span<const chess::Move> line,
                         std::span<Motif> motifs);
// §4.5 cost switch; default computes everything (the analysis never changes it)
struct MotifOptions { bool pin_tactical = true; };
// position_motifs(board, MotifOptions options = {}); line_motifs(root, line, MotifOptions options = {})

// analysis.hpp
inline constexpr int kTacticMarginUnits = kMistakeUnits;
[[nodiscard]] bool is_tactic_margin(int margin_units); // margin_units >= kTacticMarginUnits
```

The contract commit's headers are authoritative; the block above is a summary.

## 7. Puzzle check (not gating)

Lichess puzzles are built as only-move lines, and a Lichess theme names the motif behind
the puzzle's tactic (TERMINOLOGY §5.3). `tactics_puzzle_eval` treats each `test.csv`
solution line from the root as a tactic sequence and prints:

- **direct share**, per measured theme: on the theme's stratum, among puzzles where the
  theme is predicted (TP), the share where at least one mapped instance for the solver is
  judged `direct`.
- **`removal_of_defender` recall** on the `capturingDefender` stratum, and its Lichess
  agreement on background (M2 §7.3 counting).

Printed in the receipt for dev and test; never gating, never tuned against. `tactics_puzzles`
keeps its exit rule (M2 recall targets only).

## 8. Calibration and targets

None. The margin threshold (E2) and the rules in §4 are frozen. Rule changes happen only
at checkpoint 2, decided by the Captain per mismatch, each with a §15 row.

## 9. Fixtures

- **C fixtures** (`tests/fixtures/tactics.json`, `unit`, no engine), run by
  `tactics_unit_tests`: `{id, title, source, fen, line_uci, contributions,
  labels_include, labels_exclude}`. `line_uci` is a tactic sequence from `fen` (X = side
  to move), already cut to its walked plies. `contributions` lists **every** X-side motif
  of `position_motifs(fen)` and `line_motifs(fen, line)` other than `pin_tactical` as
  `{tag, side, ply, actor, targets, contribution}` (exact set: a missing or extra X motif
  fails). `pin_tactical` entries also carry `threat {kind, square}` and are checked only
  where listed (Captain, 2026-10-21: chosen instances, not an exact set). Label entries
  are `{tag, side, ply, actor, targets}`; any field may be omitted in `labels_exclude`.
  Squares as names; exact format in the file's `comment`.
- Coverage, at least: 6 fixtures whose key motif is `direct`; 3 with an `indirect` motif
  (pin stopping a recapture ×2, one free choice); 4 with a `none` motif;
  `removal_of_defender` 3 positives and 3 near misses (T33, T34 revived among them).
  IDs C01–Cnn. (Reduced 2026-10-21, §15.)
- **`pin_tactical` C rows**: C09 lists both of its fork instances: ply 0, actor d5,
  targets h4, e2, g2, threat fork f4, `direct` (black's next move Qxh4 captures h4); ply 1,
  actor d5, targets g3, e2, g2, threat fork f4, `direct` (black's next move Nxe2 captures e2).
- **T fixtures** (`tests/fixtures/motifs.json`, `unit`): T42 onward for `pin_tactical`,
  frozen in the contract commit. Includes carry `threat {kind, square}`; the motif
  harness matches it when given. At least one include per threat kind, plus excludes for
  a threat that already exists on Bx, an absolutely pinned p, a second guard, and Y in
  check; one fixture with Y to move.
- **Puzzle-corpus sampling** (Captain, 2026-10-21): `tactics_oracle` computes
  `pin_tactical` (detector and reference) only on puzzles whose 0-based row index in the
  CSV is divisible by 16, with zero mismatches required as for every tag;
  `tactics_puzzle_eval` and `tactics_puzzles` switch it off (no measured theme maps to it).
- **Hand labels**: WP4 labels from the plain meaning in §4.3, one-line reason per label,
  without running or reading the rule implementation. Lines must be tactics: a Lichess
  puzzle solution (CC0, puzzle ID as source) or a line checked by WP4 at `standard` to
  have a margin ≥ 0.13. World-championship and `constructed` positions allowed as in M2 §9.
- **F fixtures**: every M1/M2 expectation unchanged. New `equals` rows on
  `/candidates/0/tactic/...` and `/asked_move/refutation/tactic/...`; `motifs_include`
  entries take an optional `contribution`. New fixtures from F21, at least: a root
  tactic; a refutation tactic (the smoke position); a decided position whose best line is
  not a tactic (E2 limit); two equally winning moves (E4 limit). F02 gets
  `/candidates/0/tactic/forced: true`. Only values that hold at `quick`, `standard` and
  `deep`.
- **Invariant check** (F harness, every engine fixture at `standard`): non-terminal ⇒
  `candidates[0].tactic` set, `candidates[1..].tactic` null; `forced` iff one legal move;
  otherwise alternative = `candidates[1]` (san, eval), margin = es(c₀) − es(c₁),
  `is_tactic` = margin ≥ 0.13; every judged refutation has margin = (1 − es(m)) −
  `alternative.eval.es`, alternative ≠ the refutation's first move, same `is_tactic` rule;
  `labels` empty unless `is_tactic`; `contribution` non-null exactly on the §4.2 motif lists
  of tactic lines.

## 10. Template

- Under `candidates[0]`'s line and under each refutation, iff judged:
  `tactic: next best Ng6 is 0.49 worse [engine]` when `is_tactic`;
  `forced: only legal move [rule]` when `forced`; nothing when judged not a tactic.
- Label rows like motif rows: `removal_of_defender: b d5 → N d5, R e4 (ply 1) [rule]`.
- Motif rows show a non-null contribution inside the ply parentheses:
  `fork: n f3 → K e1, Q d4 (ply 1, direct) [rule]`.
- Restricted-movement wording (Captain, 2026-10-21): rows of the four restricted-movement
  tags print a phrase instead of the tag; all other rows print the tag as before. JSON
  tags are unchanged.
  - `pin_absolute`: `restricted movement (absolute pin): R e1 → n e5, k e8 (ply 0) [rule]`
  - `pin_relative`: `restricted movement (relative pin): …`
  - `overloaded_defender`: `restricted movement (overloaded defender): …`
  - `pin_tactical`: `restricted movement (tactic: Nd5-f4 fork): n d5 → P g3, Q e2, K g2
    (ply 1, direct) [rule]`. Inside the parentheses: the actor's piece letter upper case,
    actor square, `-`, threat square, the threat kind with `_` printed as a space.
- Wording pinned by golden tests; `position.motifs` still not printed.

## 11. Work packages

Each worker owns only the files listed, builds and runs its own targets, and returns
under 300 words. Batch 1: WP1, WP3, WP4. Batch 2: WP2, WP5. Batch 3 (`pin_tactical`,
§4.5): WP7, WP8, WP9. Milestone end: WP6.

| WP | Owned files | Done when | Non-goals |
|---|---|---|---|
| WP1 tactic rules | `src/tactics/tactic.cpp`, `tests/tactics/tactics_unit_tests.cpp` | §4.3, §4.4 implemented; unit tests at each rule's boundary (move motif, later capture, castling rook, en passant, pinned attacker, Y-side motif, ply n); C fixture harness; C fixtures pass once frozen (before that, own cases) | motif detectors, analysis wiring |
| WP2 analysis | `src/analysis/*`, `tests/analysis/*`, `tests/cli/*` | search R and budgets per §3; judgements per §4.1; contributions and labels per §4.2; `is_tactic_margin` unit tests (259, 260, negative); F harness invariant check and `contribution` in `motifs_include`; all F fixtures, rerun identity and timing pass | rule logic, fixture edits |
| WP3 report + render | `src/report/*` (except the contract's `to_string`), `src/render/*`, `tests/report/*`, `tests/render/*`, `tools/validate_report.py` | `to_json` emits v3 and validates for hand-built reports with forced, tactic, non-tactic and label cases; template per §10 with golden tests | analysis |
| WP4 fixtures | `tests/fixtures/tactics.json` (proposal), new rows in `tests/fixtures/positions.json` (proposal), throwaway scripts under `build/` | C table and F rows per §9, each line's tactic status checked; then the Captain's review | code; running WP1's rule |
| WP5 puzzle check | `tests/tactics/puzzle_eval.cpp` | §7 numbers printed for test (and dev locally); exit rule unchanged | rule logic |
| WP6 docs | `README.md`, `third_party/DEPENDENCIES.md` | §13 | — |
| WP7 `pin_tactical` detector | `src/tactics/motifs.cpp`, new `src/tactics/pin_tactical.cpp` (optional) and `src/tactics/CMakeLists.txt`, `src/tactics/tactic.cpp` (tag switch only), `tests/tactics/tactics_unit_tests.cpp` | §4.5 in `position_motifs`/`line_motifs` with `MotifOptions`; identity includes `threat`; M2 §5 order with `pin_tactical` after `trapped_piece`, then kind, then b; unit tests at each §4.5 boundary; T fixtures match `threat`; C harness per §9 | oracle, report, render |
| WP8 `pin_tactical` reference | `tests/tactics/oracle/*`, `tests/tactics/motif_oracle.cpp`, `tests/tactics/puzzle_eval.cpp` | independent reference of §4.5 written from this plan without reading WP7's code; oracle compares `threat`; sampling per §9; `tactics_oracle` zero mismatches | detector |
| WP9 report + render for `pin_tactical` | `src/report/*` (except the contract's `to_string`), `src/render/*`, `tests/report/*`, `tests/render/*` and goldens, `tools/validate_report.py` | `threat` key per §5 in `to_json`, validates against the v3 schema; restricted-movement wording per §10 in golden tests, including a `pin_tactical` row | detection |

## 12. Captain's checkpoints

1. After batch 1: review the C fixture table (hand labels with reasons), the new F rows
   and the smoke command (§9).
2. After batch 2: every C fixture where the rule and the hand label differ, with fixture,
   motif, both labels and the reason. The Captain decides per mismatch: change the rule
   (§15 row) or the label. The same invariant failing twice → design note (AGENTS.md).
3. Milestone end: try the smoke command and approve merging `postm2-em1`.

## 13. Credits and documentation

- `third_party/DEPENDENCIES.md`: the lichess-puzzler reference row adds
  "`removal_of_defender` after `capturing_defender`". No code used.
- `README.md`: tactic judgement, forced moves and contributions in the feature paragraph;
  C fixtures in the test section.
- Vision: §5.5 (tactic, margin, contribution, `removal_of_defender`), §6 sketch
  (`tactic`), §11 row status. `Planning/CURRENT_STATE.md`, receipt
  `Planning/receipts/PostM2_EM1.md`, `Planning/definitions/` pass (TERMINOLOGY,
  FIXTURES, TESTS).

## 14. Acceptance

1. Release and ASan/UBSan builds warning-free; all `unit` and `engine` CTests pass in
   both trees.
2. Tactic recognition is mechanical: `is_tactic_margin` boundary tests pass, and the §9
   invariant check holds on every F fixture.
3. C fixtures pass (`unit`): contributions exact, labels as listed, after checkpoint 2.
4. All F fixtures keep every M1 and M2 expectation unchanged and pass their new rows;
   every F report validates against `report.v3.schema.json`.
5. `tactics_oracle` zero mismatches (`pin_tactical` on the §9 sample and on every T
   fixture) and `tactics_puzzles` targets still met (M2 motifs unchanged); §7 numbers
   recorded in the receipt.
6. Rerun identity: byte-identical JSON as in M1 acceptance 4.
7. `standard` ≤ 12 s wall time per F fixture (Release); the receipt records the slowest.
8. Facts first: judgements carry `engine` (or `rule` when forced), labels and
   contributions `rule`; the template prints only report fields (golden tests).
9. Gates: clang-format and clang-tidy on changed C++; Ruff and basedpyright on changed
   Python; ShellCheck on changed scripts.
10. Docs (§13) updated.

Smoke command: `build/release/chesswhy --fen 'k7/8/8/8/3Q3n/8/2P5/4K3 w - - 0 1' --move
c3 --budget standard --json --no-llm`. `candidates[0].tactic.is_tactic` is false (all five
candidates at ES 1.0: the E2 limit); `asked_move.refutation.tactic.is_tactic` is true
with margin ≥ 0.13; the refutation's `fork` at ply 1 (knight f3, targets e1, d4) has
contribution `direct`.

## 15. Contract changes

| Date | Change | Reason |
|---|---|---|
| 2026-10-20 | Plan frozen (E1–E7) | Captain's approval of P1–P5 |
| 2026-10-20 | JSON key `engine.nodes.R`, not `r` | Existing node keys are `A`–`E`; the C++ field stays `NodeBudget::r` |
| 2026-10-20 | No `tests/tactics/CMakeLists.txt` change; `tactics.json` has a per-entry `reason` and a per-fixture `margin` (null for puzzles) | The unit test builds fixture paths from `CHESSWHY_SOURCE_DIR`; reasons and margin checks (§9) need a place in the file |
| 2026-10-20 | An asked move that is `candidates[0]` copies its motifs with their contributions together with its `tactic`; §4.2's root-tactic lists include that copy | Same line, same judgement; §4.2 named only `candidates[0]`, and the invariant check needs one answer |
| 2026-10-21 | §9: indirect coverage 3 (pin stopping a recapture ×2, free choice ×1); the overloaded-defender ×2 and landing-square-guard ×1 cases dropped | Captain, checkpoint 1: an indirect overload is rare and beyond players below titled level (a 50k-puzzle scan found none); a landing-square guard by an X piece is not the motif's work (C01 decision below) |
| 2026-10-21 | §4.3 rule 2: only X moves count, and only captures of a Y piece of μ; rule 3: only Y pieces of μ, and only while no piece of μ has moved or been captured | Captain, checkpoint 2. Y replies made pins direct (C10 Kxh7; C12, C13, C14 king and pawn moves); X moving its own pinner without capturing made a pin direct (C16 Re2, C09 Qxh4); the X pinner's own guard made a pin indirect (C01 Qa4 on c6) |
| 2026-10-21 | C fixture hand labels: C01 pin a4→a7,a8 `none`; C09 pin g5→g3,g2 `none` and `removal_of_defender` required; C08 `removal_of_defender` required; C16 pin e1→e5,e8 `none` | Captain, checkpoint 2. C08, C09: Lichess tags both `capturingDefender`, and the captured piece is a defender by SEE |
| 2026-10-21 | §4.5: new state motif `pin_tactical` (restricted movement through a threat that removing the Y piece enables; nine threat kinds, empty guarded squares included); `Motif.threat` (§5, §6) and motif identity includes `threat`; `MotifOptions` | Captain: comprehensive analysis first, conciseness later. Prototype on 9,300 puzzles: +9.6 reported motifs per puzzle (M2 tags 3.7), 17.8 ms per board |
| 2026-10-21 | §10: the four restricted-movement tags print `restricted movement (…)` instead of the tag; `overloaded_defender` detection unchanged | Captain: shared wording, not a merged tag. "(overloaded defender)" instead of "(overloaded: guards …)" so the targets are not printed twice |
| 2026-10-21 | §9: `pin_tactical` exempt from the C fixtures' exact set; corpus tests sample it (oracle every 16th puzzle row; puzzle eval off) | Captain: chosen C instances only (~71 X-side instances on C01–C16); no speed budget, corpus tests sample |
