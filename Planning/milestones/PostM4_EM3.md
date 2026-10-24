# PostM4_EM3 — Tempo, initiative and the impact rating

Status: **contract frozen** (2026-10-24): approved by the Captain at checkpoint 1 with the
changed concept diagrams and terms. Decisions A1–A5 and B1–B3 (§1) frozen the same day.
From here on, changes are contract changes recorded in §17.

Intent: the report says, from engine searches and frozen rules, whether time matters
in the position, which moves of a line gain a tempo, and who holds the initiative and
what becomes of it. Terms: `Planning/DBC/DEFINITIONS.md` §3.6.

## 1. Captain's decisions

Briefed 2026-10-24 (Deputy's tempo proposal, the probes and the reading of three of the
Captain's books); decided the same day.

| # | Topic | Decision |
|---|---|---|
| A1 | Threat test | (a) **Threatening move**: after the move, its side's threat (the opponent passes, as in search E) has rank ≥ 2 over the walked plies, read from the board after the move. A check is a threatening move by rule. Not the parry test's ES margin (it fails when ES is pinned at 0 or 1: V1 sits at 1.00 throughout) and not the centipawn cost of passing (≥ 100 cp even after a quiet 3.Bc4) |
| A2 | Trades | (a) A capture counts only for what it threatens next, not for the piece it took: an even trade is neither a threatening move nor a tempo. V2's `Bxa4` still gains a tempo, because it threatens `…Bxb3` |
| A3 | Ledger scope | T2: the tempo ledger covers every walked ply of every detailed line. The **timing check** becomes 15 s per F fixture at `standard` in Release (12 s before). Refined the same day: the second ledger search of a ply runs only after a threatening move ("why would we run it if there isn't a plausible reason to run it?") |
| A4 | Tempo value | (a) Ship the number with labels on the verdict scale: **tempo-dependent position** above 0.10, **non-tempo position** below 0.05, **zugzwang** below 0 |
| A5 | Initiative | (a) Derived from the tempo ledger with no extra search: one row per run with holder, plies, tempi gained, how it ended (spent, contested or runs) and converted or evaporated. "Runs" (the reply neither stops the threat nor is a pure reply, so the line turns into a tactic) was proposed by the Deputy after the budget comparison showed such replies inside runs and accepted by the Captain, both 2026-10-24 |
| B1 | Impact basis | (b) Consequence rank of the fact's line first; among positional facts the **static scale** (king safety, material, pawn structure, pieces, space/mobility/development); dynamic facts ahead of static facts in a tempo-dependent position, static facts first otherwise. Briefed after the reading of four more of the Captain's books and the move-report probe (§3.3, §3.4) |
| B2 | Use | (b) The **use test** cuts positional changes from the move report; the full report keeps every change. No cap per move report |
| B3 | Net change | (b) The relevance filter compares the root with the end of the walk, so **passing changes** leave the move report (today: the last change per feature that holds at the end) |

## 2. Rules the decisions fix

Notation as in M4: `S` the side to move at the root, `O` the opponent; `T` the side to
move on a given board; ES from the engine's WDL.

- **Searches per ply.** For ply k of a detailed line (board_k = the board after ply k,
  mover M): the **threat search**, a null search on board_k (M to move again), always
  runs; it gives the threat test for ply k and ES of passing on board_k. Ply 1 reuses
  search E. The **reply search**, a normal search on board_{k+1} (M to move after the
  opponent's reply), runs only when ply k is a threatening move; it tests whether the
  reply stopped the threat (M's best line below rank 2). Per line:
  walked plies − 1 threat searches plus one reply search per threatening move. Measured:
  9 of 15 on the Scandinavian, 13 of 15 on V2 with `Bxa4` first, 15 of 15 on V2 with
  `Ba7` first, 9 of 13 on V1.
- **Tempo value** on a board = ES_T(T moves) − ES_T(T passes). At the root it comes from A
  and B; after a candidate from A's line and E. Along a line it is not reported (the
  reply search runs only after threatening moves). Null where T is in check (no pass is
  possible).
- **Skips.** No ledger when `null_move_reliable` is false (a negative tempo value is
  still reported). A check is threatening by rule, with no threat search. The ledger stops
  at a board where the game is over.
- **Budget.** A new search kind for the ledger searches. Measured on 2026-10-24: about
  0.37 M nodes per second (0.11 s per 40k search). The label comparison (§3.2) found no
  gain from 75k or 400k over 40k. Decided (Captain, 2026-10-24): `standard` 40k, `quick` 10k,
  `deep` 160k (×¼ and ×4, as D and E). At 40k the worst case is about 1.6 s per line
  (15 searches), 8 s for five detailed lines where every move threatens; most lines need
  fewer. The slowest `standard` F fixture took 7.91 s at PostM3_EM2 (F12). If an F fixture
  exceeds 15 s in batch 1, `standard` drops to 30k; the receipt records the measurements.
- **Whole-board selections.** `--moves playable` and `--moves all` detail more lines.
  Estimate at 40k (at most 1.6 s per line): `--moves all` on the smoke position 37.9 s +
  31 × 1.6 s ≈ 88 s (target ≤ 90 s); `--moves playable` 19.4 s + about 8 s ≈ 27 s (target
  ≤ 30 s). The M4 targets stay; batch 1 measures both.

## 3. Evidence (probes of 2026-10-24)

### 3.1 Tempo value (WDL at 400k nodes)

| Position | Tempo value |
|---|---|
| Italian, quiet, White to move | +0.013 |
| London after 8.Bd3 | +0.029 |
| Start position | +0.048 |
| V2 root | +0.314 |
| Scandinavian after 3.Nc3 (queen attacked) | +0.394 |
| V1, V3 roots | +1.000 |
| `8/3k4/8/3K4/3P4/8/8/8 w - - 0 1` (zugzwang: drawn with White to move, won with Black to move) | −0.500 |

Lines: the Scandinavian (`Qxd5 Nc3 Qa5 d4 Nf6 Nf3 Bf5 Bc4`) gains White one tempo at
plies 2–3 (`Nc3` threatening, `Qa5` a pure reply). V2 with `Bxa4` first gains Black a
tempo at plies 1–2. At ply 4 `Rfc2` answers `Ba7` with a counter-threat, and the line
settles at ply 8 as a non-tempo position (tempo value 0.09) with Black the exchange up,
so Black's initiative is converted. V2 with `Ba7` first threatens on every ply at a
tempo value of 0.50 ± 0.01 and has no run. V1's `Qc4` line gains White a tempo with
`bxc5 dxc5`.

### 3.2 Ledger labels by budget

Each ply labelled at 40k, 75k and 400k nodes, with the reply search only after a
threatening move; 1.6M nodes as the reference on the two V2 lines (the other two agree at
every budget). Labels: threatening, pure reply, counter-threat, threatening reply that
does not stop the threat ("threat, runs"), reply that does not stop it ("runs").

| Line (plies) | 40k | 75k | 400k | Tempo gained (reference) |
|---|---|---|---|---|
| Scandinavian (8) | all agree | all agree | all agree | White `Nc3`; all budgets |
| V1 `Qc4` (7) | all agree | all agree | all agree | White `bxc5`; all budgets |
| V2 `Bxa4` first (8) | 1 off (ply 4) | 1 off (ply 2) | 1 off (ply 4) | Black `Bxa4`, White `Bxa7`; **75k misses `Bxa4`** (`bxa4` read as "runs") |
| V2 `Ba7` first (8) | 2 off (plies 3, 4) | 0 off | 2 off (plies 2, 4) | none; all budgets |
| Time per line | 1.0–1.6 s | 1.8–2.9 s | 8.5–13.9 s | 1.6M: 48–58 s |

Reading: the disagreements are all counter-threat against "threat, runs" inside the
exchange, where a line's net gain sits at the 1-pawn border, and they do not shrink with
more nodes (400k is off as often as 40k). The tempo gained facts agree at 40k and 400k;
75k loses one. Fixtures pin tempo gained and initiative runs, not per-ply labels inside
exchanges.

### 3.3 Positional facts in today's move reports

Counted on the 88 move reports of the F fixtures (`build/release/tests/cli/reports/`):
913 positional changes, 10.4 per move report, at most 32. The most frequent tags are
`king_zone_attacker` 163, `pawn_islands` 134, `king_shield_gap` 126, `material_ahead` 91.

- **Passing changes**: 196 of the 913 (21%, matched by tag, side, file and value, so
  approximate). `material_ahead` alone gives 37: the momentary material inside an exchange.
- **Use test** (throwaway probe, rules as in DEFINITIONS §3.6): 338 changes pass, 521 fail,
  54 are leads that depend on the tempo value. Per move report 3.8 on average, at most 17
  (plus about 0.6 leads). Without the plies 1–2 rule it is 2.7, but that cuts F25 `Bxc6`'s
  doubled c-pawns and the lost bishop pair, which are the point of the move.

### 3.4 Reading (four of the Captain's books on positional and strategic play)

- One book gives a fixed order of static factors (king position, material including the
  bishop pair, the ending after a queen trade, pawn structure), splits factors into
  lasting (static) and fading (dynamic), and names the decisions that matter: an exchange,
  a change of the pawn structure, the end of a forced series.
- The other three give no fixed order. They agree on a relevance rule: a weakness counts
  only if it can be attacked or occupied; a file, a hole, space or a passed pawn counts
  only if it is used (penetration, targets, advance); a blockaded passed pawn is worth
  little; an unsafe king counts once it can be attacked.
- All four put the opponent's threat and tactics first and treat a dynamic advantage as
  "use it or lose it": with a dynamic edge, act; with a static edge, prevent counterplay.

## 4. Contract files

| File | Change |
|---|---|
| `include/chesswhy/report.hpp` | §8: tempo types, `NodeBudget::l`, new fields on `PositionInfo`, `MoveAssessment`, `PositionalChange`; `kSchemaVersion` 7 |
| `include/chesswhy/analysis.hpp` | §8: tempo label constants and pure ledger, run and outcome functions |
| `include/chesswhy/positional.hpp` | §8: `mark_use`, `favours`, `is_dynamic`, `static_scale_group`, `impact_order` |
| `include/chesswhy/move_report.hpp` | §8: `ImpactGroup`, `TempoGained`, new `MoveReport` and `PositionSummary` fields, `impact_order` |
| `schema/report.v7.schema.json`, `schema/moves.v7.schema.json` | §7; replace the v6 files (deleted) |
| `tests/fixtures/positions.json` | F40–F44 (§11.1), changed expectations of existing F fixtures |
| `tests/fixtures/positional.json` | new `uses` section PU01–PU18 (§11.2) |

## 5. Scope

In: tempo value at the root and after each move (§6.1); the tempo ledger on every walked
ply of every detailed line (§6.2); initiative runs with ending and outcome (§6.3);
favoured side (§6.4); net change (§6.5) and the use test (§6.6) on every positional
change; the impact order of the move report (§6.7); report v7 (§7); text (§12); the
semantic checker for all of it.

Out: §6.8.

## 6. Rules

Notation: `S` the side to move at the root, `O` the opponent; boards[0] the root,
board_k the board after ply k of a line; ES in units of 1/2000 (`es_units`), always from
the point of view the field names. "Rank" is `analysis::consequence_rank`.

### 6.1 Tempo value

`TempoValue{side T, moves_units, passes_units, value_units}`, `value_units =
moves_units − passes_units`, source `engine`.

- **Root** (`position.tempo_value`): T = S; `moves_units` = A line 1's `es_units`;
  `passes_units` = 2000 − B line 1's `es_units`. Null when S is in check, the position is
  terminal, or B returned no line.
- **After move m** (`MoveAssessment.tempo_value`): T = O; `moves_units` = 2000 −
  `m.eval.es_units`; `passes_units` = 2000 − the `es_units` of E's best line (S's point
  of view). So `value_units` = E − `m.eval`. Null when m is not detailed or E did not
  run (`creates_threat_skipped` set).
- **Label** (`analysis::tempo_label`): `value_units > 200` → `tempo_dependent`;
  `value_units < 0` → `zugzwang`; `0 ≤ value_units < 100` → `non_tempo`; 100–200 → null.
- **Tempo-dependent position** (the flag every later rule reads): the root's label is
  `tempo_dependent`. A null root tempo value counts as false.

### 6.2 Tempo ledger

`MoveAssessment.tempo_ledger`, with `tempo_ledger_skipped` set iff it is null. Skip
reasons, checked in this order: `in_check` (S in check; `null_move_reliable` is then
null), `null_move_unreliable` (`null_move_reliable` false), `not_detailed`.

Plies 1..n, where n = the move's walked plies, cut after the first board that is game
over (that ply included). M_k is the side that played ply k.

**Threat test of ply k** (fields `threatening`, `by_check`, `threat_rank`):

1. board_k in check → threatening, `by_check` true, `threat_rank` null.
2. k = 1: from search E. `creates_threat` set → `threat_rank` = its rank, threatening iff
   ≥ 2. Skip `gives_check` is case 1; skips `game_over` and `no_legal_move` → not
   threatening, `threat_rank` null.
3. k ≥ 2: board_k game over, or M_k has no legal move after the null move → not
   threatening, `threat_rank` null. Otherwise the **threat search**: null move on board_k,
   MultiPV 1, `L` nodes; its line is M_k's threat, `threat_rank` = the rank of
   `analysis::consequence` of that line from board_k with M_k to move; threatening iff ≥ 2.

**Reply test** (`stops_threat`), set iff ply k−1 is threatening (so never on ply 1); a
ply k that does not exist (k − 1 = n) gets none. board_k game over → true. Otherwise the
**reply search**: normal search on board_k (M_{k−1} to move), MultiPV 1, `L` nodes;
stops iff the rank of its consequence is < 2.

**Label** (`analysis::label_ledger`, L3 `PlyLabel`): if ply k−1 is threatening:
`stops_threat` ? (threatening ? `counter_threat` : `pure_reply`) : (threatening ?
`threatening` : `neither`); otherwise threatening ? `threatening` : `neither`.
`tempo_gained` on ply k iff ply k+1 is labelled `pure_reply`. `tempi` counts the plies
with `tempo_gained` per side.

### 6.3 Initiative runs

`InitiativeRun{holder H, start_ply, end_ply, tempi, ending, outcome}`, source `rule`,
computed by `analysis::initiative_runs` from the ledger plies alone plus the line.

1. Scan plies from 1. A run starts at ply s with `tempo_gained` that lies in no earlier
   run; H = M_s. Scanning resumes at the run's `end_ply`, so a new run may start there.
2. It continues while H's plies s+2, s+4, … gain a tempo. Let g be the last of them and
   e = g + 2. `tempi` = number of gaining H plies.
3. Ending, first match wins: e > n → `unfinished`, end n; ply e not threatening →
   `spent`, end e; board_e checkmate → `runs`, end e; e + 1 > n → `unfinished`, end e;
   ply e+1 is `counter_threat` → `contested`, end e+1; ply e+1's `stops_threat` false →
   `runs`, end e+1.
4. Outcome: `converted` iff (a) H's material balance (H minus the other side, pawn 1,
   knight 3, bishop 3, rook 5, queen 9) on board_n minus that on board_{s−1} is ≥ 1, or
   (b) a change of the move's `positional` with `net_change` and `in_use` true has its ply
   in [s, end_ply] and `favours` H; otherwise `evaporated`.

