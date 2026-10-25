# 07 — Reference knowledge, reports and explanation

Date: 2026-10-24. Baseline: `1ed0ce354473f19a0c2699f9f2f342abff215e18`.

Concept nodes: `Reference` and its L2 nodes; `GameTable` and its L3 nodes; `Report` and its L2 nodes; `Explanation` and its L2 nodes; `FactCheck` and its L3 nodes. Resumed from `8b561c4`; observations below use the existing Release binary, report v8. Language-model prose is now M5, not M4, and remains planned.

## Conclusion

The strongest new finding is that first-visit counts are worded as counts of games containing a move.
A legal repetition can make “not played in these games” false even when the table follows its contract exactly.
Second, the relevance projection removes deferred-parry context before the future fact sheet can preserve it.
This is a selection defect, not evidence that the existing parry rule should change.
Loaded-byte identities, explicit pass premises and relational claim checking address important prior concerns.
EM4's motif merge is lossless under its exact equality rule; compact rows trade detail for readability without changing the restriction.
The new ply walk and replayed FENs clarify board/time references; they do not repair incorrect upstream labels or certify complete explanations.
No further independent defect in the new layout was demonstrated.

## Findings

### REP2-1 First-visit frequency is not games-in-which-played frequency

**Definition — Source.** `Planning/DBC/DEFINITIONS.md:483`: “Per source and position, the number of games, results and the count for each move played; `games` provenance; every figure names its source and platform and is never phrased as move quality.” `Planning/milestones/M3b.md:65–70` counts a repeated position only at its first occurrence and requires move counts to sum to games. `CONCEPTS.md`, `GtCount`, likewise says a repeated position counts once.

**What it claims vs what it computes.** The partition counts the move chosen on each game's first visit, not whether that game ever played a given move from the position. Both are defensible statistics, but they differ after a repetition. Deduplicating position visits silently censors later choices.

**Counterexample — Observed board check and rendering; Source counting rule.** Start FEN:

```text
rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
```

Python-chess legally replayed `Nf3 Nf6 Ng1 Ng8 e4` and confirmed identical position keys at plies 0 and 4. Consider five distinct eligible games with this prefix, ending in agreed draws. All five play e4 from the starting position; the first-visit rule counts only Nf3 there.

For an observed rendering probe, an in-memory custom table encoded exactly those first-visit counts: four position rows, each five draws and its first move counted five times; normal source headers, `games_read = games_kept = 5`, no exclusions, `max_ply = 40`, `min_games = 5`. It was supplied on stdin, with no file edits:

```text
build/release/chesswhy --fen "<start FEN above>" --moves e4 --full-report --games /dev/stdin
```

Exit 0; the e4 engine-line block printed `games: not played in these games [games]`. The Nf3 block printed `games: played in 5 of 5 games [games]`. This is a constructed table derived from the published counting rule, not a claim that the shipped database contains these five records or that the builder was run.

**Root cause.** Proxy ≠ claim and composition (families 1, 5): “count each position once” is composed with a move-frequency interpretation that quantifies over the whole game. Position deduplication is not position–move deduplication.

**Inherited by.** `GameCounts`, `GtCount`, `GtRow`, full-report game wording, move-report frequency wording, and the future exact-match fact check. Exact agreement with the number cannot repair the wrong statistical interpretation.

**Candidate fixes.**

1. Keep the current data and say “chosen on the first visit in X of Y counted games”; zero means “not chosen on the first visit”. Expose the counting policy with the table metadata. Negligible runtime cost; preserves the sum-to-games invariant.
2. Count distinct `(position, move)` pairs per game. Offline memory/work increases modestly; a game may count for several moves, so move counts no longer sum to games and the loader/schema contract must change. This supports the literal “played in” claim.

**Severity:** `wrong claim` — the negative wording can deny a move actually played in every represented game. This is separate from prior REF-5's provenance/time-control caveats.

### REP2-2 The relevance filter deletes mandatory defensive context

**Definition — Source.** `Planning/DBC/DEFINITIONS.md:89` keeps “`O`'s threat only when the move parries it”; line 501 defines fact selection as keeping “the core claims (verdict, refutation, conditions) at every level”. `Planning/PROJECT_VISION.md:309–310` explicitly includes deferred parry among those conditions. M4 §4.6 projects only successful parries, a rule unchanged by EM3 or EM4.

