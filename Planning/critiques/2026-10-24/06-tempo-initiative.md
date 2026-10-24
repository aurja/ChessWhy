# 06 — Tempo and initiative

Date: 2026-10-24. Baseline: `b86bbf08683ff99b9141217393d92c981ae629a3`.

Concept nodes: `Tempo`, `TempoValue`, `TempoLabel`, `ThreatTest`, `ReplyTest`, `PlyLabel`, `TempoGained`, `TempoLedger`, `InitiativeRun`, `RunEnding`, `RunOutcome`; L3 `PlFirst`, `PlStops`, `PlOwn`, `PlCounter`, `PlThreatening`, `PlNeither`, `PlCounterThreat`, `PlPure`.

## Conclusion

The strongest independent concern is calling an ongoing checking attack “converted”: the outcome predicate accepts the initiative's own dynamic symptoms as its proceeds.
Second, a low expected-score cost of passing does not establish a strategically non-tempo position; saturation hides forcing play.
Third, A2's universal statement about even trades contradicts the explicit check rule.
The Captain's pure-reply decision stands; part 2 must also complete the label/run state space, including unresolved evidence.
The pass-value arithmetic, side perspectives, ledger counts and bounded run segmentation are coherent operational definitions.
Threat identity/causality is already covered by [00](00-pure-reply.md); provisional TMP-1 is not retained as an independent finding.
EM4's ply blocks improve location, not these meanings: a run's outcome still uses later evidence.

## Findings

All runtime examples below used `build/release/chesswhy --fen F --moves M --full-report --json`, at the default `standard` budget, sequentially. Each returned report v8. Python-chess checked every cited root and replayed its complete reported PV legally. No builds or test suites ran.

### TMP-2 “Converted” can mean the checking attack is still happening

**Definition — Source.** `Planning/DBC/DEFINITIONS.md:180`: “a net change in use made at a ply of the run has the holder as favoured side; **evaporated** otherwise.” The alternative material test compares before the run with “the end of the tempo ledger”. `PostM4_EM3.md:218–221` specifies that disjunction.

**What it claims vs what it computes.** Conversion suggests initiative produced a retained benefit. The test accepts any favourable used feature, including an attacker continuing to check. Conversely, failure to find one is named evaporation even when the run is unfinished. Material is measured through the whole walk, not just the run, so the rule also cannot attribute later gains to that run.

**Counterexample — Observed.** FEN `3r2k1/8/2n5/8/3P4/2N5/8/R5K1 b - - 0 1`, move `Rxd4`. PV: `Rxd4 Nb5 Rg4+ Kf1 Rf4+ Kg1 Rf6 Ra6 Kg7 Nc7`. The ledger stops at ply 6 and reports Black's run, plies 3–6, two tempi, `ending: unfinished`, `outcome: converted`. Black's pawn recovery occurred at ply 1; material balance is unchanged throughout the run. The sole `net_change && in_use` change inside it is White's `king_zone_attacker`, Black's rook on f4, appearing at ply 5. This is the checking attack itself, not separately demonstrated conversion. The displayed next Black move retreats to f6.

**Root cause.** Proxy and composition (families 1, 2, 5): “some favourable feature remains at cutoff” substitutes for conversion; “no such feature” substitutes for evaporation. Feature polarity problems are separate, covered by the positional critique.

**Inherited by.** `RunOutcome`, initiative summaries; EM4 prints this future-dependent conclusion at the run's starting ply.

**Candidate fixes.** (1) Rename outcomes “observed benefit at cutoff” / “none recorded”, retaining the witness and endpoint; negligible rule/report cost, no causal claim. (2) Separate continuing dynamic pressure from conversion evidence, and keep unfinished/unsettled outcomes unknown; board/PV processing only initially, conservative and potentially less complete. Attribute material to a declared run endpoint rather than silently including later play.

**Severity:** `misleading`.

### TMP-3 Zero pass cost does not mean non-forcing chess

**Definition — Source.** `DEFINITIONS.md:182`: “Below 0.05 it is a non-tempo position, where time matters little”; above 0.10, “every tempo counts and threats decide”. Lines 181, 183 define the ES difference and exact interval.

**What it claims vs what it computes.** The number measures estimated result sensitivity to one hypothetical pass for one side. It does not measure forcingness, all future tempi, or whether dynamic facts matter. At ES saturation, very different tactical continuations have the same result score.

**Counterexample — Observed.** FEN `8/8/8/8/8/6k1/4q3/6K1 w - - 0 1`, move `Kh1`. White has exactly one legal move. The report gives `Kh1 Qd1#`, root `moves: 0`, `passes: 0`, `value: 0`, `label: non_tempo`; after Kh1, Black likewise has `moves: 1`, `passes: 1`, value zero. Python-chess also confirms `Qd1#` after White's hypothetical pass. Immediate mating play and mandatory king movement coexist with the “time matters little” classification. The number itself is correct.

