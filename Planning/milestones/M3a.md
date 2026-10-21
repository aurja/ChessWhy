# M3a — Positional facts

Status: **draft** (2026-10-21). Frozen on branch `m3` once the Captain approves the
changed diagrams and the open choices in §1. Planned together with M3b
(`Planning/milestones/M3b.md`) and built in parallel with it; both share report v4.

Intent: the report states lasting features of the position (pawn structure, king safety,
piece activity, space, development, material) and, for every candidate line and the asked
move's line, which features appear or disappear at each ply and whether the change still
holds at the end of the walked plies. Every feature is a board rule (`rule`); no engine
call is added.

## 1. Captain's decisions

| # | Decision | Choice |
|---|---|---|
| P1 | Scope | Option B (2026-10-21): pawn structure, king safety, piece activity, space and development leads, material balance (§4). Named structure families and imbalances beyond material are deferred |
| P2 | Where changes are measured | Option C (2026-10-21): every ply of a line within its walked plies, each change flagged with whether it still holds at the end (§5) |
| P3 | Lines read | Candidate lines and the asked move's line (which contains its refutation); threat lines are not read |
| P4 | Lead thresholds | **Open** (§4.4): mobility 10, space 3, development 2, measured on broadcast games |
| P5 | Judgement-laden definitions | **Open** (§4.7): passed pawns counted on the front pawn of a file only; chains of at least 3 pawns; bad bishop rule; backward pawn rule; pawn shield squares |

## 2. Contract files

Created by the orchestrator in the contract commit, before batch 1:

| File | Content |
|---|---|
| `include/chesswhy/positional.hpp` | §7 API |
| `include/chesswhy/report.hpp` | §7 structs, `kSchemaVersion = 4` (shared with M3b) |
| `src/report/report.cpp` | `to_string(PositionalTag)`, `to_string(PositionalChangeKind)` only |
| `schema/report.v4.schema.json` | §6 plus M3b §5; replaces `report.v3.schema.json` |
| `src/positional/CMakeLists.txt`, `tests/positional/CMakeLists.txt` | new module `positional` in the top-level module list |
| `tests/fixtures/positional.json` | empty skeleton (§8) |

## 3. Scope

- New module `positional`: feature detection on one board (§4), changes along a line
  (§5). Pure functions of the board; source `rule`.
- Report v4: `position.positional` (features at ply 0) and `MoveAssessment.positional`
  (changes along the line) (§6).
- Template rows for the changes (§10). `position.positional` is not printed.
- Not in scope: choosing which changes the explanation mentions (M4, node `TiedToLine`);
  named structure families (Carlsbad, isolani structures and the like); imbalances beyond
  material; opposite-coloured bishops; changes on threat lines (B, E) and on
  `creates_threat`; any engine-based weighting of features.

## 4. Feature definitions

Notation: X is the feature's **owner** (`side`), Y the other side. Relative rank `r_X(s)`
counts from X's side (1 = X's back rank). "In front of" means a higher relative rank for
X. Adjacent files of file f are f−1 and f+1 on the board. A square's colour is light or
dark. Attacks are chess-library attack sets on the current board: sliders stop at the
first piece, no x-ray, pinned pieces still attack. Features are computed on every board,
including terminal ones.

Each feature: `tag`, `side`, `pieces` (in the order given), `file` (set only where the
table says), `value` (set only where the table says). Its **identity** decides whether it
is "the same feature" on two boards (§5).

### 4.1 Pawn structure (owner = the pawns' side)

| Tag | Holds when | pieces | file | value | Identity |
|---|---|---|---|---|---|
| `isolated_pawn` | a file holds X pawns and neither adjacent file holds an X pawn | those pawns, ascending square | the file | — | file |
| `doubled_pawns` | a file holds ≥ 2 X pawns | those pawns, ascending square | the file | — | file |
| `backward_pawn` | an X pawn p is not isolated, no X pawn on an adjacent file stands on p's relative rank or lower, and the square in front of p is attacked by a Y pawn | qualifying pawns of the file, ascending square | p's file | — | file |
| `passed_pawn` | an X pawn p has no Y pawn in front of it on its own or an adjacent file, and no X pawn in front of it on its own file | [p] | p's file | — | file |
| `connected_passed_pawns` | a maximal run of ≥ 2 consecutive files each holding an X `passed_pawn` | those passed pawns, by file | — | — | the run's files |
| `pawn_islands` | X has ≥ 1 pawn | — | — | number of maximal runs of consecutive files holding X pawns | value |
| `pawn_chain` | a maximal run of ≥ 3 X pawns on consecutive squares of one diagonal, each defending the next one forward | base to head | — | — | the pawns' squares |

