# 04 — Threats, parry tests and urgency

Date: 2026-10-22. Baseline: `main` at `23bf1a52d856060556147f73c1bd5f90c67c8d5d` (report v4),
existing Release binary, project Stockfish 19, `Threads=1`.
Concept nodes: `SideThreat`, `OpponentThreat`, `NullMoveReliable`, `Consequence`,
`ConsequenceRank`, `Urgency`, `ParryTest`, `CreatedThreat` (L3: `PtByCheck`, `PtWorks`,
`UrParry`, `UrRuleCounter`, `UrQuiet`).

## Conclusion

The inspected paths follow [M1 §4.2–4.7](../../milestones/M1.md); I found no computation
defect against the frozen contract. The demonstrated weaknesses lie in what the names
promise. Three are observed at runtime:

1. Up to search noise, the searched parry test asks "is the move worth more than 0.03 ES
   than passing?". Counter-races are reported as parries (F10: `a7`, `Rb8`), and urgency's
   `counterattack` is in practice reached through checks and mates.
2. Checks, and even checkmate, print `parries threat: false`.
3. `creates_threat` is an absolute line from a hypothetical position: it repeats threats
   that already existed and ignores the move's actual refutation.

`positional`, `quiet` and the null-move flag are documented horizon and material proxies
whose names say more than they test. Every recommendation below reuses data the current
searches already produce; none needs a new search.

## What the approach does

| Fact | Computation (`standard`) |
|---|---|
| `S`'s threat | A's best line (MultiPV 5, 1,000,000 nodes) |
| `O`'s threat | B: `S` passes, single PV, 200,000 nodes, skipped in check; `t` = its first move |
| consequence | 6 plies, then on while captures continue, at most 10; pawn-unit material; mate only from an engine mate score; rank 4/3/2/1/0 for mate, ≥ 3, ≥ 1, promotion, positional |
| `null_move_reliable` | false iff `S` has at most one piece besides king and pawns |
| parry test D(m) | only if `O`'s rank ≥ 2. `t` illegal after m: rule (`false` if m mates, or checks with `t` pseudo-legal; else `true`). Otherwise `searchmoves t`, 100,000 nodes; parries iff ES_O(D) < ES_O(B) − 0.03 |
| urgency | in check → `O`; both ranks < 2 → `quiet`; different ranks → higher; equal → D on A's best move: not parried → `S` (`counterattack`), parried → `O` (`parry`) |
| `creates_threat` | E: after m and `O`'s pass, single PV, 100,000 nodes; null if m checks or ends the game |

[DEFINITIONS §3.3](../../DBC/DEFINITIONS.md) says a threat is "not the everyday 'something
attacks something'", and ranks gate both D and urgency, so a merely playable line (rank 0–1)
never drives a parry or urgency decision. "What a side would play is not a punishable
threat" is therefore a wording risk (THR-4), not a computation error.

## Findings

### THR-1 — "Parries" measures beating a pass, not stopping `t` (contract-level concern; observed)

**Root cause [SOURCE].** [`search_parry`](../../../src/analysis/analyse.cpp) (line 291)
compares D against B, whose baseline has `S` passing instead of playing m (M1 §4.5).
**[INFERENCE]** D only restricts `O` to `t`, so in exact play ES_O(D) ≤ 1 − ES_S(m). With
p = 1 − ES_O(B), `S`'s score after a pass, "the threat still works" implies
ES_S(m) ≤ p + 0.03. Whenever `t` stays legal and m does not check, any m worth more than
0.03 over passing is a "parry", whatever happens to `t`. A, B and D use 1M, 200k and 100k
nodes, so this holds up to search noise.

**Evidence [OBSERVED].** F10, `--move a7`: `O`'s threat `f1=Q a7 Qb1+ Ka5 Qa2+ Kb6 Qxg8`,
material 13, rank 3. `a7` (fine, .5435) prints `parries true (searched)`, yet its own line
`a7 f1=Q a8=Q+ …` has Black play `t` at once and queen. `Rb8` (.5) also "parries", while its
refutation `f1=Q a7 Qb1+ Ka5 Qa1+ Kb6` has class material, material 8, the same rank as B.
`Rf8` (`Rf8 f1=Q Rxf1`) truly neutralises the new queen and gets the identical label.

