# 02 — Inputs, searches and verdicts

Date: 2026-10-24. Baseline: `c601a6b769eb5fba3a92b6f009ae99b88b2832b8`.

Concept nodes: L2 `Question`, `Searches`, `Verdicts`; L3 `Verdict` and all `Vd*` nodes. Scope includes DEFINITIONS §1, §3.1–§3.2 and §3.5; threat, parry and consequence semantics are cross-referenced to [01-threats.md](01-threats.md). Investigation began at `8b561c4fcfe58a8226910234b40413b9880f62e4`; the runtime comparison below was exercised again on the resumed baseline, using the existing Release binary (schema v8), without rebuilding.

## Conclusion

The strongest new defect is the meaning of **okay**: it includes demonstrably optimal moves with exactly the best score, solely because search C supplied them.
This is not score instability, WDL saturation or insufficient search: the counterexample consists entirely of immediate checkmates.
Changing the requested output from seven named moves to `top7` changes two grades without changing any score or chess outcome.
The verdict partition is exhaustive computationally, but its “scored below the best” interpretation is false.
Inputs, selection unions, search roots and explicit search-scope disclosures otherwise have coherent operational definitions.
The earlier audit's ordering, history and reproducibility limitations now have explicit qualifications; they are not new findings here.
`allows_tactic` remains an outcome detector, not a causal comparison. I drop the provisional separate finding about unavoidable losses: no independent false causal statement was demonstrated, and the concrete horizon defect is covered by THR2-2.
No threshold change or revision of the Captain's pure-reply decision is proposed.

## Findings

### VER2-1 An exact best-score tie becomes “okay” because it came from C

**Definition — Source.** `Planning/DBC/DEFINITIONS.md:156`: “best = the engine's top line, and every other A line with exactly the same engine score”; “okay = any other playable move”; “An asked move outside A is at most okay”. `CONCEPTS.md:865–866`: `VdBest` means “The engine's best move, or tied with it exactly”; `VdOkay` means “Playable but scored below the best”.

**What it claims vs what it computes.** The latter promises a quality distinction. The actual predicate distinguishes membership of A as well as score. The complement of “A member with the top score” includes equal-scoring outsiders; it does not mean “scored below”. M4 explicitly requires this exclusion, so implementation conformity does not cure the conceptual contradiction.

**Counterexample — Observed.** Python-chess checked the following FEN as valid and all seven named moves as legal immediate checkmates:

```text
7k/4Q3/4B1K1/8/4B3/8/8/8 w - - 0 1
```

Both commands exited 0:

```text
build/release/chesswhy --fen "7k/4Q3/4B1K1/8/4B3/8/8/8 w - - 0 1" --moves Qf8,Qe8,Qd8,Qh7,Qg7,Qf6,Qh4 --budget quick --json
build/release/chesswhy --fen "7k/4Q3/4B1K1/8/4B3/8/8/8 w - - 0 1" --moves top7 --budget quick --json
```

| Moves | Seven named moves, K=5 | `top7`, K=7 |
|---|---|---|
| Qh4#, Qf8#, Qh7#, Qf6#, Qg7# | best; A ranks 1–5 | best; A ranks 1–5 |
| Qe8# | okay; rank null, from C | best; A rank 7 |
| Qd8# | okay; rank null, from C | best; A rank 6 |

Every row in both outputs has `mate: 1`, ES `1.0`, loss `0.0`. No move can improve on an immediate legal checkmate. The failure is the grade's advertised meaning, not an assertion that the engine missed a better continuation.

**Root cause.** Proxy versus claim and inherited exclusions (families 1, 3): membership in the preferred search is substituted for comparative move quality, then its complement is described as inferiority. The increased K also increases A's budget, but unchanged mate-one scores isolate the classification rule rather than a search-strength effect.

**Inherited by.** `VdOkay`, `VdBest`, move reports and later prose explaining why an asked move is merely okay. Playable membership remains correct; both labels are in that bucket. The `best` selection keyword deliberately means only line 1 and need not become an all-ties selector.

**Candidate fixes.**

1. Apply the same-score predicate to C results too. No additional search or material allocation is necessary. Cross-search centipawn equality remains approximate evidence, but that limitation also affects A's mixed-depth lines; keep search provenance separate from the verdict.
2. If exclusion of C is intentional, replace “scored below the best” with “playable; not certified as an A top-score tie”, and expose actual equality independently. No search cost; preserves the contract but admits that **okay** is not a strict quality rank. A dedicated “best status unconfirmed” field is clearer than implying inferiority.

**Severity:** `wrong claim` — `VdOkay` explicitly asserts a lower score where the report itself supplies the identical score and an independently checkable optimal outcome.

