# PostM4_EM8 — plan (contract and wording repairs from the conceptual critique of 2026-10-24)

Called PostM4_EM5 part 3 until 2026-10-25, when the Captain renamed it (parts 2 and 3 of
PostM4_EM5 became PostM4_EM7 and PostM4_EM8). Branch `postm4-em8`, from `main` after
PostM4_EM7 is merged. Source: the critique in `Planning/critiques/2026-10-24/` (findings
00–08, synthesis 09) and the PostM4_EM5 decision briefs (`Planning/milestones/PostM4_EM5.md`
§1). The contract is frozen in this file before work starts. Terms:
`Planning/DBC/DEFINITIONS.md` §3.8.

## 1. Captain's decisions

Decided on 2026-10-25 in PostM4_EM5 (`PostM4_EM5.md` §1); they apply here.

| # | Topic | Decision |
|---|---|---|
| R2 | VER2-1, verdict of an exact best-score tie from search C | Option A: the same-score test applies to search C results too, so a move from C with exactly the best score is `best`. Changes vision §5.4's rule that an asked move outside A is at most okay. From PostM4_EM6 the test is the at-least-score test and uses N6's score order (`PostM4_EM6.md` §1) |
| R3 | REP2-1, what a game count counts | Option 1: keep the first-visit table and say what it measures: "chosen on the first visit in X of Y games"; zero means "not chosen on the first visit"; the counting policy goes with the table metadata |

### 1.1 Open decisions (briefed 2026-10-25)

Briefed to the Captain by the Deputy on 2026-10-25 as part of the PostM4_EM5 briefs, so
their IDs continue that file's D1–D14. **Open**: none is decided.

| # | Finding | Question | Deputy's recommendation |
|---|---|---|---|
| D15 | TMP-3 | Tempo labels | Keep the names; meaning becomes the pass cost only; null tempo value prints unknown |
| D16 | 09 §4 | Vision drift | Vision follows EM4 R1 on printed restrictions; a tempo section in vision §5 (reply evidence is built, PostM4_EM5); the other four items mechanical |
| — | TAC2-2, TMP-5, POS2-3, PRF2-1, PRF2-2, POS2-4 | Block approval | Wording fixes as the critiques' first candidates; POS2-4 specifies the legal domain only |

## 2. Findings

Group letters G1–G10 are the synthesis's (09 §2).

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

## 3. Contract

Not frozen. Frozen here after the Captain decides D15, D16 and the block approval.

## 4. Contract changes

| Date | Change | By |
|---|---|---|
| 2026-10-25 | Renamed from PostM4_EM5 part 3; R2 and R3 restated, D15, D16, the block approval and the findings table moved here from `PostM4_EM5.md` §1.1 and §2 unchanged; R2 notes PostM4_EM6's at-least-score test | Captain |