### 6.4 Favoured side

`positional::favours(change)`, `std::optional<Side>`: `pawn_islands`, `pawn_chain` →
null. Weakness tags (`isolated_pawn`, `doubled_pawns`, `tripled_pawns`, `backward_pawn`,
`king_shield_gap`, `open_file_near_king`, `half_open_file_near_king`,
`king_zone_attacker`, `bad_bishop`): appears → the owner's opponent, disappears → the
owner. Every other tag: appears → the owner, disappears → the owner's opponent.

### 6.5 Net change

For one move's `positional` list: per feature identity (`positional::same_identity`), only
its last change can be net. It is a **net change** iff (`change == appears`) ≠ (a feature
of the same identity is among the root features). Every other change has `net_change`
false and `in_use` null.

### 6.6 Use test

On each net change; X = its owner, Y = the other side, k = its ply, W = the move's walked
plies, line = the walked moves; feature squares = the squares of `feature.pieces`.
King-file tags: `king_shield_gap`, `open_file_near_king`, `half_open_file_near_king`.
Shelter tags: the king-file tags and `castling_kingside`, `castling_queenside`. Rook
files: `rook_open_file`, `rook_half_open_file`. Weak pawns: `isolated_pawn`,
`doubled_pawns`, `tripled_pawns`, `backward_pawn`. Passers: `passed_pawn`,
`connected_passed_pawns`. First match wins (L3 `UseTest`):

