# PostM4_EM3 — Tempo, initiative and the impact rating

Status: **tempo and initiative decisions frozen** (A1–A5, Captain, 2026-10-24). The impact
rating (B1–B3) is still open: the Captain is pointing the Deputy to further reading
first. Not yet written: the contract (headers, report schema, fixtures, acceptance,
work packages). The contract is frozen at checkpoint 1, together with the changed
concept diagrams; from then on, changes are contract changes recorded in the last section.

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

## 4. Open

- B1, basis of the impact rating: (a) consequence rank only; (b) consequence rank, then
  the static scale, dynamic facts first in a tempo-dependent position (Deputy); (c) an
  engine measure per fact (not computable: a doubled pawn cannot be removed and searched).
- B2, use: (a) order only, nothing cut; (b) the use test cuts positional changes from the
  move report, the full report keeps all (Deputy); (c) (b) plus a cap of N positional
  changes per move report.
- B3, net change: (a) today's "last change per feature that holds at the end"; (b) root
  against end of the walk, which drops passing changes (Deputy).
- Contract sections: schema v7 fields, headers, fixtures (Scandinavian, V2 both orders,
  V1, the zugzwang position, a quiet position), acceptance, work packages.

## 5. Contract changes

| Date | Change | Reason |
|---|---|---|
