# Conceptual critique of ChessWhy

Date: 2026-10-22. Reviewed baseline: `23bf1a52d856060556147f73c1bd5f90c67c8d5d`
(M1, M2, PostM2_EM1, M3a and M3b complete; report v4).
Branch: `critique/conceptual-audit-2026-10-22`.

Requested by the Captain: an adversarial, constructive review of the approaches,
not of the models or people that produced them. The main session framed and
integrated the conceptual critiques; task-model agents investigated the facts.
Five reports were already saved when she requested cheaper drafting; those were
retained. The remaining five were assigned to `smol` to draft from the task agents' findings.
Runtime probes exercised the existing implementation.

## Reports

| Area | Topic | Report |
|---|---|---|
| Recognition and labeling | Motifs | [01 — Motifs](01-motifs.md) |
| Recognition and labeling | Tactics | [02 — Tactics](02-tactics.md) |
| Recognition and labeling | Positional facts | [03 — Positional facts](03-positional-facts.md) |
| Recognition and labeling | Threats | [04 — Threats](04-threats.md) |
| Recognition and labeling | Verdicts | [05 — Verdicts](05-verdicts.md) |
| Evaluation | Searches | [06 — Searches](06-searches.md) |
| Evaluation | References | [07 — References](07-references.md) |
| Evaluation | Proofs | [08 — Proofs](08-proofs.md) |
| Reporting | Reports | [09 — Reports](09-reports.md) |
| Reporting | Explanations | [10 — Explanations](10-explanations.md) |

## Recommended order of decisions

These are review recommendations, not approved contract changes.

1. **Make claims safe before M4 verbalises them.** Contribution labels do not prove
   causation; E's threat assumes a pass; `parries` tests a narrower condition than its
   everyday meaning; `holds_at_end` stops at the walked horizon, not necessarily the
   displayed line's end. See TAC-1, THR-1–3, POS-3 and EXP-1–2.
2. **Make comparison and completeness explicit.** Mixed-depth MultiPV results can be
   out of evaluation order, and `fine_truncated: false` does not establish an exhaustive
   fine set. Preserve the approved ES thresholds while deciding what evidence a
   best/alternative/completeness claim must carry. See VER-1–3 and SRCH-1–2.
3. **Recover omitted explanatory witnesses.** Null-move and refutation roots can lose
   the very state motifs that explain their first move. This is separate from adding
   more motif types or shortening output. See MOT-1 and TAC-2.
4. **Identify the data actually loaded.** A custom `--games` file should not inherit the
   shipped table's identity. Settings and declared dataset names are not content
   identities. See REP-4 and REF-5.
5. **Measure chess meaning separately from rule conformance.** Retain the existing
   oracles and regression floors; add bounded independent semantic checks and
   contradictory-sentence cases before treating them as evidence of explanatory
   correctness. See REF-1–4, PRF-1–4 and EXP-5.

The architecture worth retaining is report-first, deterministic board rules,
reproducible engine searches, explicit provenance, and separation of game frequency
from move quality. The main risk is promoting a bounded observation into a stronger
chess claim—not a lack of more detectors or a larger language model.

## How to read the findings

- **Observed** means an actual command or probe produced the stated result.
- **Source** means a contract or implementation establishes the stated behavior;
  it does not claim a new runtime observation.
- **Inference** identifies a consequence or risk not directly demonstrated.
- **Proposed** experiments have not passed merely because they are described.
- Accepted limitations are not retroactively treated as milestone failures.
  Proposed changes require a decision; this review does not approve them.

The shared question is whether each conclusion is supported at the strength at
which it is presented. A deterministic rule, an engine result, an independently
implemented specification, a book passage and a sample of games establish different
things. Origin alone does not establish certainty, relevance or causation.

## Boundaries

M4's language-model explanation and M5's book retrieval are plans, not implemented
features. Their reports assess prospective risks, not fictional runtime failures.
The Captain's approved comprehensive motif and positional output remains in scope
as a design choice; output volume alone is not a defect. The accepted expected-score
thresholds, saturation limit and two-winners limit are identified rather than
silently replaced.

No production code, fixtures, tests, milestone decisions or concept definitions
are changed by this review. The DBC reference set therefore remains authoritative
and intentionally unchanged. Suggested experiments in these reports are not new
fixtures or tests. Private book titles and excerpts, generated engine output and
local configuration changes are excluded from the branch.

## Evidence baseline

Commands use the existing Release binary and project engine, not the system
Stockfish. The exercised engine identifies itself as Stockfish 19; its executable
SHA-256 is `0f83d24cc46d2c66c60f16001af5444873bc112b7d028594513426894c12da19`.
Report-specific commands and selected observations are recorded in the topic
reports. Historical gate counts remain historical evidence; this investigation
does not claim to have rerun the full test suites.
