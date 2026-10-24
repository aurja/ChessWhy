# 04 — Tactics, opportunities and contribution

Date: 2026-10-24. Baseline: `20d259c371b75f07e4779f700600b6b9fe8bc568`.

Concept nodes: `Tactics`, `JudgedLine`, `BestAlternative`, `OnlyMoveMargin`, `OpportunityJudgement`, `TacticTest`, `TacticLine`, `TacticSequence`, `Contribution`, `TacticLabel`; all `Tj*` and `Co*` nodes. Resumed from `8b561c4fcfe58a8226910234b40413b9880f62e4`; runtime evidence below uses the current Release binary. No build or test suite was run.

## Conclusion

M4's separation of material/mate tactics from evaluation-margin opportunities resolves the old margin-as-mechanism problem.
The strongest new defect is temporal: rule 3′ preserves a pin's identity when its pieces stay still, although a third piece can remove the pin.
Its denied-answer test can consequently credit a restriction that no longer exists.
This is demonstrated on a legal supplied line and the written rule, not claimed as an observed engine PV.
A second, narrower defect is the unretired tactic-recognition definition, which still tests the criterion renamed opportunity.
EM2's target continuity fix closes prior TAC-5's piece-substitution example; its denied-answer experiment also fixes the old back-rank examples.
EM4's repeat merge and ply-local presentation do not change either classification predicate.
Inherited material-horizon and restricted-move defects are cross-referenced below, not duplicated.

## Findings

### TAC2-1 Unmoved pieces do not imply a surviving pin

**Definition — Source.** `Planning/DBC/DEFINITIONS.md:283`: indirect applies “before any motif piece has moved or been captured” when “one of the motif's denied moves would answer that move”. Line 284 calls a denied move “A move the motif takes away from the defender”. `PostM3_EM2.md:103` defines an absolute pin's denied moves as the held piece's pseudo-legal moves whose destination is off the actor–king segment; lines 167–178 apply that test on later boards while the motif pieces remain untouched.

**What it claims vs what it computes.** It claims that the motif removes an answer. The continuity guard establishes only that the participants have not moved. A different piece can interpose between pinner and held piece, making the previously forbidden move legal. The old segment still supplies the denied-move list.

**Counterexample — Observed board check; Source rule result.** Python-chess validated:

```text
FEN: 4k3/1r2nb2/P7/8/8/8/8/1B2R1K1 w - - 0 1
Line: axb7 Be6 Bf5 Nxf5
UCI:  a6b7 f7e6 b1f5 e7f5
```

At the root Re1 absolutely pins Ne7 to Ke8. All four moves are legal. After `Be6`, Ne7 is no longer pinned; after `Bf5`, Black is not checked and `Nxf5` is legal. White's material change is +5 after `axb7` and +2 after `Nxf5`. Re1, Ne7 and Ke8 remain untouched through ply 3.

Under the written rule, the root pin qualifies as indirect at ply 3: e7–f5 is pseudo-legal, leaves the old e-file segment and captures the piece that just moved. `src/tactics/tactic.cpp:613–619,642–657,776–782` implement precisely these tests without re-establishing the pin. That is a **Source** derivation, not an observed C++ contribution result. The legal line demonstrates the invalid inference even though Black's purportedly denied answer is then actually played.

The CLI cannot force a complete PV. A probe of this FEN with `--moves axb7 --full-report --json` at the original baseline selected a different continuation and did not demonstrate the false contribution. No claim is made that Stockfish chooses the supplied line.

**Root cause.** Wrong board/time and composition (families 2, 5): piece continuity substitutes for relation continuity. A correct off-ray test for an existing pin is applied after the pin has disappeared.

**Inherited by.** `CoDenied`, `CoIndirect`, the indirect contribution, root-motif relevance filtering and any statement that the move uses that pin. EM4 merging preserves the contribution value; changing presentation cannot repair its premise.

**Candidate fixes.** (1) Before testing an old motif on each later board, revalidate its relation: for an absolute pin, require the same pinner–held-piece–king ray with no new blocker. Small board-rule cost, no searches; other motifs need their own validity predicates. (2) Require each absolute-pin denied reply actually to expose the king to the named pinner, rather than merely leave its historical segment. Small per-reply board cost; preserve the documented hypothetical-pawn and counterfactual check-answer handling.