### 4.2 King safety (owner = the king's side)

The **king files** are the X king's file and its adjacent files.

| Tag | Holds when | pieces | file | value | Identity |
|---|---|---|---|---|---|
| `castling_kingside` | X still has the kingside castling right | — | — | — | tag, side |
| `castling_queenside` | X still has the queenside castling right | — | — | — | tag, side |
| `king_shield_gap` | the X king stands on relative rank k ≤ 2, and king file f holds no X pawn on relative rank k+1 or k+2 | [X king] | f | — | file |
| `open_file_near_king` | king file f holds no pawn | [X king] | f | — | file |
| `half_open_file_near_king` | king file f holds no X pawn and ≥ 1 Y pawn | [X king] | f | — | file |
| `king_zone_attacker` | a Y knight, bishop, rook or queen attacks the X king's square or a square next to it | [attacker] | — | — | attacker's square and piece |

### 4.3 Pieces (owner = the pieces' side)

| Tag | Holds when | pieces | file | value | Identity |
|---|---|---|---|---|---|
| `bishop_pair` | X has a bishop on a light square and a bishop on a dark square | X bishops, ascending square | — | — | tag, side |
| `rook_open_file` | an X rook stands on a file with no pawn | X rooks on that file, ascending square | the file | — | file |
| `rook_half_open_file` | an X rook stands on a file with no X pawn and ≥ 1 Y pawn | X rooks on that file, ascending square | the file | — | file |
| `rook_seventh` | an X rook stands on relative rank 7, and the Y king stands on X's relative rank 8 or a Y pawn stands on X's relative rank 7 | X rooks on relative rank 7, ascending square | — | — | tag, side |
| `outpost` | an X knight or bishop stands on square s with `r_X(s)` 4–6, s is defended by an X pawn, and no Y pawn stands on a file adjacent to s in front of s | [the piece] | — | — | square and piece |
| `bad_bishop` | for a square colour c holding an X bishop: ≥ 2 X pawns on colour c have a Y pawn on the square in front of them, and X has more pawns on colour c than on the other colour | X bishops on c, ascending square, then those blocked pawns, ascending square | — | — | square colour of the bishop |

### 4.4 Space and development (owner = the side with the lead)

Counts for a side Z, with W the other side:

- **mobility(Z)**: over Z's knights, bishops, rooks and queens, the number of attacked
  squares that hold no Z piece and are not attacked by a W pawn (summed per piece).
- **space(Z)**: squares on files c–f with `r_Z` 2–4 that hold no Z pawn, are not attacked
  by a W pawn, and have a Z pawn in front of them on the same file.
- **undeveloped(Z)**: Z knights and bishops on Z's relative rank 1.

| Tag | Holds when | value | Identity |
|---|---|---|---|
| `mobility_lead` | mobility(X) − mobility(Y) ≥ 10 | the difference | tag, side |
| `space_lead` | space(X) − space(Y) ≥ 3 | the difference | tag, side |
| `development_lead` | undeveloped(Y) − undeveloped(X) ≥ 2 | the difference | tag, side |

`pieces` empty, `file` null. Thresholds (P4) from 3,000 games of the August 2022 Lichess
broadcasts with both players 2200+, share of positions where one side leads by at least
the threshold, at plies 10 / 20 / 30 / 40 / 60:

| Lead | Threshold | Share at plies 10 / 20 / 30 / 40 / 60 | Threshold − 1 |
|---|---|---|---|
| mobility | 10 | 9 / 15 / 17 / 18 / 16 % | ≥ 8: 20 / 25 / 28 / 30 / 25 % |
| space | 3 | 15 / 17 / 20 / 24 / 27 % | ≥ 2: 34 / 40 / 43 / 47 / 49 % |
| development | 2 | 3 / 9 / 4 / 3 / 1 % | ≥ 1: 49 / 57 / 47 / 34 / 26 % |

So a lead is reported in roughly one position in six (mobility, space) and only for a
clear gap in development (two minor pieces).

### 4.5 Material (owner = the side ahead)

Piece values are `tactics::piece_value` (pawn 1, knight 3, bishop 3, rook 5, queen 9).

| Tag | Holds when | value | Identity |
|---|---|---|---|
| `material_ahead` | material(X) − material(Y) ≥ 1 | the difference | value |
| `exchange_up` | X has more rooks than Y, and Y has more knights plus bishops than X | — | tag, side |