**Impact.** The fact cannot distinguish preventing, neutralising and outweighing `t`, and
urgency rule 4 inherits that (THR-2). "Another route to the same consequence" is the
documented narrow meaning: fixture F04 pins `Ra1` parrying `Rc1#` while blundering into
another mate.

**Strongest defence.** M1 §4.5 replaced a rank comparison that flickered at the horizon (old
F11). `Rb8` supports the ES choice: its +8 counts Black's queen while White's counterplay
lies beyond the walked plies, and ES .5 (a draw) is the better summary. With a saturated
baseline the 0.03 band means "no longer decisive": the parent's direct UCI run of F12's B
gave `wdl 1000 0 0` at 200k nodes, unchanged when restricted to `t` at 100k and 200k.

**Recommendation [PROPOSED].** Keep the ES test, described as "after m, `t` no longer keeps
B's score". Add two rule facts from data in hand: whether `t` is `O`'s reply in m's own
line, and that reply line's consequence for every assessed move (the refutation code already
computes it for moves that are not fine). Report B's ES so the pass baseline is visible.
Prose may say "`t` is illegal here" when the legality test establishes it. Otherwise say
"`t` is not the displayed best reply"; absence from one PV does not prove it was stopped.

**Acceptance/falsification.** F10 → `a7`, `Rb8`, `Rf8` "`t` still played", reply ranks 1, 3,
1 (`a7` computed by hand from its line); README FEN → `Qe7`, `Qf6`, `Nh6`, `d5` "not
played", reply ranks below 4; F04 → `Ra1` reply rank 4 equals B ("another route").
Falsifier: log B and D ES on F08 (`--fen "r1bb1r1k/p4ppp/2p2n1N/7Q/2p5/2B2N2/qPP2PPP/2KR3R w - - 2 19" --move Qxf7`,
unrun) and further boards; non-checking moves printing `parries false (searched)` with
ES_S(m) > p + 0.03 would show noise dominates the inequality.

### THR-2 — Checks and mate "do not parry"; `counterattack` mostly means "the best move checks" (contract-level wording concern; observed)

**Root cause [SOURCE].** `search_parry` lines 277–278 implement M1 §4.5 rule 1 "(or m
mates)"; [CONCEPTS](../../DBC/CONCEPTS.md) `PtByCheck` says "A check only postpones the threat";
urgency rule 4 consumes the boolean (`analyse`, lines 507–510).

**Evidence.** [OBSERVED] Constructed `6k1/1p3ppp/8/8/8/8/1q4PP/R6K w - - 0 1`: both threats
are mate in 1 (`Ra8#`, `Qxa1#`); the text prints
`Candidate 1 parries threat: false (deferred_by_check) [rule]` and
`Candidate 1 creates threat: null [rule]` for `Ra8#`, then
`Threat urgency: white, rule=counterattack`. [OBSERVED, parent run] F22 FEN
`k7/8/8/8/3Q3n/8/2P5/4K3 w - - 0 1` at `quick`: best `Qh8+`, line `Qh8+ Kb7 Qxh4`, captures
the knight behind Black's `Nf3+` fork threat, yet prints `parries false (deferred_by_check)`
and urgency `counterattack`; at `deep` the best is `Qxh4` (`threat_move_illegal`), `S`'s
threat is mate in 8 and the rule is `consequence_class`. White is more urgent at both.

**[INFERENCE]** With THR-1, rule 4 yields `counterattack` only when A's best move checks or
mates, or beats passing by at most 0.03 (saturation, near-zugzwang). A strong non-checking
counterattack lowers `O`'s ES and prints `parry`, contrary to the stated intent in
[vision §5.3](../../PROJECT_VISION.md). The `counterattack` fixture F09 pins fine `Nf3+` and a
fork by the f3 knight on Black's own threat line, consistent with the check path (unrun:
`--fen "8/6pk/7p/5p2/5Pqn/1P5P/P2Q2P1/2r2RK1 b - - 0 39" --move Qg3`).

**Strongest defence.** The rule skips a D search where `t` is illegal only because of check;
a check does force `O` to answer first; both urgency sides above are right.

