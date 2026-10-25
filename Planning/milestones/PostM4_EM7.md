# PostM4_EM7 — plan (correctness fixes from the conceptual critique of 2026-10-24)

Called PostM4_EM5 part 2 until 2026-10-25, when the Captain renamed it (parts 2 and 3 of
PostM4_EM5 became PostM4_EM7 and PostM4_EM8). Branch `postm4-em7`, from `main` after
PostM4_EM6 is merged. Source: the critique in `Planning/critiques/2026-10-24/` (findings
00–08, synthesis 09) and the PostM4_EM5 decision briefs (`Planning/milestones/PostM4_EM5.md`
§1). The contract (rules, report changes, fixtures, work packages, acceptance) is frozen in
this file before work starts. Terms: `Planning/DBC/DEFINITIONS.md` §3.8.

## 1. Open decisions (briefed 2026-10-25)

Briefed to the Captain by the Deputy on 2026-10-25 as part of the PostM4_EM5 briefs, so
their IDs continue that file's D1–D9. **Open**: none is decided. Evidence at briefing: the
Release build of `main` after the PostM4_EM4 merge (report v8) on the 43 F fixtures; to be
re-measured on report v10 before the freeze.

| # | Finding | Question | Deputy's recommendation |
|---|---|---|---|
| D10 | MOT2-1 | Costly restricted move | Net exposure |
| D11 | MOT2-2 | Checking escape | Legal-capture gate (the hanging-piece gate) for relative pins and overloaded defenders |
| D12 | MOT2-3 | Back rank guarded | Back-rank guard; the motif reference implementation and recall move with it |
| D13 | TAC2-1 | Surviving pin in rule 3′ | Motif revalidation for every tag |
| D14 | REP2-2 | Relevance filter | Mandatory conditions incl. `O`'s rank ≥ 2 threat and the parry result; root urgency back as one row |

## 2. Findings

Group letters G1–G10 are the synthesis's (09 §2).

| Finding | Group | Fix owner |
|---|---|---|
| MOT2-1 a costly restricted move can collect more than it exposes | G4 | restricted moves |
| MOT2-2 a checking escape restricted by an illegal reply | G4 | restricted moves (relative pin) |
| MOT2-3 being on the back rank is not guarding it | G4 | `back_rank_weakness` |
| TAC2-1 unmoved pieces do not imply a surviving pin | G4 | contribution rule 3′ |
| POS2-4 break cost over unsearchable moves | G4 | specify the legal search domain only; break cost stays an open proposal |
| REP2-2 relevance filter drops deferred-parry conditions | G8 | relevance filter's mandatory conditions |
| PostM4_EM5 checkpoint 2 (2026-10-25): the forced test's cost. F32 `--moves playable` took 24.6–29.9 s (limit 30 s) and F12 14.2 s (limit 15 s) on node budgets. PostM4_EM6 removed the time limits and made speed a later milestone: at depth 14 the forced test is most of the explanation phase (F32 `--moves playable` 58.2 s) | — | forced test: cut its search cost (e.g. reuse the reply search), or leave it to the speed milestone (Captain to decide at the freeze) |
| PostM4_EM5 checkpoint 2 (2026-10-25): the engine collector's duplicate fallback may shorten a MultiPV search silently, as it shortened F32's forced test to 4 lines (scope proposal) | — | engine line collection: audit |
| PostM4_EM5 completion review (2026-10-25): en passant and the file-specific shelter witness. The checker presses the king from the removed pawn's square for king files and from the landing square otherwise; P11 is silent on which square counts for a file witness (scope proposal) | — | P11 wording: en passant square for the shelter witness |

## 3. Contract

Not frozen. Frozen here after the Captain decides D10–D14.

## 4. Contract changes

| Date | Change | By |
|---|---|---|
| 2026-10-25 | Renamed from PostM4_EM5 part 2; decisions D10–D14 and the findings table moved here from `PostM4_EM5.md` §1.1 and §2 unchanged, the forced-test cost row updated for PostM4_EM6 | Captain |
