# 01 — Threats, consequences, urgency and parry tests

Date: 2026-10-24. Baseline: `c601a6b769eb5fba3a92b6f009ae99b88b2832b8`.

Concept nodes: `Threats`, `SideThreat`, `OpponentThreat`, `NullMoveReliable`, `WalkedPlies`, `Consequence`, `ConsequenceRank`, `Urgency`, `ParryTest`; all `Cq*`, `Ur*` and `Pt*` nodes in their L3 tables. Investigation resumed after the original baseline `8b561c4fcfe58a8226910234b40413b9880f62e4`; current-baseline runtime evidence below uses the existing Release binary, not a rebuild.

## Conclusion

The strongest new defect is an omitted outcome: a stalemating capture becomes a material achievement followed by an opponent's “refutation”, although no opponent reply exists.
The material arithmetic is correct; treating its classification as the outcome is not.
Second, capture-only extension can cut an exchange before a checking zwischenzug and its forced recovery, reversing the apparent material result even inside the ten-ply cap.
That second finding is demonstrated on a legal supplied line, not claimed as an observed engine PV.
The explicit pass premises, skip reasons, signed material accounting and reported walk length remain useful and sound within their stated bounds.
Urgency's rule tree is mechanically exhaustive, but ranks and the parry result do not independently establish necessity or speed.
I found no additional demonstrated urgency/parry defect beyond the prior audit's concerns; EM2 deliberately retained those tests and clarified their premises, while M4 omitted urgency from move reports.
Neither finding revisits the Captain's decided pure-reply fix.

## Findings

### THR2-1 Terminal draws fall outside both consequence and refutation

**Definition — Source.** `Planning/DBC/DEFINITIONS.md:128`: “What a line achieves for the side playing it, computed by rule from the engine line: `class`, `mate_in`, `material`, `promotes`, `plies`.” Line 131: “`mate` (engine mate score for the side), else `material` (net gain ≥ 1 pawn over the walked plies), else `promotion`, else `positional`.” Line 139 defines refutation as “`O`'s best reply line after it, as a threat by `O`.” `CONCEPTS.md:753` describes the positional residual as “Rank 0: only the evaluation changes”.

**What it claims vs what it computes.** Consequence purports to describe achievement, but combines one terminal outcome, mate, with material transactions and a residual. Stalemate has no outcome representation. Refutation assumes every unplayable move can be answered. A capture's nominal gain remains true, but neither a material class nor an empty positional threat states why this move threw away a win.

**Counterexample — Observed.** Python-chess checked that `8/8/8/8/6q1/6R1/5k2/7K b - - 0 1` is valid and `Qxg3` is legal, leaves zero legal moves, and stalemates. Run:

```text
build/release/chesswhy --fen "8/8/8/8/6q1/6R1/5k2/7K b - - 0 1" --moves Qxg3 --full-report --json
```

Exit 0. The asked move has ES `0.5`, verdict `mistake`, line `["Qxg3"]`, consequence `material`, material `5`, plies `1`, and `is_tactic: true`. Its created threat correctly skips for `game_over`. Nevertheless its refutation is a White threat with `line: []`, consequence `positional`, material `0`, plies `0`. White cannot play the purported reply. The earlier-baseline mirror, `7k/5K2/6r1/6Q1/8/8/8/8 w - - 0 1`, `Qxg6` with the same flags, produced the same pattern.

**Root cause.** Non-exhaustive outcome categories and composition (families 4, 5): `rules.cpp:167–175` special-cases winning mate but not terminal draws; `analyse.cpp:359–368` manufactures a reply threat for every non-playable move, including an empty suffix. These follow the conceptual contracts, rather than miscomputing the captured rook's value.

**Inherited by.** `CqIsMaterial`, `CqIsPositional`, refutation, tactic facts and explanations of mistakes. Rank-based consumers cannot distinguish terminal drawing captures from continuing material gains from consequence alone; no incorrect root urgency is claimed for this example.

**Candidate fixes.**

1. Add an orthogonal terminal outcome to consequences, computed while replaying the line. Preserve `material: 5`, but lead with “stalemates; draw”. Board-rule cost only, no search; slightly larger report contract. Stalemate/checkmate can be exact without unavailable game history.
2. Permit a non-playable move to have no refutation when it ends the game, with an explicit terminal reason. Derive that reason from the post-move board. Negligible cost; consumers must stop assuming every mistake has an opponent reply.

