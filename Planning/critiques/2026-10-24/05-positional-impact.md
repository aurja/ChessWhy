# 05 — Positional facts, use and impact

Date: 2026-10-24. Baseline: `b86bbf08683ff99b9141217393d92c981ae629a3`.

Concept nodes: `Positional`, `PawnStructure`, `KingSafety`, `PieceActivity`, `SpaceDevelopment`, `MaterialBalance`, `PositionalChange`, `TiedToLine`, `NetChange`, `UseTest`, `FavouredSide`, `StaticScale`, `ImpactRating`; their `Pc*` and `Ut*` nodes. Resumed from `8b561c4`; runtime observations below use the current Release binary, schema 8. No build or test suite was run.

## Conclusion

The strongest defect is treating disappearance of a weakness as a gain even when its owner lost the weak piece.
Second, the use test can connect a shelter change to a check that does not use that shelter change.
Third, impact and static/dynamic labels describe a display policy, not measured importance or duration.
The proposed break cost also lacks a searchable domain for absolute pins and hypothetical captures.
Feature identities and root-to-end differencing remain sound bookkeeping, provided they are not promoted into benefit or causation.
EM4's ply blocks and explicit endpoint wording improve temporal clarity; they do not repair these semantic links.
Prior POS-1–5 are not restated: exchange terminology, persistent features, horizons and prevalence remain qualified limitations.

## Findings

### POS2-1 Losing a weak pawn is credited as helping its owner

**Definition — Source.** `Planning/DBC/DEFINITIONS.md:196`: “The side a positional change helps”; an appearing weakness helps the opponent, and “a disappearance helps the side the appearance would not”.

**What it claims vs what it computes.** It assigns polarity from tag and direction, without asking why the feature disappeared. Repairing an isolated pawn and losing it to capture receive the same beneficiary.

**Counterexample — Observed.** Python-chess validated the FEN and capture:

```text
build/release/chesswhy --fen "3r2k1/8/2n5/8/3P4/2N5/8/R5K1 b - - 0 1" --moves Rxd4 --full-report --json
```

Exit 0. The line begins `Rxd4 Nb5 Rg4+ Kf1 Rf4+ Kg1`; the six-ply consequence is Black material `+1`. At ply 1, White's d4 `isolated_pawn` and `passed_pawn` both disappear with `net_change: true`, `in_use: true`. **Source:** `src/positional/positional.cpp:926–941` assigns White as beneficiary of the former and Black of the latter. These assignments are not serialized fields; their derivation is source evidence, not invented CLI output. White has lost the pawn, not repaired it.

**Root cause.** Complement inheritance and wrong temporal accounting (families 3, 2): “weakness gone” becomes “owner helped”, ignoring the material removed to eliminate it.

**Inherited by.** `FavouredSide`, `UseTest`, and initiative conversion, which accepts any qualifying favoured change. A falsely converted White run is a downstream **Inference**, not observed in this probe.

**Candidate fixes.** (1) Record whether disappearance came from capture, repair, movement or identity replacement; do not count a captured weakness as its owner's gain. Board-replay cost only; promotion and exchanges need explicit treatment. (2) Rename the present value “nominal feature polarity” and exclude it alone from conversion evidence. No search cost; fewer conversion claims.

**Severity:** `wrong claim` — the beneficiary definition can turn loss of the weak asset into a gain claim.

### POS2-2 A check anywhere certifies use of a particular shelter change

**Definition — Source.** `DEFINITIONS.md:190`: “a later move of the walked line uses it”; line 195 defines king pressed by any subsequent enemy check, capture in the king zone, or two endpoint zone attackers. `PostM4_EM3.md:257–272` uses that predicate for king-file changes while castling rights remain.

**What it claims vs what it computes.** Temporal coexistence of a changed shelter and a check is not a relation between them. The test never connects the checker, its route or attacked squares to the changed file.

**Counterexample — Observed board check; Source classification.** Python-chess validated:

```text
FEN: 6k1/8/8/8/1n6/8/4BP2/4K2R w K - 0 1
Line: f4 Nd3+ Bxd3 Kf7 Bc4+ Ke7
```

The f-file shield gap appears at ply 1 and remains at ply 6; White retains kingside castling rights. The contract therefore passes it because of `Nd3+`. But that knight checks e1 independently of f2: python-chess also verified legal `Rh2 Nd3+`, with the f2 pawn untouched. Black's knight is captured on the next ply. This is a supplied legal-line counterexample, **not** an observed Stockfish PV; CLI `--moves` cannot prescribe an entire continuation.

**Root cause.** Proxy versus claim and composition (families 1, 5): a broad pressure event is reused as evidence that this specific feature was exploited.

**Inherited by.** `UtPressed`, `UtKeep`, `TiedToLine`, and conversion's “net change in use” premise. The explicit ply-1/2 and always-pass exceptions similarly establish selection, not causal use.