**What it claims vs what it computes.** “Facts that bear on the move” becomes “positive successful-parry facts”. A checking move can matter precisely because it postpones an opponent's threat. That relationship is omitted, even though the full report computed it. The future model, restricted to move-report fields, cannot restore it without inventing chess content.

**Counterexample — Observed.** Python-chess checked this FEN and legal `Ne2+`:

```text
2r3k1/Q5pp/1Bq5/P2pb3/8/2n2R1P/2p2PP1/2R3K1 b - - 3 34
```

Commands: `build/release/chesswhy --fen "<FEN above>" --moves Ne2+`, and the same with `--full-report --json`. Both exit 0. In v8, Ne2+ is `best`; its full-report parry object is `{source: rule, value: false, test: deferred_by_check}`. White's conditional threat is `Qf7+ Kh8 Qf8+ Rxf8 Rxf8#`; `creates_threat_skipped` is `gives_check`.

The default ply walk prints the checking fork and material gain, but neither the White threat nor the deferred-parry condition. Its only line begins `Ne2+ Kf1 Bf4 ...`. The opponent's conditional threat is unavailable to the reader and projected fact sheet. This does **not** establish that the mating threat persists after Kf1; the omitted fact concerns the root threat and the immediate checking response only.

**Root cause.** Inherited exclusion and composition (families 3, 5): a false boolean conflates “searched and not parried” with the deliberately separate “deferred by check” case when the projection discards both. Faithful copying of retained facts does not prove relevance completeness.

**Inherited by.** `RelevanceFilter`, `MoveReport`, `ProjectionCheck`, `Conditions`, `FactSheet`, template fallback and eventual prose. This is the new M4/v8 downstream angle on prior EXP-4, not a repeat of the old entity-only checker criticism.

**Candidate fixes.**

1. Project a qualified defensive-relation object for deferred checks, including the existing opponent threat and test reason. Print “checks, postponing the root threat” with the pass premise. No extra search; one more conditional row.
2. Define a mandatory claim/condition set before projection and require both projection and prose to cover it, not merely avoid unsupported claims. Deterministic membership checks are cheap; deciding which ordinary failed-parry cases are explanatory remains an explicit policy choice.

**Severity:** `misleading` — printed positive facts remain true, but the supposedly relevant explanation removes computed defensive context.

## Sound definitions

- **Position key, opening name, last named position:** exact lookup/mainline semantics; counters intentionally ignored for descriptive opening matching.
- **Game-count table, rating floor, broadcast month, excluded/broken game, ply limit, minimum count, `chesswhy-games`:** explicit bounded sampling rules; exclusions do not claim all later moves were legal. `GtStandard`–`GtLegal`, `GtExcluded`, `GtKeep` and `GtDropped` accurately describe those operations.
- **Table id, data digest, `custom@`:** loaded-byte identity fixes prior REP-4's data issue; identity is not authentication of a custom table's historical claims.
- **Book corpus/index/concept tag/passage:** planned retrieval is separated from analysis, with applicability and stance requirements addressing prior REF-4; implementation remains unobserved.
- **Fact, provenance, facts first, report/full report, schemas, JSON/output flags:** useful origin and representation contracts, not independent proofs of chess meaning.
- **Reproducible/rerun identity:** scoped to the same configuration; missing executable identity remains documented, not rediscovered here.
- **Move selection, move report document, semantic/projection checks:** deterministic selection and conformance checks; not completeness or causal-validation certificates.
- **Explanation, template, level, fact sheet/selection, claim unit, language model, fact check/relational tests:** sound intended separation and relational checking; mandatory coverage must survive projection (REP2-2). No model or checker performance is claimed.
- **Motif repeats/compact rows:** EM4 §2.1 requires equal board, pieces, restriction and contribution; `also` preserves differing labels. Compression does not merge distinct plies or restrictions. Upstream restriction meanings remain the motif critique's concern.
- **Ply walk/start block/ply block/`line_fens`:** explicit replay anchors, bounded net-change wording and separate refutation plies are sound. Observed `--moves Bxa4,Ba7 --json` on V2 (`1br3k1/5pp1/p3p2p/1bBn4/N7/1P3PP1/5R1P/2R3K1 b - - 0 1`) produced seven FENs per move, all independently matched by legal python-chess replay; merged relative/tactical-pin labels remained present. Tempo and positional interpretations are covered in critiques 00–06, not re-litigated here.