`pieces` empty, `file` null.

### 4.6 Order

`position.positional` and every change list are ordered by tag (the order of the tables
above), then side (white first), then file, then the square index of `pieces[0]`, then
value. Identity also always includes tag and side.

### 4.7 Choices for the Captain (P5)

- `passed_pawn` on the front pawn of a file only: doubled passed pawns are one passed
  pawn. Alternative: every pawn with a free path.
- `pawn_chain` needs 3 pawns: two pawns defending each other are not called a chain.
- `bad_bishop`: blocked pawns on the bishop's colour (≥ 2) and a pawn majority on that
  colour. Alternative: majority only.
- `backward_pawn`: the stop square must be attacked by a Y pawn; a pawn behind its
  neighbours on a closed file without that attack is not backward.
- `king_shield_gap`: only for a king on relative rank 1 or 2, two squares in front.

## 5. Changes along a line

A line `mv₁ … mvₙ` from root P₀ (side to move S), cut to its walked plies
(`analysis::walked_plies`, the same cut as `motifs`). Pₖ is the board after k plies.
F(Pₖ) is the feature list of Pₖ (both sides).

For k = 1 … n:

- **appears**: a feature of F(Pₖ) whose identity is not in F(Pₖ₋₁); reported as on Pₖ.
- **disappears**: a feature of F(Pₖ₋₁) whose identity is not in F(Pₖ); reported as on
  Pₖ₋₁.

`holds_at_end`: an appearance whose identity is in F(Pₙ), or a disappearance whose
identity is not in F(Pₙ). A feature whose identity persists while its pieces or value
change (a passed pawn advancing, a mobility lead growing) is not a change. Order: by ply,
then appears before disappears, then §4.6.

Lines read: every `candidates[i]` and the asked move. The asked move's `line_san` starts
with the move and continues with the engine's reply, so its refutation is covered without
a separate list. The asked move copying a candidate copies its changes.

## 6. Report v4 (positional part)

- `schema_version` 4; `schema/report.v4.schema.json` replaces v3 (shared with M3b §5).
- `position.positional` (required, array): F(P₀). A feature:

```json
{"source": "rule", "tag": "doubled_pawns", "side": "black",
 "pieces": [{"square": "c6", "piece": "p"}, {"square": "c7", "piece": "p"}],
 "file": "c", "value": null}
```

- `MoveAssessment.positional` (required, array): the changes of §5, each a feature with
  three more keys after `source`:

```json
{"source": "rule", "ply": 2, "change": "appears", "holds_at_end": true,
 "tag": "doubled_pawns", "side": "black",
 "pieces": [{"square": "c6", "piece": "p"}, {"square": "c7", "piece": "p"}],
 "file": "c", "value": null}
```

- `file` is a letter `a`–`h` or null; `value` an integer or null; `pieces` uses the
  existing `$defs/piece_on_square`. Threat objects get no positional key.
- Nothing else changes; every v3 field keeps its meaning.

## 7. Headers

```cpp
// report.hpp
enum class PositionalTag : std::uint8_t {
    IsolatedPawn, DoubledPawns, BackwardPawn, PassedPawn, ConnectedPassedPawns,
    PawnIslands, PawnChain, CastlingKingside, CastlingQueenside, KingShieldGap,
    OpenFileNearKing, HalfOpenFileNearKing, KingZoneAttacker, BishopPair, RookOpenFile,
    RookHalfOpenFile, RookSeventh, Outpost, BadBishop, MobilityLead, SpaceLead,
    DevelopmentLead, MaterialAhead, ExchangeUp };
[[nodiscard]] std::string to_string(PositionalTag tag);          // "isolated_pawn", ...
struct PositionalFeature {                                       // §4; source rule
    PositionalTag tag = PositionalTag::IsolatedPawn;
    Side side = Side::White;                                     // owner X
    std::vector<PieceOnSquare> pieces;
    std::optional<std::uint8_t> file;                            // 0 = a ... 7 = h
    std::optional<int> value;
};
enum class PositionalChangeKind : std::uint8_t { Appears, Disappears };
[[nodiscard]] std::string to_string(PositionalChangeKind kind);  // "appears", "disappears"
struct PositionalChange {                                        // §5
    int ply = 0;                                                 // 1..walked plies
    PositionalChangeKind change = PositionalChangeKind::Appears;
    bool holds_at_end = false;
    PositionalFeature feature;
};
// PositionInfo:   std::vector<PositionalFeature> positional;   // F(P0), both sides
// MoveAssessment: std::vector<PositionalChange> positional;    // §5 over the walked plies
// kSchemaVersion = 4

// positional.hpp (pure, provenance rule)
namespace chesswhy::positional {
inline constexpr int kMobilityLead = 10;
inline constexpr int kSpaceLead = 3;
inline constexpr int kDevelopmentLead = 2;
[[nodiscard]] int mobility(const chess::Board& board, chess::Color side);    // §4.4
[[nodiscard]] int space(const chess::Board& board, chess::Color side);       // §4.4
[[nodiscard]] int undeveloped(const chess::Board& board, chess::Color side); // §4.4
// Every feature of §4 for both sides, in §4.6 order.
[[nodiscard]] std::vector<PositionalFeature> features(const chess::Board& board);
// §4 identity; false for different tags or sides.
[[nodiscard]] bool same_identity(const PositionalFeature& a, const PositionalFeature& b);
// §5; `line` already cut to the walked plies and legal from `root`.
[[nodiscard]] std::vector<PositionalChange> line_changes(const chess::Board& root,
                                                         std::span<const chess::Move> line);
} // namespace chesswhy::positional
```