**Recommendation [PROPOSED].** A third parry value meaning "not tested (check)", a separate
case for mate, and a reason for a null `creates_threat` (check versus no threat). For checks,
THR-1's reply facts already show `Qh8+` removing `t`'s piece at ply 3. Changing rule 4's
outcomes touches F07–F09 and is the Captain's decision. **Acceptance:** the mate board and
F22 at `quick` no longer print "false"; urgency sides are unchanged.

### THR-3 — `creates_threat` is absolute and hypothetical (contract-level concern; observed)

**Root cause [SOURCE].** [`assess_move`](../../../src/analysis/analyse.cpp) (lines 321–336)
stores E's line with no baseline. Vision §5.4 separates "executes or keeps `S`'s threat"
from "creates a new threat (E)"; no "keeps" fact was built. `NullMoveReliable` flags only
`OpponentThreat`; in E the passing side is `O`, unflagged.

**Evidence.** [OBSERVED] Trapped knight, `1k6/8/8/8/8/8/6K1/3R3n w - - 0 1`: White's threat
is mate in 14 with `Kxh1` at ply 5; all five candidates are fine (ES 1.0); `Rd7`, `Kf3` and
`Rc1` each "create" class material, material 3, taking the h1 knight that candidates `Kxh1`
and `Rxh1` already take. [OBSERVED, parent run] README FEN: asked `Nf6` "creates" `Nxh5`
(material 9) while its refutation is `Qxf7#`; the text prints both with no "if White
passes", which only the JSON `root_fen` conveys. F10 (mine): `a7` "creates" mate in 5 from a
root where Black skipped `f1=Q`.

**Strongest defence.** The definition is documented (M1 §4.7), `root_fen` exposes the
hypothetical root, and genuine creations are right (README `g6` → `gxh5`, material 9;
`d5` → `dxc4`, material 3).

**Recommendation [PROPOSED].** Show E beside the actual reply and explicitly state
"if the opponent passed after m". Compute the material-based null-move warning for E
too. Comparing root and E ranks or first moves can suggest examples to inspect, but
cannot establish novelty: different threats can share a rank, and A lists only five
moves. Do not infer that E is irrelevant merely because a refutation has equal or
higher rank. **Acceptance:** the trapped-knight cases no longer assert that the knight
win is new; README `Nf6` states that White can mate instead of passing; genuine new
attacks such as `g6` → `gxh5` retain their conditional line. A general novelty label
requires separately specified and validated evidence.

### THR-4 — `positional`, `quiet` and "biggest problem" are horizon residuals (accepted limitation; M4 wording risk)

**[SOURCE]** [`consequence`](../../../src/analysis/rules.cpp) makes `positional` the residual:
no positive mate score, under one pawn within the walked plies, no kept promotion. M1 §4.3
dropped the vision's ES swing and [M3a](../../milestones/M3a.md) P3 reads no positional changes
on threat lines, so the class carries no positional content. `quiet` means both ranks are
below 2, and [DEFINITIONS §3.4](../../DBC/DEFINITIONS.md) adds "the explanation leads with
positional facts"; vision §5.2 and `OpponentThreat` call `O`'s threat "White's biggest
problem". All of this is documented.

**Evidence.** [OBSERVED, parent run] F01: `quiet`, both threats positional, yet the root line
is a tactic (`O-O-O` .9825 against `Kf1` .598, margin .3845) and the other four candidates
are blunders; `Kf1`'s refutation is positional, material 0 (tactic side:
[02 — Tactics](02-tactics.md)). [OBSERVED] Trapped knight: Black's "biggest problem for
White" is a line in which Black loses its knight (positional, material −3). [SOURCE] M1
§16: the old F11's B line (`Kb7`, promotion at ply 7, cp 1021) was positional; the fixture
was replaced.

**Rebuttal.** Ranking material over promotion is less crude than alleged: a promotion that
keeps its piece counts as material (+8, rank 3). Class `promotion` survives only when that
material returns within the walk: F10's race line (both sides queen, net 0) and the unit
test `a8=Q Rxa8` (−1).

**Strongest defence.** Captain-accepted (F01 is pinned as quiet), objective and
reproducible.