## Sound definitions

These are sound at the stated operational scope, not certificates of perfect play or complete chess explanation. Closely related historical aliases are listed together; no new finding is claimed for previously documented limitations.

- **Target:** explicitly separates a motif's object from a test's numerical target.
- **Precision:** historical meanings are distinguished; bare-word ambiguity is deliberately avoided.
- **Line:** a move sequence from a stated root, not a forced variation.
- **Threat:** explicitly conditional on having the move; stronger prevention/novelty claims belong to the threat audit.
- **Gain:** SEE, consequence material and opportunity margin are separate quantities, not interchangeable units.
- **Forced:** exactly one legal move, distinct from one acceptable move.
- **Fine / fine set / `fine_truncated`:** historical terms are retired, with their former search scope disclosed.
- **Hanging:** the stipulated SEE rule, explicitly not synonymous with undefended.
- **Side:** S/O and motif X/Y distinguish different perspectives.
- **Frozen / gate:** governance and acceptance terms, not evidence of chess truth.
- **Level:** audience and diagram senses are distinguished; audience wording does not alter analysis.
- **Tempo:** its senses are distinguished; the pure-reply issue is already assigned and decided elsewhere.
- **Report / fact / facts first:** specify storage and ordering of computation before verbalisation, not independent validation of every predicate.
- **Provenance:** identifies evidence type; verdicts additionally depend on ChessWhy's cut-offs and, since M4, its tactic rule. “Engine” must not be read as an engine-authored label.
- **Budget / NodeBudget:** deterministic node allowances, not guaranteed equal depth or exactly exhausted work; an engine can finish early.
- **Reproducible:** same-configuration scope is explicit; it does not promise portable binary identity.
- **Rerun identity:** an exact two-run equality check, not a proof for every future environment.
- **Asked move / AskedMove:** names a legal move and specifies reuse from A versus search C.
- **Candidates / CandidateMove:** engine-slot order and reported depth are explicit; no independently sorted ranking is promised.
- **Playable move / PlayableSet:** the declared ES-loss threshold and A-scoped list are precise; “worth playing” remains relative to that metric.
- **`--moves` / MoveRequest:** union and deduplication are unambiguous; `best` intentionally selects only line 1.
- **`--playable`:** explicitly retired before implementation, not a second active input convention.
- **Whole-board keyword:** establishes coverage of every legal root move, not equal search quality per move.
- **Selection:** states membership and ordering independently of the quality interpretation challenged above.
- **Detailed line:** identifies eligibility for auxiliary searches, not unconditional execution of skipped/inapplicable searches.
- **Move report / relevance filter:** deterministic projection contracts; relevance and net-change chess claims require their separate audits.
- **Move report document / full report:** distinguish selected projected facts from the full computed record.
- **`--full-report` / JSON output / full report text:** content and serialization choices are orthogonal.
- **Projection check:** verifies preservation under the specified filter, not the truth of source facts.
- **Line depth:** engine-reported depth, explicitly possibly different across MultiPV slots.
- **`legal_moves`:** board-rule count makes search coverage visible.
- **Command line / native apps:** current input interface versus planned presentation layer.
- **Analysed position / FEN / PGN and ply:** a declared position-only scope, not a history-aware repetition judgement.
- **Game mainline:** earlier positions are retained for opening naming, not secretly treated as search history.
- **Input error:** unusable requests fail without a partial report.
- **Report schema / semantic check:** structural and relational validation, not an independent chess oracle.
- **A / C / StockfishProcess:** root search, restricted asked-move search and UCI transport have explicit roles.
- **B / D / E:** their pass and forced-first-reply premises are explicit; these searches alone prove neither real-world necessity nor novelty.
- **R:** excluding the refutation's first move measures an alternative at the same child root. The glossary's old “not fine/tactic” wording should follow the current concept node's “not playable/opportunity”; this is terminology drift, not a new search defect.
- **ExpectedScore / loss:** perspective-aware WDL arithmetic and non-negative clipping are operationally defined; clipping does not prove A was globally optimal.
- **VdPlayable / VdSmall / inaccuracy:** disjoint, inclusive boundaries implement the chosen policy.
- **AllowsTactic / VdAllows:** detects net loss in the walked prefix or a negative mate score; not whether that loss was newly avoidable. THR2-2 demonstrates the inherited endpoint limitation.
- **VdBlunder / VdMistake:** exhaustive branches above 0.10 under that detector; not a second scalar ordering by loss magnitude.
- **`playable_truncated`:** a one-way warning condition, not a completeness guarantee when false.
