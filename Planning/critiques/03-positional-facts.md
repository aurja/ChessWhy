# 03 — Positional facts

Date: 2026-10-22. Baseline: `23bf1a52d856060556147f73c1bd5f90c67c8d5d` (M3a complete,
report v4). Concept nodes: `PawnStructure`, `KingSafety`, `PieceActivity`,
`SpaceDevelopment`, `MaterialBalance`, `PositionalChange`, `TiedToLine`.

## Conclusion

M3a is a sound, conservative base of facts. Its predicates are deterministic, its
identities are documented, and it has boundary fixtures and an independent reference with
zero mismatches. The weaknesses are conceptual and sit at two seams:

- where a predicate carries a chess name;
- where a per-ply diff of identities is read as an explanation.

Only `exchange_up` fires widely where its own name does not fit. The other findings are
accepted, documented trade-offs. Their cost lands in M4: a persistent decisive feature is
never "changed by a line", so under the current `TiedToLine` meaning it cannot enter the
text, while incidental churn always can. Nothing here breaches the frozen M3a contract.

## What the approach does

- [SOURCE] `positional::features` computes 24 tags on one board, with `rule` provenance
  and no engine weighting ([M3a](../milestones/M3a.md) §3–§4). `position.positional` holds
  the root's features and appears in JSON only (§10).
- [SOURCE] `line_changes` compares identities between consecutive boards over the walked
  plies (6, extended while the next move is a capture, up to 10). `holds_at_end` compares
  each change with the last walked board (§5). A feature whose identity persists while its
  pieces or value change is explicitly "not a change" (§5; PL04–PL06).
- [SOURCE] Captain's decisions: every ply is measured (P2, option C), every change is
  printed and M4 selects (option A, [CURRENT_STATE](../CURRENT_STATE.md)), thresholds are
  set by P4 and definitions by P5. `TiedToLine` is implemented only as "the renderer
  prints line changes" ([CODE](../DBC/CODE.md)).

## Findings

### POS-1 — `exchange_up` counts piece types, but its name names a trade (contract-level concern)

**Root cause.** §4.5 says "X has more rooks than Y, and Y has more knights plus bishops
than X" (identity: tag and side). This is documented in
[DEFINITIONS](../DBC/DEFINITIONS.md) §9, not an undocumented choice. The ordinary term
means a rook for *one* minor piece, but the rule also fires for:

- a rook against two minors. That is the `Imbalances` case that
  [CONCEPTS](../DBC/CONCEPTS.md) defers, because "a wrong label would still carry `rule`
  provenance" ([vision](../PROJECT_VISION.md) §5.6);
- positions where other material dwarfs the trade.

**Evidence.** [OBSERVED] Four boards where the side to move is checkmated, so no search
runs (`position.terminal`). Command: `build/release/chesswhy --fen FEN --json --no-llm`;
every run exited 0.

| FEN | Material | `material_ahead` | `exchange_up` |
|---|---|---|---|
| `3R2k1/5ppp/8/8/8/8/5PPP/2b3K1 b - - 0 1` | R v B | white 2 | white |
| `3R2k1/5ppp/8/8/8/8/5PPP/2bn2K1 b - - 0 1` | R v B+N | black 1 | white |
| `6k1/R7/8/8/8/4n3/6q1/7K w - - 0 1` | R v Q+N | black 7 | white |
| `6k1/8/8/8/8/7b/6q1/RR4K1 w - - 0 1` | 2R v Q+B | black 2 | white |

[OBSERVED, throwaway python-chess re-implementation of §4.5, not the shipped detector]
Run on the `positional_oracle` boards (each puzzle FEN in
[test.csv](../../tests/fixtures/puzzles/test.csv) plus every solution ply), it reproduces
the receipt's 53,378 boards and 7,662 `exchange_up` boards exactly. Of those 7,662:

- 5,242 (68%) have the single-exchange shape: one more rook, one more enemy minor, equal
  queens;
- 1,075 (14%) are a rook against two minors;
- on 1,373 (18%) the owner is behind in material.

The preliminary critique says the rule "says nothing about net material". That is
accurate, but net material is not the defect: "the exchange for two pawns" is ordinary
usage. The defect is the shape. Fixtures P55–P57 hold only R v N for each colour and an
R v Q near miss.

**Impact.** [INFERENCE] The text can pair `exchange_up` for White with `material_ahead`
black 7. M5's tag-to-keyword book query (vision §8) would fetch "winning the exchange"
passages for a side that is a queen down.

**Strongest defense.** `material_ahead` carries the net count, and the predicate is exact
and checked by the oracle.

