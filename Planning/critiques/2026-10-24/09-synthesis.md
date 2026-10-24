# 09 — Cross-slice synthesis

Date: 2026-10-25. Baseline: critiques 00–08 at their recorded baselines, plus the current definitions, concept map and vision. No new runtime evidence.

## Conclusion

The shared failure is promoting a local measurement, bounded prefix or selection rule into a stronger chess claim; downstream agreement preserves the mismatch (THR2-2, MOT2-2, REP2-2, PRF2-1). EM4 part 2 should repair those contracts, not ask prose to compensate.

Below, **D** = [DEFINITIONS](../../DBC/DEFINITIONS.md), **C** = [CONCEPTS](../../DBC/CONCEPTS.md), **V** = [PROJECT_VISION](../../PROJECT_VISION.md). IDs refer to adjacent critiques; [00](00-pure-reply.md) has no finding ID. Grouping, proposed repair ownership and priorities are **[INFERENCE]**, not additional observed failures.

## 1. Shared words, different claims

| Term | Senses, disagreement and §1 coverage |
|---|---|
| **Threat** | D §3.3: a side's conditional engine line, including positional outcomes. D §3.6: a *threatening move* requires rank ≥ 2, or check. D §3.1 `relevance filter`: a reported created threat requires mate/material/promotion. D §4.3 `threat`: a tactical-pin `{kind, square}` witness after hypothetical removal, not a PV. **[INFERENCE]** A rank-1 promotion meets the report filter but not the tempo threshold without check; a motif witness need not establish any engine-ranked threat. §1 explains only the engine-line sense. |
| **Forced** | D §1/§4.5: exactly one legal move; 00 and D §3.6 `pure reply`: necessity of meeting an identified threat. **[INFERENCE]** Several legal replies can exist while only threat-meeting replies are acceptable. TMP-4 requires the acceptance domain/tolerance to be settled. §1 distinguishes legal uniqueness from “only good”, but does not define this new response-level necessity. |
| **Gain** | D §1 distinguishes SEE pawns, consequence material and ES margin, but still calls the margin a *tactic's* gain; D §4.5 now assigns it to opportunity. D §3.6 additionally needs retained independent benefit, not merely a future threat or nominal favourable feature. TAC2-2, 00 and POS2-1/TMP-2 are the disagreement cases. §1 is helpful but stale and incomplete. |
| **Use / relevance** | D §4.5 contribution concerns involvement or a denied answer; D §3.6 `use test` also admits early changes and always-pass tags; D §3.1 projection calls retained facts relevant. These are not interchangeable exploitation claims (TAC2-1, POS2-2), nor completeness guarantees (REP2-2). §1 has no disambiguation. |
| **Tempo** | D §1 separates one move, tempi gained and pass value; D §3.6 additionally turns pass value into a strategic position label. TMP-3 supplies the disagreement between that label and forcingness. §1 already separates the quantities, but does not justify the strategic inference; development tempo is explicitly uncomputed. |

## 2. Shared roots and downstream spread

Each repair owner below is a **semantic contract**, not a promise that one code edit repairs every consumer.

| Root group / findings | Nodes touched → single repair owner |
|---|---|
| **G1 — Prefix mistaken for outcome:** THR2-1, THR2-2 | `Consequence`, `WalkedPlies`, `ConsequenceRank` → `TacticTest`, `AllowsTactic`, `Urgency`, `ReplyTest`, `RunOutcome`, `Refutation`. Own the fix in **Consequence replay**: terminal outcome and endpoint completeness, retaining the material measurement. |
| **G2 — Negation mistaken for pure defence:** 00, TMP-4 | `ReplyTest`, `PlyLabel`, `TempoGained`, `TempoLedger`, `InitiativeRun`, `RunEnding` → **reply-evidence/state-transition contract**. Separate stopping, forcedness, counter-threat, benefit and unknown. |
| **G3 — Feature evidence mistaken for benefit:** POS2-1, POS2-2, TMP-2 | `FavouredSide`, `UseTest`, `TiedToLine`, `RunOutcome` → **benefit-evidence boundary feeding RunOutcome**. Require disappearance cause, use witness and declared attribution endpoint; continuing pressure is not automatically conversion. |
| **G4 — Geometry/exposure mistaken for effective restriction:** MOT2-1/2/3, TAC2-1, POS2-4 | `StateMotif`, `StaticExchange`, `Restriction`, `Contribution`, proposed break cost → **Motifs' board-local witness contract**. Account for compensation, legal responses, effective guarding and relation survival; distinguish actual from hypothetical moves. |
| **G5 — Display policy mistaken for strategic measurement:** POS2-3, TMP-3 | `TempoLabel`, `StaticScale`, `ImpactRating`, `UseTest` → **impact-policy contract**, distinguishing pass cost, missing evidence and presentation categories from forcingness, duration and importance. |
| **G6 — Search membership mistaken for quality:** VER2-1 | `VdSameScore`, `VdBest`, `VdOkay`, `MoveReport` → **verdict contract**; separate provenance from the claimed score comparison. |
| **G7 — Renames/generalisation left inconsistent:** TAC2-2, TMP-5 | `TacticTest`, `OpportunityJudgement`, `ThreatTest` → **canonical glossary predicates**: tactic recognition is not opportunity recognition; capture proceeds alone do not establish threateningness. |
| **G8 — Positive selection loses mandatory context:** REP2-2 | `RelevanceFilter`, `MoveReport`, `ProjectionCheck`, `Conditions`, `FactSheet`, `FactCheck` → **RelevanceFilter's mandatory-condition set**, before rendering or model selection. |
| **G9 — Counting unit changes under composition:** REP2-1 | `GtCount`, `GtRow`, `GameCounts`, `GamesOracle`, `FactCheck` → **GameCounts' statistical contract**: first visits versus games containing the move. |
| **G10 — Evidence scope mistaken for adequacy:** PRF2-1, PRF2-2 | `Proof`, `Recall`, `PuzzleSample`, `ReferenceImplementation`, `PositionalReference` → **Proof's measurement contract**: output volume is not noise; comparison populations must name gate/build/tag. |