**Severity:** `wrong claim` — the refutation definition asserts an opponent reply where none exists. The material number and tactic flag satisfy their literal contracts; they are not themselves arithmetic errors.

### THR2-2 Capture-only extension can stop before a forced exchange recovery

**Definition — Source.** `Planning/DBC/DEFINITIONS.md:129`: “How far the consequence rule follows a line: 6 plies, then on while the next move is a capture, up to 10.” Read together with line 128's “What a line achieves” and line 131's material classification.

**What it claims vs what it computes.** The horizon is explicitly bounded, but extending only captures does not establish a settled exchange. A checking intermediate move and its evasion can separate the material paid from the material recovered. This is a more specific surviving angle on prior THR-4: recovery can already be inside the displayed ten-ply line, not merely beyond the maximum horizon.

**Counterexample — Observed board check; Source classification.** Valid FEN:

```text
6rk/p5p1/8/8/2B5/8/PB6/K3R3 w - - 0 1
```

Python-chess legally replayed every move of:

```text
a3 a6 a4 a5 Bxg7+ Kxg7 Rg1+ Kh6 Rxg8
```

White's material change is `+1` at ply 5, `−2` at ply 6, and `+3` at ply 9. After `Rg1+`, all five legal evasions (`Kh8`, `Kf8`, `Kh7`, `Kh6`, `Kf6`) allow `Rxg8`, and none then permits a capture of White's rook. The c4 bishop protects g8. Yet the source rule stops at six because ply 7 is not a capture (`rules.cpp:126–132`), discarding this recovery before the ten-ply cap. With a non-positive/non-mate source score, its consequence is positional, material `−2`, not the supplied line's recovered `+3`.

This is a legal-line counterexample to the rule's semantic interpretation, **not** a claim that Stockfish selects these opening moves or emits this PV. No CLI flags force a whole PV; no runtime result is invented.

**Root cause.** Wrong stopping time and composition (families 2, 5): “the next move is not a capture” is used as the stopping condition even when a forcing intermediate move leaves the exchange unresolved. A prefix balance is subsequently consumed as gain/loss evidence. The rule itself accurately reports that prefix.

**Inherited by.** Consequence rank, threat classification, urgency, M4 `is_tactic`/`allows_tactic`, and downstream material-based tempo/initiative tests. Their arithmetic can change sign solely because the exchange is interrupted by check.

**Candidate fixes.**

1. Continue through checks and mandatory evasions as well as captures, within the existing ten-ply cap. Reuses the PV and board rules, no searches; fixes this example but not arbitrary quiet intermediate threats or cap exhaustion.
2. Expose the endpoint's incompleteness and call the number “material change through ply N”, not a completed gain/loss. At minimum flag a pending forcing continuation visible in the unused PV. Small replay cost; conservative wording is needed because no fixed horizon proves an exchange settled.

**Severity:** `misleading` — the bounded number is true, but its outcome interpretation is stronger than its evidence.

## Sound definitions

- **Threat / SideThreat / OpponentThreat, operational portion:** A's best line and B's conditional line have unambiguous roots; this does not certify “biggest problem”.
- **Material accounting:** signed captures and promotion deltas correctly measure change from each line's own root, not lifetime winnings.
- **Walked plies / `walked_plies`:** exact, reproducible bounds and useful disclosure; not a quiescence certificate.
- **Mate / `mate_in` / `CqMate`:** positive engine mate scores are distinguished from mere material gains; the mate may extend beyond the material walk.
- **`promotes` / `CqPromotion`:** occurrence of a promotion inside the walk, not survival of the promoted piece.
- **Rank:** deterministic encoding of the ordered class tests; categories are disjoint by precedence, not independent chess outcomes.
- **`null_move_reliable`:** the false/null conditions are exact as a material/check flag, not a proof of reliability when true.
- **`creates_threat` / skip reason / conditions:** the conditional premise and reasons for skipping are explicit; no novelty inference is warranted.
- **Parry legality branch / provenance / not-judged state:** correctly distinguish illegality, searched evidence and an untested low-rank threat. The ES branch is sound only as its specified baseline comparison, not proof of prevention.
- **`in_check`:** a legal reply must answer check.
- **Urgency dispatch (`quiet`, differing ranks, equal-rank parry/counterattack):** mechanically exhaustive after the check branch; “quiet”, “more urgent” and “who is faster” remain interpretations, not additional observations. No new independent counterexample is claimed here.