**Recommendation.** [PROPOSED, Captain's decision] Either restrict the rule to the
single-exchange shape, or keep it and document the compound cases in DEFINITIONS with P
fixtures that pin them. Either way, M4 and M5 never paraphrase `exchange_up` without
`material_ahead`.

**Experiment.** Rerun the four boards against the chosen rule, and record the shape table
in a receipt.

### POS-2 — Persistent decisive features stay silent while incidental churn is listed (accepted limitation; prospective `TiedToLine` risk)

Suppressing persistence is explicit, accepted M3a behaviour (§5, PL04–PL06, P2, option
A). This finding is about its cost to the explanation, not about a hidden bug.

**Evidence.**

- [OBSERVED] King and pawn ending: `build/release/chesswhy --fen
  '6k1/5ppp/8/8/8/1P6/5PPP/6K1 w - - 0 1' --move b4 --budget standard --json --no-llm`.
  - The root scores ES 1.0 (cp 523), and its `position.positional` holds White's
    `passed_pawn` on b, which is never printed.
  - b4 is a passed-pawn push, yet there is no row at ply 1.
  - The asked line and the five candidates carry 89 rows in total. Every one is
    `king_shield_gap` or `open_file_near_king`, produced by king walks.
  - At ply 6, when …Kd6 leaves rank 2, three Black `king_shield_gap` rows disappear. That
    reads like a repaired shelter.
  - The engine did not advance the pawn again within the walked plies, so this
    demonstrates persistence, not a pawn race.
- [OBSERVED] Doubling rooks on the seventh: `--fen '4r1k1/R4ppp/8/8/8/8/5PPP/3R2K1 w - -
  0 1' --move Rdd7 --budget standard --json --no-llm`. The asked Rdd7 is a blunder
  (Re1#), so the case shown is the best line `h4 h5 Rdd7 Rf8 Rdc7 Kh7…`. At ply 3 (Rdd7):
  - no `rook_seventh` or d-file row;
  - `king_zone_attacker` R a7 disappears (there is no x-ray, §4) and R d7 appears;
  - `mobility_lead` white 13 disappears and returns as 15 at ply 4.

  The classic strengthening move reads as an attacker swap and a lost lead.
- [OBSERVED, parent's run] On the README FEN (`--move Nf6 --budget standard --no-llm`),
  the line `d5 Bxd5 Qf6 Bxc6+ bxc6…` prints 22 rows in six plies. Because
  `material_ahead` takes its identity from its value, the Bxc6+ bxc6 exchange alone prints
  five rows (white 1, then 4, then 1).

**Root cause.** Identity is chosen per tag, the diff runs ply to ply only, the king-file
predicates ignore the game phase, and attack sets have no x-ray.

**Impact.** [INFERENCE] CONCEPTS admits a fact to the explanation "only when a line or
verdict changes it". The b-pawn never qualifies; king-file churn always does.

**Strongest defense.** M3a promised changes, not reasons. The Captain kept comprehensive
output, and M4 owns selection.

**Recommendation.** [PROPOSED, M4 fact sheet, no schema change]

- Derive a net diff of identities from the root to the end of the line, using existing
  fields.
- Demote rows with `holds_at_end` false unless a motif or verdict acts at the same ply.
- Decide whether a verdict can tie a root feature.

End-of-line values for persistent features would be a separate schema decision.

**Experiment.** For these three commands, the fact sheet names the b-pawn, the doubled
seventh-rank rooks and the net +1, within a stated item budget.

### POS-3 — `holds_at_end` means "present on the last walked board" (contract-level wording concern)

**Evidence.**

- [SOURCE] A change at the last walked ply holds by construction. `MoveAssessment` carries
  no walked-ply count ([report.hpp](../../include/chesswhy/report.hpp)), yet the text
  prints lines of up to 10 plies.
- [OBSERVED, parent's run] README line `Qe7 Nc3 Nd4 Qd1 c6 Nf3 Nf6 O-O d6 Nxd4` prints
  `+ development_lead: white 2 (ply 6) [rule]` with no "not at end".
- [OBSERVED, throwaway §4.4 count] The next printed move, …Nf6 at ply 7, drops that lead
  to 1, below the threshold.
- Similarly, all seven rows of `Nf6 Qxf7#` fall on its final ply.
- [SOURCE] "Lasting" in the M3a intent and in DEFINITIONS describes tag families, not a
  measured duration.

**Impact.** [INFERENCE] Prose could claim "a lasting lead" from a flag that only means
"still there at ply 6".

**Strongest defense.** DEFINITIONS defines the flag exactly, and motifs and consequences
use the same horizon.

**Recommendation.** [PROPOSED] The fact sheet says "still present at ply N, the last ply
read", and the fact check rejects duration words on positional facts. Emitting walked
plies would be a schema decision.

**Experiment.** A seeded sentence "White keeps a lasting development lead" for this Qe7
line is rejected.

### POS-4 — Lead thresholds fix prevalence, not stability or value (prospective risk)

**Evidence.** [SOURCE] §4.4 sets the thresholds so that a mobility or space lead holds in
about one broadcast position in six, publishes the shares at threshold − 1, and claims
nothing about outcome or stability. [OBSERVED, parent's run] On the README position:

- Nh6 line: `+ mobility_lead: white 10 (ply 2, not at end)`, then
  `- mobility_lead: white 10 (ply 3)`;
- Qe7 line: white 12 lasts two plies;
- P2 above: the lead vanishes at the doubling move.

**Root cause.** A per-board difference is compared with a fixed cut, with no hysteresis,
so each side's developing move pushes the difference back and forth across the cut.

**Strongest defense.** P4 is the Captain's decision. Calibrating on 2200+ games rather
than puzzles is a strength, and engine weighting is out of scope.

**Recommendation.** [PROPOSED, non-gating] Measure:

- the flicker rate: leads that disappear within two plies, on F lines and on broadcast
  continuations;
- the leading side's game score, at the threshold and at threshold − 1.

Thresholds remain the Captain's to set.

**Experiment.** Record the numbers. Prose never relies on a lead that lasts one ply.

### POS-5 — Sampled conformance is checked, chess adequacy is not measured (accepted limitation; scope proposal)

**Evidence.**

- [SOURCE] P labels come "from the plain §4 definitions", and the reference was "written
  from this plan" (§8). Zero mismatches establish agreement on the compared boards, not
  universal fidelity or agreement with chess terms. POS-1 passed both checks.
- [SOURCE] The ten book positions check `pawn_islands` (5), `doubled_pawns` (2),
  `passed_pawn` (2), `outpost` (1) and a `pawn_chain` near miss (1). None checks
  `bad_bishop`, `exchange_up`, the leads, king safety, backward pawns or isolated pawns
  (P07 is constructed).
- [SOURCE] Puzzle boards skew the §9 counts (castling rights on about 10% of boards,
  `development_lead` on 1,208 of 53,378) but not fidelity: every tag has includes and near
  misses, and the unit tests cover both colours, edge files, promotions, en passant and
  castling-right loss (§8, §11).
- [SOURCE] I rebut the general "sounds like an assessment" worry: `bad_bishop`, `outpost`
  and `development_lead` are standard technical names for structural predicates, and the
  P5 rules are conservative. `bad_bishop`, for example, needs two blocked pawns plus a
  majority, stricter than the rejected "majority only" alternative. The real risk is
  paraphrase.
- One gloss overclaims: DEFINITIONS §9 says no Y pawn "can ever attack" an outpost, while
  §4.3 checks only Y pawns already on adjacent files. [INFERENCE, unrun] A pawn two files
  away can capture onto an adjacent file.

**Strongest defense.** Adequacy was never an M3a acceptance criterion, and the deferrals
guard against the riskiest labels.

**Recommendation.** [PROPOSED, non-gating audit] For each evaluative tag, the Captain rates
about 20 broadcast boards where the rule fires and about 20 annotated positions that use
the term. Record the share of labels she accepts, the share of annotated features detected,
and the disagreements. Any rule change still goes through a contract change.

**Experiment.** A table per tag. Tags below the Captain's bar get a glossary note or a
rule proposal.

## Strengths to retain

- Pure predicates that never call the engine, with `rule` provenance. The template prints
  only report fields.
- Exact identities per tag, pinned by PL01–PL07.
- An independent reference with zero mismatches, plus boundary fixtures.
- Named structures and imbalances deferred for the right reason.
- Thresholds calibrated on strong real games, with the shares at threshold − 1 published.
- Root features kept out of the text, and selection left to M4.

## Limitations and verification receipt

- **I ran:** the four terminal boards, P1 and P2 (`standard`), all exit 0 with no stderr.
  I also ran throwaway python-chess counts, not committed.
- **From the parent's shared `standard` run:** the README rows.
- **Not run:** tests, builds, the proposed audits, the outpost counterexample and the
  broadcast measurements.
- **Limits:** each engine line is one bounded run. The puzzle-board shares include boards
  in the middle of capture sequences.
- **Already documented by the repository:** persistence (§5), the walked-ply horizon
  (DEFINITIONS), full output (option A) and the deferrals (vision §5.6).
- **Incidental drift:** DEFINITIONS §9 still says "Planned until M3a is built" and calls
  the lead threshold "open until the Captain approves", although P4 decided it.
- **Cross-links:** [10 — Explanations](10-explanations.md) on M4 selection;
  [07 — References](07-references.md) on M5 queries.

## Sources

- [M3a](../milestones/M3a.md) §1, §3–§5, §8, §10, §11; [receipt](../receipts/M3a.md) §9
  counts and "Volume along lines".
- [DEFINITIONS](../DBC/DEFINITIONS.md) §8–§9; [CONCEPTS](../DBC/CONCEPTS.md), "Positional
  facts"; [CODE](../DBC/CODE.md); [FIXTURES](../DBC/FIXTURES.md) §8.
- [Vision](../PROJECT_VISION.md) §5.6, §8; [CURRENT_STATE](../CURRENT_STATE.md).
- [positional.cpp](../../src/positional/positional.cpp): `add_leads_and_material`,
  `same_identity`, `line_changes`. [report.hpp](../../include/chesswhy/report.hpp):
  `MoveAssessment`. [analyse.cpp](../../src/analysis/analyse.cpp): `assess_move`,
  `analyse`. [render.cpp](../../src/render/render.cpp): `write_positional_changes`.
- [positional.json](../../tests/fixtures/positional.json) P01–P70, PL01–PL07;
  [positional_oracle.cpp](../../tests/positional/oracle/positional_oracle.cpp).