1. board_W is checkmate → fail.
2. Superseded → fail: a castling tag disappears and ply k is a castling move;
   `material_ahead` disappears and the material count on board_W is not level;
   `king_zone_attacker` disappears; a king-file tag disappears and X's king stands on
   another square on board_W than on boards[0].
3. `material_ahead`, `exchange_up`, `rook_seventh`, `outpost`, `bad_bishop`,
   `bishop_pair` → pass.
4. `pawn_islands`, `pawn_chain` → fail.
5. `mobility_lead`, `space_lead`, `development_lead` → pass iff tempo-dependent position.
6. A king-file tag (either direction) while X keeps a castling right on board_W → pass
   iff the king is pressed from k.
7. k ≤ 2 → pass for weak pawns, passers, shelter tags and rook files (not
   `king_zone_attacker`).
8. Weak pawns. Appears → pass iff a Y move at plies k+1..W captures on a feature square,
   or on board_W an X pawn on a feature square is attacked by Y. Disappears → pass iff
   ply k is a Y capture landing on a feature square.
9. Passers. Appears → pass iff a move at plies k+1..W starts from a feature square.
   Disappears → pass iff ply k starts from a feature square, or ply k is a Y move to one.
10. Shelter tags and `king_zone_attacker` appearing → pass iff the king is pressed from k.
11. Rook files. Appears → pass iff a move at plies k+1..W starts on a feature square and
    stays on its file. Disappears → fail.

