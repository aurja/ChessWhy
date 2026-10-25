# PostM4_EM5 — plan (fixes from the conceptual critique of 2026-10-24)

Branch `postm4-em5`, from `main` after PostM4_EM4 is merged. Source: the critique in
`Planning/critiques/2026-10-24/` (findings 00–08, synthesis 09). This file holds the
Captain's decisions and the assignment of every finding to a part. Each part's contract
(rules, report changes, fixtures, work packages, acceptance) is frozen in this file before
its work starts; parts 2 and 3 do not depend on part 1 and may be frozen while it runs.

## 1. Captain's decisions

| # | Topic | Decision |
|---|---|---|
| R1 | Milestone shape (2026-10-25) | PostM4_EM4 closes as the critique and move report text milestone; the critique's fixes, including the pure reply replacement, form this milestone in three parts: 1 reply evidence, 2 correctness, 3 contract and wording repairs |
| R2 | VER2-1, verdict of an exact best-score tie from search C (2026-10-25) | Option A: the same-score test applies to search C results too, so a move from C with exactly the best score is `best`. Changes vision §5.4's rule that an asked move outside A is at most okay. Rejected: B, keep the rule and redefine okay as "playable, not tied within A" |
| R3 | REP2-1, what a game count counts (2026-10-25) | Option 1: keep the first-visit table and say what it measures: "chosen on the first visit in X of Y games"; zero means "not chosen on the first visit"; the counting policy goes with the table metadata. Rejected: 2, count each distinct (position, move) per game (table rebuild, move counts no longer sum to games) |
| R4 | Pure reply (2026-10-24, from PostM4_EM4) | Replace the definition with "forced and gains nothing" (option C of 2026-10-24) |

## 2. Findings by part

Group letters G1–G10 are the synthesis's (09 §2).

### Part 1 — reply evidence (in this order)

| Finding | Group | Fix owner |
|---|---|---|
| THR2-1 terminal draws fall outside consequence and refutation | G1 | consequence replay: terminal outcome |
| THR2-2 capture-only extension stops before a forced recovery | G1 | consequence replay: endpoint completeness |
| POS2-1 losing a weak pawn credited to its owner | G3 | favoured side |
| POS2-2 any check certifies use of a shelter change | G3 | use test |
| TMP-2 "converted" while the checking attack goes on | G3 | run outcome |
| 00 pure reply; TMP-4 non-pure and unresolved branches | G2 | reply evidence and the ply label / run ending state space (R4) |

Open at freeze: whether the tempo rows return to the move report text (left out by
PostM4_EM4 R3 while the tempo model is being fixed).

### Part 2 — correctness

| Finding | Group | Fix owner |
|---|---|---|
| MOT2-1 a costly restricted move can collect more than it exposes | G4 | restricted moves |
| MOT2-2 a checking escape restricted by an illegal reply | G4 | restricted moves (relative pin) |
| MOT2-3 being on the back rank is not guarding it | G4 | `back_rank_weakness` |
| TAC2-1 unmoved pieces do not imply a surviving pin | G4 | contribution rule 3′ |
| POS2-4 break cost over unsearchable moves | G4 | specify the legal search domain only; break cost stays an open proposal |
| REP2-2 relevance filter drops deferred-parry conditions | G8 | relevance filter's mandatory conditions |

### Part 3 — contract and wording repairs

| Finding | Group | Fix owner |
|---|---|---|
| VER2-1 exact tie from C graded okay | G6 | verdict (R2) |
| REP2-1 first-visit counts worded as games played | G9 | game counts (R3) |
| TMP-3 low pass cost labelled non-tempo | G5 | tempo label |
| POS2-3 impact and duration from the tag taxonomy | G5 | impact rating, static scale |
| TAC2-2 tactic recognition describes the retired margin test | G7 | glossary |
| TMP-5 even trade vs check rule | G7 | glossary (threatening move) |
| PRF2-1 output volume is not noise | G10 | proof measurements |
| PRF2-2 oracle scope wording hides the sample | G10 | proof wording |
| Vision vs concept map, six items (09 §4) | — | vision and `CONCEPTS.md`, each reconciled with the Captain |

### Not in this milestone

Plans and pawn-break purpose, prophylaxis and defensive resources, favourable
simplification (09 §3): later scope proposals.

## 3. Contract changes

| Date | Change | By |
|---|---|---|