**Candidate fixes.** (1) Call the filter “selected for explanation” and retain separate reason codes, including `pressure_cooccurs`; negligible cost, no exploitation claim. (2) Require a geometric witness relating the check/capture route to the changed shelter squares. Board-rule cost; handles direct lines but misses indirect exploitation, which would need bounded comparisons.

**Severity:** `misleading` — the recorded change is real; its alleged use is stronger than the evidence.

### POS2-3 Impact and duration are inferred from a tag taxonomy

**Definition — Source.** `DEFINITIONS.md:185`: “the rank … of the consequence the fact bears on”; line 189: “A static fact lasts until the structure changes” and “a dynamic fact fades with every tempo unless it is used”.

**What it claims vs what it computes.** EM3 §6.7 assigns every positional group rank zero and a fixed dynamic/static class. It measures neither per-fact consequence participation nor persistence. A display priority is legitimate; these stronger explanations of it are not established.

**Counterexample.** **Observed board checks:** from `7k/R7/8/8/8/8/8/7K w - - 0 1`, legal `Ra6` removes the “static” seventh-rank feature without changing pawns or material. From the standard initial FEN, legal `Nf3 Nf6 Ng1 Ng8` preserves all “dynamic” castling rights without exercising them. **Source plus POS2-1's observed report:** the disappearance of White's one-pawn material lead directly records Black's material consequence, yet its positional group receives rank zero (`src/report/move_report.cpp:700–711`).

**Root cause.** Proxy versus claim (family 1): tag membership substitutes for duration and consequence linkage.

**Inherited by.** `StaticScale`, `ImpactRating`, fact-sheet priority and within-ply ordering. EM4 correctly narrows visible ordering to each ply block; no incorrect global text order is alleged.

**Candidate fixes.** Describe the existing classes and static scale as display-policy categories, with no duration guarantee; zero runtime cost. If per-fact impact is required, attach an explicit consequence witness rather than inheriting a group rank; additional replay/linking work, potentially searches for indirect effects.

**Severity:** `naming` — deterministic ordering is useful, but not a measurement of impact or lifetime.

### POS2-4 Break cost takes a maximum over unsearchable moves

**Definition — Source.** `DEFINITIONS.md:187`: “the best score over the moves that are not restricted moves minus the best score over the restricted moves”. This remains an open proposal, not shipped output.

**What it claims vs what it computes.** Restricted moves are not necessarily legal alternatives. Absolute pins deliberately include illegal moves; pawn restrictions can contain hypothetical captures. An ordinary search cannot score these as moves from the stated board.

**Counterexample — Observed.** Python-chess validated this position and Kf2:

```text
build/release/chesswhy --fen "k3r3/8/8/8/8/8/4N3/4K3 w - - 0 1" --moves Kf2 --full-report --json
```

Exit 0. The root absolute pin lists `e2c1,e2c3,e2d4,e2f4,e2g1,e2g3` as restricted and no free moves. Python-chess confirms all six are pseudo-legal but none legal. Thus the proposed restricted-search maximum has an empty legal domain.

**Root cause.** Domain mismatch (families 4, 6), distinct from MOT2-1/2's defective restriction predicates: even a perfectly correct absolute pin cannot supply legal breaking moves.

**Inherited by.** Future break-cost scores and any ranking based on them.

**Candidate fixes.** Define cost only over legal alternatives, from the held side's score perspective, and return an explicit non-applicable reason for empty sets; two restricted-root searches where applicable. Treat hypothetical captures as separately labelled counterfactual boards, never the same cost; extra searches and explicit changed-position premises.

**Severity:** `misleading` — the open proposal promises a uniform numerical quantity where its input domain does not support one.

## Sound definitions

- **Feature, owner, identity, positional change:** explicit board snapshots and identity differences; owner is not beneficiary.
- **Net change, passing change, holds-at-end:** coherent endpoint bookkeeping; “passing” operationally includes every non-net event, not necessarily an entire feature lifetime.
- **Pawn tags:** deterministic structural predicates; tripled also doubled is intentional overlap, not an exclusive classification error.
- **King files, relative rank, shelter and zone tags:** explicit geometry, not a king-safety evaluation; castling tags mean rights, not immediate legal castling.
- **Bishop pair, rook files/seventh, outpost, bad bishop:** sound bounded predicates; no guarantee that the piece survives or is valuable. Outpost's pawn-capture exclusion is now explicit.
- **Mobility, space, undeveloped, thresholds/leads:** reproducible specified counts, not legal-move counts, development history or calibrated advantage.
- **Material ahead:** sound nominal balance. Exchange-up's broader shape remains the disclosed prior POS-1 limitation.
- **Superseded and use-test exclusions:** coherent selection rules, not proofs of redundancy or irrelevance; mate/castling suppression deliberately prioritizes other explanation facts.
- **Core-first impact policy and full-report retention:** preserve the main judgement and omitted evidence; no new defect demonstrated in those safeguards.