**King pressed from k:** a Y move at plies k..W gives check, or captures on a square of
X's king zone (X's king square and its neighbours, on the board before that move); or on
board_W at least two distinct Y pieces other than pawns and the king attack the zone.

Measured with the Deputy's oracle of these rules on the M4 full reports of the F
fixtures (2026-10-24): 932 changes in move reports before; 726 net changes; 170 kept
with leads cut (1.9 per move report, at most 9) and 197 with leads kept (2.2, at most 11).

### 6.7 Impact order

**Static scale group** (`positional::static_scale_group`), in order: king safety
(`castling_kingside`, `castling_queenside`, king-file tags, `king_zone_attacker`) >
material (`material_ahead`, `exchange_up`, `bishop_pair`) > pawn structure (the eight
pawn tags) > pieces (rook files, `rook_seventh`, `outpost`, `bad_bishop`) > space,
mobility and development (the three leads). **Dynamic** (`positional::is_dynamic`):
`castling_kingside`, `castling_queenside`, `king_zone_attacker`, `mobility_lead`,
`development_lead`; every other tag is static.

**Positional order** (`positional::impact_order`, stable sort): dynamic before static in
a tempo-dependent position, static before dynamic otherwise; then by group; then the
full report's order.

**Group order** (`impact_order` in `move_report.hpp`): the non-empty groups among
`parried_threat`, `created_threat`, `motifs` (motifs or root motifs), `tempo`
(`tempo_value` set, or `tempo_gained` or `initiative` non-empty), `positional_dynamic`,
`positional_static`. Sorted by rank, descending: parried = rank of `O`'s threat;
created = rank of the created threat; motifs = rank of the move's consequence; the
rest 0. Ties keep the base order: tempo-dependent position `[parried_threat,
created_threat, motifs, tempo, positional_dynamic, positional_static]`, otherwise
`[parried_threat, created_threat, motifs, positional_static, positional_dynamic, tempo]`.

