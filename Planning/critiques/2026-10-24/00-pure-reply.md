# Critique 00 — pure reply (2026-10-24)

Written by the PureReplyCritique agent on 2026-10-24, before the eight-part critique, on the
PostM4_EM3 definition of pure reply (DEFINITIONS "pure reply", decided A1) and the F41 tempo
ledger. The Captain chose its second candidate ("forced and gains nothing", her option C) for
EM4 part 2. Saved to a file on 2026-10-25; the text below is the agent's verbatim output.

---

The definition confuses “no further threat” with “no independent accomplishment.” Its two tests concern what can happen **after** the reply: the threatening side’s best line falls below rank 2, and the replying side has no rank-2 threat. Neither measures what the reply just achieved, nor establishes that the opponent had to spend his move meeting our threat. A2 deliberately excludes a capture’s proceeds from *threatening move*; using that predicate’s negation to establish *pure reply* consequently makes realized gains invisible.

In the checked F41 ledger, the legal sequence is `Bxa4 bxa4 Ba7 Rfc2 Nb4 Bxa7 Nxc2`. Python-chess confirms that `Nxc2` captures a rook. The saved continuation is `a5 Rc4 Bb6`, retaining that capture. Calling `Nxc2` pure because its next threat has rank 0 mistakes **collecting a gain** for **doing nothing except defending**.

Another revealing ply is `Nb4`: the ledger calls it a counter-threat, and the knight attacks the rook on c2. `Bxa7` explicitly has `stops_threat=false`; Black then executes that very capture. The model recognizes Black’s prospective business at ply 5, but forgets it when accomplished at ply 7. This is a conceptual defect, not evidence of faulty implementation.

There is also a causal weakness: the reply search tests the threatening side’s *entire best continuation*, not the persistence of the particular preceding threat. “No rank-2 continuation remains” does not itself establish “this move was forced solely by that threat.”

**Downstream effects.** False pure replies award false tempi gained—here White’s `Bxa7`. Counter-threat remains meaningful as defined, but its complement is not pure defence: a productive response need not threaten anything next. Initiative runs therefore start or continue through the opponent’s successful activity. Their endings are no longer exhaustive after correcting purity: a productive, nonthreatening answer that stops the threat is neither spent, contested, nor runs. It needs a separate ending. Converted/evaporated then describe potentially spurious runs; moreover, their endpoint material/use tests establish retained benefit, not that initiative caused it. “Evaporated” does not repair a run that never existed. Tempo value is separately defined and does not inherit this defect directly.

Two computable alternatives:

| Candidate | Rule and required evidence | Trade-off |
|---|---|---|
| **Immediate-productivity veto** | Retain the existing reply and threat tests, but require no positive material-balance change caused by the reply and no independently productive positional change established by deterministic feature/use rules. Compare boards immediately before and after the reply. | Cheap and transparent; rejects `Nxc2`. But an obligatory recapture can fail purity despite merely restoring an exchange, while transient gains can look productive. It still does not establish forcedness. |
| **Forced, nonproductive response** | Require that acceptable replies must meet the identified threat, the actual reply meets it without counter-threatening, and the reply achieves no retained independent benefit. Search legal replies, classify which leave that threat executable, and compare their evaluations with the best reply under an explicit tolerance. Evaluate material from **before the reply** through a tactically resolved continuation; use declared positional-feature/use rules for nonmaterial benefit, excluding merely removing the threatened loss. | Closer to the literature; costs more searches and needs explicit tolerances, tactical endpoints, and threat identity. Unresolved evidence must remain unknown rather than establish purity. |

Under both, `Nb4` remains a counter-threat, not a pure reply. The second rejects `Nxc2` on the saved continuation’s retained rook capture; whether its forcedness gate passes was not searched. **[INFERENCE]** A longer resolving search should confirm that productivity classification.

I recommend the second as the target definition, with the first understood only as a conservative approximation. Keep separate facts for **stops threat**, **threatens back**, **achieves benefit**, and **forced response**. Award a tempo gained only when the evidence supports the last and excludes independent benefit. Do not broaden counter-threat to conceal the missing category: winning something now and threatening something next are different chess facts.