The contract commit's headers are authoritative; the block above is a summary.

## 8. Fixtures and reference

- **P fixtures** (`tests/fixtures/positional.json`, `unit`, no engine), run by
  `positional_unit_tests`:
  - `features`: `{id, title, source, fen, include, exclude, reason}`. `include` and
    `exclude` hold partial features in report JSON form (any subset of `tag`, `side`,
    `file`, `value`, `pieces`); an include matches iff some feature of `features(board)`
    has every given key equal; an exclude passes iff none does.
  - `lines`: `{id, title, source, fen, line_uci, include, exclude, reason}` with partial
    changes (`ply`, `change`, `holds_at_end` plus feature keys) against `line_changes`.
  - IDs `P01…` and `PL01…`. `source` names a game (players, event or year, move) or
    `constructed`; never a book.
  - Coverage: every tag has ≥ 2 includes and ≥ 1 near-miss exclude at its rule's boundary
    (threshold and threshold − 1 for the leads; ranks 3 and 4 for `outpost`; two pawns
    for `pawn_chain`; one blocked pawn for `bad_bishop`). Line fixtures cover appears and
    disappears, `holds_at_end` true and false, and persistence without a change (a passed
    pawn advancing, a rook moving along an open file, a lead changing value above the
    threshold).
  - At least 10 `features` fixtures are positions from the Captain's local books on pawn
    structure and positional play, transcribed by the fixture worker from text and
    checked against the diagram image; each lists the features the book's text names
    for that position.
  - Labels come from the plain §4 definitions, written before any detector runs.
- **F rows** (`tests/fixtures/positions.json`, `engine`): new key `positional_include`
  (pointer → partial items, same matching as `motifs_include`). At least three rows: the
  Exchange Ruy Lopez after `1.e4 e5 2.Nf3 Nc6 3.Bb5 a6 --move Bxc6` (black
  `doubled_pawns` on c appears at ply 2 and holds at the end; white `bishop_pair` absent
  and black `bishop_pair` present at the end); one `position.positional` row on an
  existing fixture; one row on a castling move (`castling_kingside` disappears at ply 1).
- **Reference** (`positional_oracle`, `unit`): an independent implementation of §4–§5 in
  `tests/positional/oracle/` written from this plan without reading the detector code.
  It compares `features` on every board of every solution line in
  `tests/fixtures/puzzles/test.csv` (root and each ply), `line_changes` on every solution
  line, and the counts of §4.4 on the same boards. Zero mismatches.

## 9. Measurements (not gating)

`positional_oracle` prints, per tag, how many boards of the puzzle sample carry it, so
the receipt can show tags that never fire or fire everywhere.

## 10. Template

- Under each candidate's and the asked move's line, one row per change of
  `MoveAssessment.positional`:
  `+ doubled_pawns: black, c file: p c6, p c7 (ply 2) [rule]` for an appearance,
  `- castling_kingside: white (ply 1) [rule]` for a disappearance, with
  `, not at end` inside the parentheses when `holds_at_end` is false, and the value after
  the side for valued tags (`+ mobility_lead: white 12 (ply 3) [rule]`). Pieces print as in
  motif rows; a feature without pieces prints none.
- Wording pinned by golden tests. `position.positional` is not printed.