**Root cause.** Proxy and perspective (families 1, 6): a result-scale, side-specific counterfactual becomes a whole-position strategic type. Null root tempo value also becomes false rather than unknown (`PostM4_EM3.md:171–172`), including check positions.

**Inherited by.** `TempoLabel`, dynamic/static impact ordering and the use test for leads; not the separately computed tempo ledger.

**Candidate fixes.** (1) Name the labels high/low **estimated pass cost**; no extra search. (2) Keep tactical forcingness and missing-value state orthogonal, using existing check/mate/tactic facts; cheap, but requires explicit ordering rules rather than one strategic boolean.

**Severity:** `misleading`.

### TMP-4 EM4 needs non-pure and unresolved branches

**Definition — Source.** `DEFINITIONS.md:174`: an answer stopping a threat “is a pure reply, or a counter-threat when it is itself a threatening move”. Line 179 lists only `spent`, `runs`, `contested`, `unfinished`. The replacement purity requirement is at line 169.

**What it claims vs what it computes.** Today's categories are exhaustive only because every stopping, nonthreatening answer is deemed pure. The decided replacement removes that equivalence. Neither productive defence, optional defence, nor unresolved forcedness/productivity implies a counter-threat or an unstopped threat.

**Counterexample — Observed; consequence Inference.** Re-running [00](00-pure-reply.md)'s FEN `1br3k1/5pp1/p3p2p/1bBn4/N7/1P3PP1/5R1P/2R3K1 b - - 0 1`, move `Bxa4`, reproduces `Bxa4 bxa4 Ba7 Rfc2 Nb4 Bxa7 Nxc2`: ply 7 has `stops_threat: true`, `threatening: false`, `pure_reply`. Rejecting its purity removes the present run starting at ply 6. If this response instead follows an already-established run, none of the existing ending predicates describes it. This is the integration gap identified in 00, not a new diagnosis of Nxc2.

**Root cause.** Inherited complement and non-exhaustiveness (families 3, 4). The new definition adds independent predicates without supplying their state transitions.

**Inherited by.** `PlPure`, `PlNeither`, `TempoGained`, run continuation/endings, counts and outcome evidence.

**Candidate fixes.** Keep stopping, counter-threatening, forcedness and retained benefit separate; add a stopping-but-non-pure label/ending and an unresolved state. These transitions are cheap once part 2's searches exist. Specify whether “forced” quantifies over legal or acceptable alternatives, the acceptance tolerance, gain baseline and settlement criterion. Unknown must award no established tempo, without falsely asserting that the opponent gained something. Do not broaden counter-threat to include realised gains.

**Severity:** `misleading` — a required part-2 contract completion, not a claim that the pending implementation already fails.

### TMP-5 The even-trade assertion conflicts with the check rule

**Definition — Source.** `DEFINITIONS.md:168`: “a check is a threatening move by rule” and “so an even trade is not a threatening move”.

**What it claims vs what it computes.** Ignoring material already captured does not exclude checking trades, or trades with independent follow-up threats.

**Counterexample — Observed.** FEN `8/8/2p1k3/3b4/2B5/8/8/R5K1 w - - 0 1`, move `Bxd5+`. PV begins `Bxd5+ cxd5 Kf2`. The two bishops are exchanged, restoring the initial material balance. Ply 1 nevertheless has `threatening: true`, `by_check: true`. This follows the check rule correctly; purity need not be debated to demonstrate the contradiction.

**Root cause.** Overgeneralisation (family 1): “capture proceeds do not count as a future threat” becomes “an even trade cannot threaten”.

**Inherited by.** A2 examples and explanations of threatening moves.

**Candidate fixes.** Replace the conclusion with “the captured material alone does not make the move threatening; checks and independent follow-up threats still count”. Documentation-only cost; preserves the computed rule.

**Severity:** `naming`.

## Sound definitions

- **Tempo senses:** one move, forcing-response count and pass value are usefully separated; development tempo is explicitly not computed. The count is not a literal extra legal move.
- **Threatening move / threat search:** clear conditional, forward-looking test; check exception is exact. It establishes neither novelty nor profitability of the move itself.
- **Reply search:** well-defined unrestricted continuation test, not identification of the original threat; see 00, rather than a duplicate TMP-1.
- **Tempo gained / counter-threat:** coherent predicates conditional on correct stopping and purity evidence; “returns the tempo” is not a numerical conservation law.
- **Ledger / ply labels / skip reasons:** explicit walked scope, first-ply convention and counts; current exhaustive dispatch needs TMP-4's migration.
- **Initiative / run / endings:** precise contiguous forcing-run model, not all strategic initiative; `unfinished` correctly marks horizon uncertainty. `runs` does not by itself prove sound execution of a tactic.
- **Tempo value:** root and after-move perspectives correctly complement ES; no pass in check. Different-budget estimates are not exact game-theoretic values.
- **Tempo-dependent / non-tempo / zugzwang intervals:** disjoint with an explicit neutral gap. Negative estimated pass cost operationally supports “engine prefers passing”, not certified zugzwang from an arbitrarily small difference; spare-tempo resources are not separately detected.