Core claims are not rated and come first: heading and verdict, line and conditions,
tactic and opportunity, refutation. Games stay last.

### 6.8 Not in scope

Tempo value along lines; development tempo of opening theory; break cost; a cap on facts
per move report; changes to features, motifs, verdicts or the relevance filter's
non-positional rows; model prose.

## 7. Report v7

`kSchemaVersion` 7; `schema/report.v7.schema.json` and `schema/moves.v7.schema.json`
replace v6. Numbers as on the verdict scale (`units / 2000`).

- **Full report.** `position.tempo_value`; on each candidate `tempo_value`,
  `tempo_ledger`, `tempo_ledger_skipped`; on each positional change `net_change` (bool)
  and `in_use` (bool or null, set iff `net_change`); `engine.nodes["L"]`.
  - `tempo_value`: `{side, moves, passes, value, label, source: "engine"}`.
  - `tempo_ledger`: `{plies: [{ply, san, side, threatening, by_check, threat_rank,
    stops_threat, label, tempo_gained}], tempi: {white, black}, runs: [{holder,
    start_ply, end_ply, tempi, ending, outcome, source: "rule"}], source: "engine"}`.
- **Move report document.** `position.tempo_value`; per move `tempo_value`,
  `tempo_gained` (`[{ply, san, side, source: "engine"}]`, null iff no ledger),
  `initiative` (the ledger's runs, null iff no ledger), `impact_order` (list of group
  names). `positional` keeps exactly the changes with `net_change` and `in_use` true,
  in positional order (§6.7).
- Schema if/then: `tempo_ledger_skipped` set iff `tempo_ledger` null; `in_use` set iff
  `net_change` true; `stops_threat` null on ply 1; `threat_rank` null when `by_check`.
- Enum spellings: labels `tempo_dependent`, `non_tempo`, `zugzwang`; ply labels
  `threatening`, `pure_reply`, `counter_threat`, `neither`; endings `spent`,
  `contested`, `runs`, `unfinished`; outcomes `converted`, `evaporated`; skips
  `in_check`, `null_move_unreliable`, `not_detailed`; groups as in §6.7.

## 8. Headers