## 11. Work packages

Each worker owns only the files listed, builds and runs its own targets, and returns
under 300 words. Batch 1: WP-A1, WP-A2, WP-A3, WP-R (shared with M3b). Batch 2: WP-I
(shared with M3b). Milestone end: WP-D (shared with M3b).

| WP | Owned files | Done when | Non-goals |
|---|---|---|---|
| WP-A1 detectors | `src/positional/*`, `tests/positional/positional_unit_tests.cpp` | §4–§5 implemented; unit tests at each rule's boundary (relative ranks for both colours, edge files, promoted pieces, en passant, castling-rights loss by a rook capture); P fixture harness; P fixtures pass once frozen (before that, own cases) | report, render, analysis |
| WP-A2 reference | `tests/positional/oracle/*`, `tests/positional/positional_oracle.cpp` | independent §4–§5 reference written from this plan without reading WP-A1's code; `positional_oracle` per §8; §9 counts printed; zero mismatches once WP-A1 lands | detector |
| WP-A3 fixtures | `tests/fixtures/positional.json` (proposal), new rows in `tests/fixtures/positions.json` (proposal), throwaway scripts under `build/` | P fixtures and F rows per §8, each label with a reason; then the Captain's review | code; running WP-A1's rules |
| WP-R report + render (M3a and M3b) | `src/report/*` (except the contract's `to_string`), `src/render/*`, `tests/report/*`, `tests/render/*` and goldens, `tools/validate_report.py` | `to_json` emits v4 (§6 and M3b §5) and validates for hand-built reports; template per §10 and M3b §8 with golden tests | analysis, CLI |
| WP-I analysis + CLI (M3a and M3b) | `src/analysis/*`, `src/cli/*`, `tests/analysis/*`, `tests/cli/*` | `position.positional` and `MoveAssessment.positional` filled per §5; M3b §7 wiring; F harness `positional_include` and M3b's `games` rows; all F fixtures, rerun identity and timing pass | rule logic, fixture edits |
| WP-D docs (M3a and M3b) | `README.md`, `third_party/DEPENDENCIES.md` | §13 and M3b §12 item 7 | — |

## 12. Captain's checkpoints

0. Before batch 1: the changed diagrams in `Planning/DBC/CONCEPTS.md`, P4 and P5.
1. After batch 1: the P fixture table (hand labels with reasons, book positions) and the
   F rows.
2. After batch 2: every P fixture where the rule and the hand label differ, with
   fixture, feature, both labels and the reason. The Captain decides per mismatch: change
   the rule (§15 row) or the label. The same invariant failing twice → design note
   (AGENTS.md).
3. Milestone end: try the smoke command and approve merging `m3`.

## 13. Credits and documentation

- `README.md`: positional facts in the feature paragraph; P fixtures and
  `positional_oracle` in the test section.
- Vision §5.6 and §11 row status; `Planning/CURRENT_STATE.md`; receipt
  `Planning/receipts/M3a.md`; DBC pass (CONCEPTS statuses, CODE rows, DEFINITIONS,
  FIXTURES, TESTS).

## 14. Acceptance

1. Release and ASan/UBSan builds warning-free; all `unit` and `engine` CTests pass in
   both trees.
2. P fixtures pass (`unit`) after checkpoint 2, including the ≥ 10 book positions.
3. `positional_oracle` zero mismatches; §9 counts recorded in the receipt.
4. All F fixtures keep every earlier expectation and pass their new rows; every F report
   validates against `report.v4.schema.json`.
5. Rerun identity: byte-identical JSON as in M1 acceptance 4.
6. `standard` ≤ 12 s wall time per F fixture (Release); the receipt records the slowest.
7. Facts first: every feature and change carries `rule`; the template prints only report
   fields (golden tests).
8. Gates: clang-format and clang-tidy on changed C++; Ruff and basedpyright on changed
   Python; ShellCheck on changed scripts; `python3 tools/check_dbc.py`.
9. Docs (§13) updated.

Smoke command: `build/release/chesswhy --fen
'r1bqkbnr/1ppp1ppp/p1n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4' --move Bxc6 --budget
standard --json --no-llm` (after `1.e4 e5 2.Nf3 Nc6 3.Bb5 a6`): `asked_move.positional`
lists black `doubled_pawns` on c at ply 2 with `holds_at_end` true, and white
`bishop_pair` disappearing at ply 1.

## 15. Contract changes

| Date | Change | Reason |
|---|---|---|