**Recommendation [PROPOSED, wording].** Describe `positional` as "no mate, no pawn gained
and no kept promotion within the walked plies"; speak of `O`'s line as a problem only from
rank 2; in M4, do not lead with positional facts when `candidates[0].tactic.is_tactic` or any
candidate is a blunder ([10 — Explanations](10-explanations.md)). **Acceptance:** F01's fact
sheet states "only fine move; root tactic" before positional facts.

### THR-5 — `null_move_reliable` is a one-sided material proxy (accepted limitation; source)

[`null_move_prone`](../../../src/analysis/rules.cpp) counts `S`'s pieces; `true` asserts more
than it tests; the vision's risk table documents this mitigation, and E is unflagged
(THR-3). **Recommendation [PROPOSED, no new root search]:** expose when the pass search
scores better than A's searched best move (p > ES_S(best) + 0.03), as a diagnostic
alongside the material warning, not proof of zugzwang. Different budgets and WDL
contexts can also create that difference; see [06 — Searches](06-searches.md).
**Experiments (unrun):** trébuchet `8/8/8/2Kp4/3Pk3/8/8/8 w - - 0 1`
(flag false; pass-better expected); Sämisch–Nimzowitsch, Copenhagen 1923, after 25…h6,
`6k1/3q2p1/p2bp2p/3p1r2/1p1Pp3/3bQ1PP/PP1B1rB1/1N2R1RK w - - 0 26`
(replayed legally with `chesswhy-pgn`; flag true).

## Strengths to retain

- Explicit operational definitions; ranks gate D and urgency; mate only from engine scores;
  one walk shared with motifs.
- Rule/engine provenance split for parry tests (M1 §16); F04 documents the narrow meaning;
  B skipped in check and guarded against a stalemated null position (F19, F20).
- Every threat carries `root_fen`; output is deterministic.
- The ES test is sound engine evidence that `t` stops paying; all proposals reuse A's lines,
  B's WDL and the refutation code.

## Limitations and verification receipt

Run by me, sequentially at `standard`, each exit 0 with empty stderr (other agents ran
engines concurrently: no timing claims):

- `build/release/chesswhy --fen "6R1/8/P7/8/1K6/6p1/5pk1/8 w - - 0 58" --move a7 --json`
- `build/release/chesswhy --fen "6k1/1p3ppp/8/8/8/8/1q4PP/R6K w - - 0 1"`, with and without `--json`
- `build/release/chesswhy --fen "1k6/8/8/8/8/8/6K1/3R3n w - - 0 1" --json`
- `build/release/chesswhy-pgn` on a temporary PGN of the 1923 game (no engine).

Parent-run: the README FEN with `--move Nf6` at all budgets; F22 at `quick` and `deep`; F01
at `standard`; direct UCI searches of F12's B. The report omits B's and D's ES, so THR-1's
inequality is inferred from source. F08, F09, the zugzwang boards and the old F11 are
unrun; F04, F09 and F10 expectations are historical fixture receipts.

## Sources

[analyse.cpp](../../../src/analysis/analyse.cpp) (`search_parry`, `assess_move`, `analyse`);
[rules.cpp](../../../src/analysis/rules.cpp) (`null_move_prone`, `walked_plies`, `consequence`,
`consequence_rank`); [report.hpp](../../../include/chesswhy/report.hpp) (`Threat`, `ParryInfo`,
`Threats`); [render.cpp](../../../src/render/render.cpp) (`write_threat`);
[report.v4.schema.json](../../../schema/report.v4.schema.json);
[positions.json](../../../tests/fixtures/positions.json) (F01, F04, F07–F10, F22);
[unit_tests.cpp](../../../tests/analysis/unit_tests.cpp); [M1](../../milestones/M1.md) §4.2–4.7,
§9, §16; [M3a](../../milestones/M3a.md) P3; [vision](../../PROJECT_VISION.md) §5.1–5.4 and risks;
[DEFINITIONS](../../DBC/DEFINITIONS.md) §3.3–3.4; [CONCEPTS](../../DBC/CONCEPTS.md) L2/L3;
[CODE](../../DBC/CODE.md).