```cpp
// report.hpp (additions; kSchemaVersion = 7)
enum class TempoLabel : std::uint8_t { TempoDependent, NonTempo, Zugzwang };
struct TempoValue {                    // source engine (§6.1)
    Side side = Side::White;           // T, the side to move on the board it describes
    int moves_units = 0;               // ES_T when T moves
    int passes_units = 0;              // ES_T when T passes
    int value_units = 0;               // moves_units - passes_units
    std::optional<TempoLabel> label;   // analysis::tempo_label(value_units)
};
enum class TempoPlyLabel : std::uint8_t { Threatening, PureReply, CounterThreat, Neither };
struct TempoPly {                      // §6.2
    int ply = 1;
    std::string san;
    Side side = Side::White;           // M_k
    bool threatening = false;
    bool by_check = false;
    std::optional<int> threat_rank;    // null: by check, game over, no legal move
    std::optional<bool> stops_threat;  // set iff ply - 1 is threatening
    TempoPlyLabel label = TempoPlyLabel::Neither;
    bool tempo_gained = false;         // ply + 1 is PureReply
};
enum class InitiativeEnding : std::uint8_t { Spent, Contested, Runs, Unfinished };
enum class InitiativeOutcome : std::uint8_t { Converted, Evaporated };
struct InitiativeRun {                 // source rule (§6.3)
    Side holder = Side::White;
    int start_ply = 1;
    int end_ply = 1;
    int tempi = 1;
    InitiativeEnding ending = InitiativeEnding::Spent;
    InitiativeOutcome outcome = InitiativeOutcome::Evaporated;
};
struct Tempi { int white = 0; int black = 0; };
struct TempoLedger {                   // source engine; runs source rule
    std::vector<TempoPly> plies;       // 1..n
    Tempi tempi;
    std::vector<InitiativeRun> runs;
};
enum class TempoLedgerSkip : std::uint8_t { InCheck, NullMoveUnreliable, NotDetailed };
// NodeBudget:       std::uint64_t l = 0;  // tempo ledger: threat and reply searches
// PositionalChange: bool net_change = false;
//                   std::optional<bool> in_use;              // set iff net_change
// PositionInfo:     std::optional<TempoValue> tempo_value;   // root, §6.1
// MoveAssessment:   std::optional<TempoValue> tempo_value;   // after the move, T = O
//                   std::optional<TempoLedger> tempo_ledger;
//                   std::optional<TempoLedgerSkip> tempo_ledger_skipped; // null iff ledger
// to_string for every new enum: the §7 spellings.

// analysis.hpp (additions)
inline constexpr int kTempoDependentUnits = 200; // label tempo_dependent above this
inline constexpr int kNonTempoUnits = 100;       // label non_tempo from 0 below this
[[nodiscard]] std::optional<TempoLabel> tempo_label(int value_units);
[[nodiscard]] bool tempo_dependent(const std::optional<TempoValue>& root);
// Sets label and tempo_gained from threatening and stops_threat (§6.2).
void label_ledger(std::vector<TempoPly>& plies);
[[nodiscard]] int material_balance(const chess::Board& board, chess::Color side); // §6.3
// §6.3; `line` is the ledger's n moves from `root`, `changes` the move's positional list
// after positional::mark_use.
[[nodiscard]] std::vector<InitiativeRun> initiative_runs(
    std::span<const TempoPly> plies, const chess::Board& root,
    std::span<const chess::Move> line, std::span<const PositionalChange> changes);

// positional.hpp (additions)
enum class StaticScaleGroup : std::uint8_t {
    KingSafety, Material, PawnStructure, Pieces, SpaceMobilityDevelopment };
[[nodiscard]] StaticScaleGroup static_scale_group(PositionalTag tag);
[[nodiscard]] bool is_dynamic(PositionalTag tag);
[[nodiscard]] std::optional<Side> favours(const PositionalChange& change); // §6.4
// §6.5 and §6.6 on the output of line_changes(root, line); computes the root features.
void mark_use(std::vector<PositionalChange>& changes, const chess::Board& root,
              std::span<const chess::Move> line, bool tempo_dependent);
// §6.7 positional order; stable.
void impact_order(std::vector<PositionalChange>& changes, bool tempo_dependent);

// move_report.hpp (additions)
enum class ImpactGroup : std::uint8_t {
    ParriedThreat, CreatedThreat, Motifs, Tempo, PositionalDynamic, PositionalStatic };
struct TempoGained { int ply = 1; std::string san; Side side = Side::White; };
// MoveReport:      std::optional<TempoValue> tempo_value;
//                  std::optional<std::vector<TempoGained>> tempo_gained; // null iff no ledger
//                  std::optional<std::vector<InitiativeRun>> initiative; // null iff no ledger
//                  std::vector<ImpactGroup> impact_order;
//                  positional: net_change && in_use, positional::impact_order
// PositionSummary: std::optional<TempoValue> tempo_value;
[[nodiscard]] std::vector<ImpactGroup> impact_order(const MoveReport& move, bool tempo_dependent);
```

`rules.cpp` `node_budget`: `l` = 30 000 standard (40 000 before the batch 1 timing
fallback, §17), 10 000 quick, 160 000 deep.
`analyse` fills the root tempo value after B, runs `mark_use` on every candidate's
changes (walked moves, root tempo-dependent flag), then builds the ledger.

## 9. Puzzle check

None: no motif or tactic rule changes.

## 10. Calibration and targets

- Label boundaries 0.10, 0.05, 0 and the rank-2 threat test are frozen (§1 A1, A4).
- Timing check 15 s per F fixture at `standard` in Release. If an F fixture exceeds it
  in batch 1, `l` at `standard` drops to 30 000 (contract change) and the receipt records
  the measurements.
- F32 smoke targets: `--moves playable` ≤ 30 s, `--moves all` ≤ 90 s.

## 11. Fixtures

### 11.1 Engine fixtures (`positions.json`)