**Do not fix twice or contradict another fix.** G1 belongs upstream, not in separate tactic/verdict/ledger exceptions. 00 and TMP-4 are one migration. G3's polarity/use corrections do not alone establish TMP-2's conversion claim; renaming “use” as selection while retaining it as benefit evidence would conflict. G4 must not delete all illegal/hypothetical restrictions to make break cost searchable: POS2-4 requires a separate legal search domain. VER2-1's all-search tie proposal changes V §5.4's A-only rule; wording repair and classifier expansion are alternatives, not simultaneous requirements.

## 3. Missing concepts — later scope proposals

**[INFERENCE]** These are proposed explanatory relations absent from the node tables, not defects. V §§1–2 require a small position-specific tool, not a trainer; §12 excludes tablebases and our own evaluation function.

- **Plans and pawn-break purpose:** “prepare this break / improve this piece to reach that objective.” C L2 `PawnStructure`, `PieceActivity`, `PositionalChange` describe features and changes, not objectives or preparation. `NamedStructures` and `Imbalances` already exist as deferred nodes: do not propose them as missing. Touch V §5.6; §8 may supply general advice, not position-specific proof.
- **Prophylaxis, defensive resources and quiet preparation:** “deny the opponent's next plan / preserve this saving resource.” C `ParryTest` addresses B's immediate threat; `Contribution` tests narrow denied answers; `Counter-threat` is a glossary term, not a general defence inventory (D §§3.3, 3.6, 4.5). Propose a witnessed alternative/prevention relation under V §§5.2–5.5. Quiet moves already receive engine verdicts through `SearchA/C`; missing is their purpose, not input support or `UrQuiet`'s position classification.
- **Favourable simplification:** “exchange into an endgame whose advantage is easier to realise.” C `MaterialBalance`, `NetChange`, `RunOutcome` supply neither an endgame-transition judgement nor conversion difficulty. Touch V §§5.4–5.6; remain engine-backed and bounded, without tablebases (§12).

## 4. Vision versus concept-map drift

The vision wins until the Captain reconciles these contracts.

- **Outcome:** V §5.2 says captures settle; D §3.3 `walked plies` and C `WalkedPlies` only extend consecutive captures (THR2-2). C `CqIsPositional` says “only the evaluation changes”; V §5.2 explicitly says residual class, with no ES swing measured.
- **Defensive coverage:** V §§5.4, 7.2 require deferred-parry conditions; D §3.1 `relevance filter`/C `RelevanceFilter` retain only successful parries (REP2-2). D's same row drops urgency, while V §§5.3, 7.4 still promise positional-first quiet explanations and urgency coverage.
- **M4 vocabulary:** V §5.5 separates tactics from opportunities; D §1 `gain`, `forced`, §3.2 `R`, §3.3 `refutation`, and §4.5 `tactic recognition` retain obsolete tactic/fine wording (TAC2-2; critique 02, sound definitions).
- **Wrong side:** C `CreatedThreat` names no legal move for **O**; V §5.4 and D §3.3 `creates_threat` name **S after O passes**.
- **Printed restrictions:** V §5.5 promises restricted/free moves; C `RestrictedMovement` and D §4.5's same row reserve them for full-report text. The recorded compact-output decision needs explicit vision reconciliation, not silent rollback.
- **Coverage lag:** C L1 `Tempo`, L2 `ThreatSearch`/`ReplySearch` and D §3.6 have no corresponding tempo specification in V §§5.1, 11. This is documentary coverage drift, not evidence that tempo is forbidden.

## 5. Priority

1. **EM4 part 2, first:** G2's decided “forced and gains nothing” plus TMP-4; integrate G1 and G3 so the replacement does not inherit unsettled or nominal benefit evidence.
2. **Same part, correctness:** G4's existing predicates and G8's lost conditions. Specify POS2-4's domain, but do not build the open break-cost proposal.
3. **Same part, contract repair:** G5–G7, G9–G10 and §4 drift; Captain approval before changing the frozen verdict rule or counting population.
4. **Later scope:** §3's plans/breaks, preventive/quiet-purpose relations and endgame simplification. Do not make them EM4 acceptance blockers (V §2).
