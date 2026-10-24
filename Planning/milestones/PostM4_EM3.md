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
| A3 | Ledger scope | T2: the tempo ledger covers every walked ply of every detailed line. The **timing check** becomes 15 s per F fixture at `standard` in Release (12 s before) |
| A4 | Tempo value | (a) Ship the number with labels on the verdict scale: **tempo-dependent position** above 0.10, **non-tempo position** below 0.05, **zugzwang** below 0 |
| A5 | Initiative | (a) Derived from the tempo ledger with no extra search: one row per run with holder, plies, tempi gained, how it ended (spent or contested) and converted or evaporated |

## 2. Rules the decisions fix

Notation as in M4: `S` the side to move at the root, `O` the opponent; `T` the side to
move on a given board; ES from the engine's WDL.

- **Searches per ply.** For ply k of a detailed line (board_k = the board after ply k,
  mover M): a null search on board_k (M to move again) gives the threat test for ply k
  and ES of passing on board_k; a normal search on board_k (T = M's opponent to move)
  gives the reply test for ply k−1 and ES of moving on board_k. Ply 1 reuses search E
  (null) and A's line (normal). About 2 × walked plies − 1 new searches per line.
- **Tempo value** on a board = ES_T(normal) − ES_T(null). At the root it comes from A and
  B; after a candidate from A's line and E; along a line from the two ledger searches.
  Null where T is in check (no pass is possible).
- **Skips.** No ledger when `null_move_reliable` is false (a negative tempo value is
  still reported). A check is threatening by rule, with no null search. The ledger stops
  at a board where the game is over.
- **Budget.** A new search kind for the ledger searches. Measured on 2026-10-24: about
  0.37 M nodes per second, so 50k nodes take 0.14 s and 100k nodes 0.27 s per search.
  With five detailed lines and about 12 ledger searches each, 50k nodes per search adds
  about 8.5 s to today's 6.3 s on V1–V3. The slowest `standard` F fixture took 7.91 s at
  PostM3_EM2 (F12), so 50k likely overshoots and 30k–40k is expected. The `standard`
  value is the largest of 50k, 40k, 30k and 25k that keeps every F fixture within 15 s;
  batch 1 measures it and the receipt records it. `quick` and `deep` scale with it as D
  and E do (×¼, ×4).
- **Whole-board selections.** `--moves playable` and `--moves all` detail more lines.
  Estimate at 40k nodes (0.11 s per search, about 12 per line): `--moves all` on the smoke
  position 37.9 s + 31 × 1.3 s ≈ 79 s (target ≤ 90 s); `--moves playable` 19.4 s + about
  8 s ≈ 27 s (target ≤ 30 s). The M4 targets stay; batch 1 measures both.

## 3. Evidence (probes of 2026-10-24, WDL at 400k nodes)

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
tempo at plies 1–2. At ply 3 `Rfc2` answers `Ba7` with a counter-threat, and the line
settles at ply 8 as a non-tempo position (tempo value 0.09) with Black the exchange up,
so Black's initiative is converted. V2 with `Ba7` first threatens on every ply at a
tempo value of 0.50 ± 0.01 and has no run. V1's `Qc4` line gains White a tempo with
`bxc5 dxc5`.

## 4. Open

- B1–B3, the impact rating: basis, use, and the relevance filter.
- Contract sections: schema v7 fields, headers, fixtures (Scandinavian, V2 both orders,
  V1, the zugzwang position, a quiet position), acceptance, work packages.

## 5. Contract changes

| Date | Change | Reason |
|---|---|---|