| ID | Position and arguments | Expected |
|---|---|---|
| F40 | `rnbqkbnr/ppp1pppp/8/3P4/8/8/PPPP1PPP/RNBQKBNR b KQkq - 0 2` `--moves Qxd5` | Qxd5's ledger: ply 2 Nc3 `tempo_gained`, ply 3 `pure_reply` |
| F41 | V2 `1br3k1/5pp1/p3p2p/1bBn4/N7/1P3PP1/5R1P/2R3K1 b - - 0 1` `--moves Bxa4,Ba7` | root label `tempo_dependent` (≈ 0.314); Bxa4: Black gains a tempo with Bxa4, White with Bxa7, a run that is `converted`; Ba7: no tempo gained |
| F42 | V1 `5k2/7p/pr1pB1p1/2pP4/1P6/6PP/R3qP2/R5K1 b - - 1 33` `--moves Qc4` | root value 1.0, `tempo_dependent`; Qc4's ledger: White gains a tempo with bxc5 |
| F43 | `8/3k4/8/3K4/3P4/8/8/8 w - - 0 1` | root label `zugzwang` (≈ −0.5); every ledger skipped `null_move_unreliable` |
| F44 | the first of F32's root, the start position and F28's root with root label `non_tempo` | root label `non_tempo`; `impact_order` ends with `tempo` where present |

F25 also pins its move report positional list (`moves_equals`): Bxc6 keeps
`doubled_pawns` Black and `bishop_pair` White disappearing, drops the passing
`material_ahead`. Existing F expectations that the use test changes are re-blessed by
WP6 and listed for checkpoint 2. Any F40–F44 value that differs from this table goes to
checkpoint 2, not into the fixture silently.

### 11.2 Use-test fixtures (`positional.json`, section `uses`)

Entries `{id, source, fen, line_uci, tempo_dependent, kept[], cut[], passing[]}`; each
change `{tag, side, file|square, change, ply}`. kept = `net_change && in_use`; cut =
`net_change && !in_use`; passing = not net. Listed changes must match exactly; changes
not listed are not checked. PU01–PU18 with their kept, cut and passing changes:
`Planning/DBC/FIXTURES.md` §11. WP2 converts the SAN lines to UCI and copies the root
FENs; PU18 is checked against the C++ features first and its board adjusted if needed.

## 12. Templates

Pinned by golden tests (WP4). Provenance tag on every row, as today.

| Item | Text |
|---|---|
| Root (both texts, after the position row) | `Tempo value: 31.4% for Black (tempo-dependent position) [engine]`; `(non-tempo position)`, `(zugzwang)`; no parenthesis for a null label; row omitted when null |
| After a move | `After the move: tempo value 39.4% for White (tempo-dependent position) [engine]` |
| Tempo gained (move report) | `Tempo: White gains a tempo with Nc3 (ply 2) [engine]`, one row per gained ply |
| Initiative (both texts) | `Initiative: White from ply 2, 2 tempi; spent at ply 6; converted [rule]` (`1 tempo`; ending word and `end_ply`) |
| Ledger (full report) | `Tempo ledger: 1. Qxd5 neither; 2. Nc3 threatening, tempo gained; 3. Qa5 pure reply; …; White 1 tempo, Black 0 [engine]`; skipped: `Tempo ledger: skipped (null move unreliable) [engine]` (`S in check`, `line not detailed`) |
| Positional (full report) | every change as today, with ` (passing change)` when `net_change` is false and ` (fails the use test)` when `in_use` is false |

Move report order: heading; line and conditions; tactic, opportunity and tactic labels;
refutation with its rows; then the groups in `impact_order` (parried threat, created
threat, motifs, tempo rows, dynamic then static positional rows or the reverse); games.

## 13. Work packages

Batch 1: WP1 and WP2 together (WP2 commits `positional.hpp` first, exactly as §8, and
messages WP1). Batch 2 after batch 1 lands: WP3, WP4, WP5. Batch 3: WP6, then
checkpoint 2. Milestone end: WP7.

| WP | Owned files | Done when | Non-goals |
|---|---|---|---|
| WP1 analysis | `include/chesswhy/report.hpp`, `include/chesswhy/analysis.hpp`, `src/analysis/*`, `src/report/report.cpp`, `schema/report.v7.schema.json`, `tests/analysis/*`, `tests/report/report_unit_tests.cpp`, `tests/report/CMakeLists.txt`; v7 bumps in `tests/cli/CMakeLists.txt`, `tests/checker/unit_tests.py` | §6.1–6.3, §7 full report, budget `L`, `mark_use` called; unit tests on label boundaries (99/100, 200/201, −1/0), ledger labels and every run ending, both outcome arms | move reports, text, checker |
| WP2 positional | `include/chesswhy/positional.hpp`, `src/positional/*`, `tests/positional/*`, `tests/fixtures/positional.json` | §6.4–6.7 positional parts; PU01–PU18 pass; each use-test rule has a fixture or unit case | engine, report JSON |
| WP3 move reports | `include/chesswhy/move_report.hpp`, `src/report/move_report.cpp`, `schema/moves.v7.schema.json`, `tests/report/move_report_tests.cpp` | §6.7 groups, §7 move report on hand-built reports: positional filter and order in both modes, tempo fields, both base orders and rank sorting | analysis, text |
| WP4 render | `src/render/*`, `tests/render/*` and goldens | §12 on hand-built documents of both kinds | analysis |
| WP5 checker | `tools/validate_report.py`, `tests/checker/*` | independent python-chess re-implementation of §6.1–6.7 (tempo arithmetic and labels, ledger labels and tempi, runs and outcome, net change, use test) and the v7 projection; one mutant per new relation, each fails | C++ |
| WP6 fixtures | `tests/fixtures/positions.json`, `tests/cli/engine_fixture_test.cpp` (timing 15 s) | F40–F44 and §11.1 re-blessing; every F passes; changed values listed for checkpoint 2 | code |
| WP7 docs | `README.md` | §15 | — |