**Severity:** `wrong claim` — the rule can say a motif removes a reply that is legal on the tested board. Runtime incidence in engine-selected lines remains unmeasured.

### TAC2-2 Tactic recognition still specifies the retired opportunity test

**Definition — Source.** `Planning/DBC/DEFINITIONS.md:282`: “Does the line gain against the best alternative, and do we label it a tactic exactly when it does? Mechanical from the engine, so exact by construction”. In contrast, line 267 and `CONCEPTS.md:366` define the current tactic test through mate or net material; line 268 assigns the margin test to opportunity.

**What it claims vs what it computes.** This recognition definition still equates tactic recognition with superiority over alternatives. The current report deliberately permits a tactic with no superiority at all. Unlike the historical explanation in the tactic row, this row is not marked retired.

**Counterexample — Observed.** Python-chess validated the FEN and both legal mates:

```text
build/release/chesswhy --fen "6k1/5ppp/8/8/8/8/8/R3R1K1 w - - 0 1" --moves 'Ra8#,Re8#' --full-report --json
```

Exit 0. Both moves have `is_tactic: true`, consequence `mate`, mate distance 1. Ra8# has Re8# as its alternative, margin `0.0`, and `is_opportunity: false`. Re8# has no opportunity judgement, as intended for a non-first candidate. This is correct M4 behaviour and a direct counterexample to the still-current recognition sentence, not a request to reinstate the old threshold.

**Root cause.** Composition/naming (families 5, 1): the conceptual rename changed the classifier but left its recognition/validation claim attached to the old meaning.

**Inherited by.** Documentation and any acceptance argument using “exact tactic recognition” to mean margin arithmetic. No incorrect runtime tactic verdict is alleged.

**Candidate fixes.** Rename this row **opportunity recognition** and define tactic recognition separately, including the explicit refutation baseline exception in M4 R1. Documentation-only cost; tests of arithmetic must still be distinguished from evidence that a bounded line represents a settled chess outcome.

**Severity:** `naming`.

## Sound definitions

- **Motif versus tactic versus tactic label:** presence, bounded line result and multi-ply relation are useful separate concepts; a motif alone proves no gain.
- **Tactic / `TacticTest`:** coherent as the approved material/mate classifier, with position-only recapture and refutation baselines stated explicitly. Terminal draws and unsettled exchanges are inherited issues in [THR2-1/2](01-threats.md), not additional findings here.
- **Opportunity / only-move margin / best alternative:** precise operational comparison, not proof of a unique chess mechanism. Prior TAC-4's separate-search qualification remains applicable; no new numerical failure was demonstrated.
- **Judged line / opportunity judgement / forced / `Tj*`:** scope and three-way dispatch are explicit; one legal move has no comparison. Saturation, two winners and width dependence remain accepted limits.
- **Tactic line / tactic sequence / root motifs:** explicit union, bounded walk and per-move attribution; opportunity-only lines intentionally receive contributions too.
- **Direct contribution / side gate:** sound as the stated involvement convention, not causal necessity. **Indirect / none:** EM2's denied-answer test improves the old back-rank examples, but the stronger general independence gloss is not established; TAC2-1 is a new temporal failure, not a repeat of TAC-1.
- **Answer / denied move:** explicit check-evasion versus recapture predicate, not an exhaustive definition of chess defence. Restricted-move cost and legality inputs additionally inherit [MOT2-1/2](03-motifs.md).
- **Held piece / move set / hypothetical capture / free moves / held-to / existence condition:** coherent motif-local bookkeeping; hypothetical captures are counterfactual, and free does not mean globally safe.
- **`removal_of_defender`:** its SEE and later-capture conditions are explicit; target continuity repairs prior TAC-5. It remains a bounded exchange witness, not a necessity proof.
- **Motif recognition / retired judgement and tag terminology / milestone entries:** operational or historical definitions, not independent chess-strength claims. EM4 repeats preserve distinct restrictions and contribution values rather than establishing equivalence of unlike motifs.