## 14. Captain's checkpoints

1. Before batch 1: this contract, the changed concept diagrams and the new terms
   (unfinished, favoured side, the converted rule, use-test rules 1, 2 and 6).
2. After batch 3: every F value that changed (fixture, field, old, new, reason), F44's
   choice, the boards of F40–F44, and a text sample of V2 `--moves Bxa4,Ba7`.
3. Milestone end: try the smoke commands.

## 15. Credits and documentation

- DEFINITIONS (contract commit): §3.6 contracted rows, L, P fixtures, use-test fixture.
- `README.md`: tempo value, tempo ledger, initiative, the use test and the impact order;
  schema v7.
- `Planning/CURRENT_STATE.md`, receipt `Planning/receipts/PostM4_EM3.md`, DBC pass.

## 16. Acceptance

1. Release and ASan/UBSan builds warning-free; all `unit`, `engine` and `games` CTests
   pass in both trees.
2. §6.1–6.3: unit tests of WP1; F40–F43 as in §11.1 (or as approved at checkpoint 2);
   F44 chosen and passing.
3. §6.4–6.6: PU01–PU18 pass; F25's move report positional list as in §11.1.
4. §6.7: move report unit tests of both base orders; every F move report document's
   `impact_order` passes the projection check.
5. §7: every F full report validates against `report.v7.schema.json` and passes the
   semantic checks; every F move report document validates against
   `moves.v7.schema.json` and passes the projection check; every mutant fails.
6. Timing ≤ 15 s per F fixture; F32 `--moves playable` ≤ 30 s and `--moves all` ≤ 90 s;
   rerun identity byte-identical for both documents.
7. Facts first: every tempo row prints a report field (golden tests); no rule adds a
   chess claim outside the report.
8. Gates: clang-format and clang-tidy on changed C++; Ruff and basedpyright on changed
   Python; `python3 tools/check_dbc.py`.
9. Docs (§15) updated.

Smoke command: `build/release/chesswhy --fen
'1br3k1/5pp1/p3p2p/1bBn4/N7/1P3PP1/5R1P/2R3K1 b - - 0 1' --moves Bxa4,Ba7` (text: root
tempo value with "tempo-dependent position"; Bxa4 with its tempo and initiative rows;
Ba7 without a tempo gained).

## 17. Contract changes

| Date | Change | Reason |
|---|---|---|
| 2026-10-24 | Contract drafted (§4–§16) | Decisions A1–A5, B1–B3 |
| 2026-10-24 | Checkpoint 1: contract, diagrams and terms approved as drafted, including `unfinished`, the converted rule and use-test rules 1, 2 and 6 | Captain |
| 2026-10-24 | WP1's v7 cutover is version-only outside its files: it renames `moves.v6.schema.json` to `moves.v7.schema.json` (version 7, no new fields) and updates schema names and version numbers in `tools/validate_report.py`, `tests/report/CMakeLists.txt`, `tests/report/move_report_tests.cpp`, `include/chesswhy/move_report.hpp` (comment), `tests/cli/CMakeLists.txt` (schema path only) and `tests/checker/unit_tests.py`; WP3 and WP5 extend them in batch 2. `tests/positional/oracle/*` is not WP2's: the test-progress task (Captain's request, not milestone scope) owns it with the other long-running test drivers during batch 1 | keep the tree building between batches |
| 2026-10-24 | §6.3 rule 3 clarified (no change of meaning): the endings are exhaustive on plies labelled by `label_ledger`, because a pure reply on ply e+1 gives ply e a tempo and so continues the run; `initiative_runs` states that precondition and treats a violation as a programming error | question from WP1 |
| 2026-10-24 | Timing fallback of §2 and §10 applied: `l` at `standard` drops from 40 000 to 30 000; `quick` and `deep` unchanged. Batch 1 at 40k in the Release gate (`-j8`, games tests alongside): F32 19.9 s, F12 15.2 s; alone: F32 12.4–13.0 s, F12 14.6 s. At 30k alone: F32 11.0 s, F12 13.2 s | §10 trigger; measurements for the receipt |
