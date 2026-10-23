# 07 — References

Date: 2026-10-22. Baseline: `main` at `23bf1a52d856060556147f73c1bd5f90c67c8d5d`, branch
`critique/conceptual-audit-2026-10-22`. Concept nodes: `ReferenceImplementation`,
`PositionalReference`, `GamesOracle`, `PuzzleSample`, `Recall`, `HandLabel` and
`PositionalFixtures` (book positions), `OpeningNames`, `GameCounts`; planned `BookCorpus`,
`BookIndex`, `ConceptTags`, `BookPassages` (`FactCheck` only where they meet).

## Conclusion

The parent critique holds. The project has five kinds of reference: specification oracles,
the Lichess theme taxonomy, hand or book labels for chess meaning, descriptive opening and
game data, and planned book advice. They are mostly kept apart, and the repository already
documents several alleged weaknesses: targets are floors, Lichess agreement is not
precision, and game counts never judge a move. The gaps are where one reference borrows
another's authority:

1. Recall measures coverage, but the glossary says it shows that definitions catch what
   players mean. For `hangingPiece` and `pin`, the mapped motifs also appear in 61% and 44%
   of background puzzles without that theme.
2. Nothing checks the positional definitions against chess meaning. The ten book positions
   carry labels taken from the definitions.
3. The planned M5 acceptance checks only whether passages are on topic, not whether they
   apply to the position.

Opening names and game counts are the best-validated references. The game-count checker
uses a different chess library and found a real position-key bug, which also affected
opening-name lookup. Two provenance caveats remain.

## What the references establish

| Role | Reference | Agreement shows | Cannot show |
|---|---|---|---|
| Specification | `tactics_motif_oracle`, `positional_oracle`: a second implementation written from the plan text; both use chess-library | the code matches the written definition | that the definition matches chess meaning; shared library behavior |
| Specification | `games_oracle`: python-chess | the table matches the M3b rules | that counts reflect move quality (by design) |
| Taxonomy | `tactics_puzzle_eval`: recall on 300-puzzle strata; agreement and direct share are printed | coverage of Lichess themes | precision; that the matched instance is the puzzle's idea |
| Chess meaning | T, C and P fixtures; 10 P boards from books | a hand-set label on a few boards | see REF-3 |
| Descriptive | opening names and game counts | sourced facts | quality |
| Planned advice | BM25 passages | topical match | applicability |

## Findings

### REF-1 — Recall shows coverage, not identification (contract-level concern)

- **Root cause:** [SOURCE] In [`puzzle_eval.cpp`](../../tests/tactics/puzzle_eval.cpp)
  (`process_row`), the evaluator joins the root's state motifs with every line motif. A
  theme counts as found when any mapped instance belongs to the solver, at any ply and in
  any role ([M2](../milestones/M2.md) D1, §7.3). Only recall gates.
- **Evidence:** [OBSERVED] Running
  `build/release/tests/tactics/tactics_puzzle_eval tests/fixtures/puzzles/test.csv` (exit
  0) prints background columns that the receipts omit:

  | Tag | Background tagged | Background predicted | Background FP | Recall | Direct share |
  |---|---:|---:|---:|---:|---:|
  | hangingPiece | 179 | 3,710 | 3,531 | 1.000 | 1.000 |
  | pin | 449 | 2,906 | 2,459 | 0.997 | 0.515 |
  | fork | 870 | 1,253 | 408 | 0.980 | 1.000 |
  | skewer | 128 | 544 | 422 | 0.927 | 1.000 |

  The mapped motif fires on 60.7% (3,531/5,821) of background puzzles without
  `hangingPiece` and on 44.3% (2,459/5,551) without `pin`. For `fork` and `skewer` the rate
  is 7–8%. The run's `--disagreements 3 out.tsv` option writes an example, row `pin FP
  cQUXI`. That puzzle is themed as a fork, and its solution is the Qxb5+ fork. The `pin`
  prediction comes from a root `pin_relative` (Rh1 against h6 and h8) and a ply-4 pin of
  c6 behind Qb5.

  [SOURCE] An [M2](../milestones/M2.md) §15 row of 2026-10-19 says that requiring a later
  move to touch the motif "cut pin recall by .127". Each 0.05-step target lets 9 (`fork`)
  to 23 (`skewer`) more of the 300 stratum puzzles be missed before the gate fails.
- **Impact:** [INFERENCE] For two of the ten themes, the gate cannot tell recognizing the
  idea from firing often. Section 5.3 of
  [DEFINITIONS](../DBC/DEFINITIONS.md) says recall "checks that our definitions catch what
  chess players mean by the word"; that overstates what is measured. The repository
  already documents the neighbouring limits: agreement is not precision (§5.4), targets
  are floors (§5.6), and the direct share (§5.8).
- **Strongest defense:** The broad union is deliberate. At M2 checkpoint 1 the Captain
  removed a filter that existed only in the evaluator, so that recall measures exactly what
  the report contains. Lichess themes are incomplete, so precision cannot be gated. The
  direct share shows that, for nine themes, every match has at least one mapped instance
  judged `direct`.
- **Recommendation:** [PROPOSED] Keep the gates. Print the untagged-background firing rate
  next to recall. Report any stratum puzzle that a change newly misses, even while recall
  stays above target. Audit false positives the way §8.3 audits misses: 20 per theme,
  classified as either "pattern present, Lichess names another idea" or "the definition
  fires where players would not use the word". Phrase §5.3 as coverage.
- **Experiment:** [PROPOSED] Audit 20 matched stratum puzzles each for `hangingPiece` and
  `pin`, in hash order. If at least 19 of 20 contain the solution's idea as a matched
  instance, that weakens the concern on this sample; it does not establish general accuracy.

### REF-2 — The test sample is selective and no longer a clean holdout (accepted limitation; prospective risk)

- **Root cause:** [SOURCE] A puzzle is eligible only with rating deviation ≤ 80, at least
  1,000 plays, popularity ≥ 80 and at most 7 moves ([M2](../milestones/M2.md) §7.1).
  [`sample_puzzles.py`](../../tools/sample_puzzles.py) accepts only `--split test|dev`. The
  test split has since been read puzzle by puzzle:
  - the §8.3 audit of misses;
  - the 2026-10-19 false-positive analysis ("92% … on test");
  - the open scope proposals in [CURRENT_STATE](../CURRENT_STATE.md), which come from test
    misses.
- **Evidence:** [OBSERVED] A throwaway pass applied the §7.1 filter to the pinned export
  (SHA-256 `76335bfa…`, matching §7.1 and `assets/MANIFEST.txt`). 1,326,623 of 6,157,341
  puzzles are eligible. Its effect on what remains:

  | Puzzle group | All puzzles | Eligible |
  |---|---|---|
  | `equality` ("save the draw") | 12,875 | 1 |
  | rating ≥ 2500 | 3.79% | 0.13% |
  | `quietMove` | 4.19% | 2.75% |
  | `mate` | 31.8% | 21.8% |

  The test background (6,000 puzzles) has no `equality` puzzles, 330 `defensiveMove` and
  173 `quietMove`. [SOURCE] Each root follows the opponent's move `Moves[0]`, and the
  solution is an only-move line ([PostM2_EM1](../milestones/PostM2_EM1.md) §7).
  [SOURCE] The [M3a receipt](../receipts/M3a.md) attributes the rarity of
  `development_lead` (1,208 of 53,378 boards) to the boards being mostly middlegames.
  [`fetch_assets.sh`](../../tools/fetch_assets.sh) `fetch_puzzles` downloads whatever
  export is current. The dev split can therefore only be rebuilt from the archived export.
- **Impact:** [INFERENCE] Recall is an estimate for popular, short, decisive puzzles.
  Implementing the open scope proposals would raise test recall partly on the very puzzles
  that were audited. The oracles check agreement rather than estimate rates, so the sample
  does not bias them, but they see fewer opening positions and quiet structures.
- **Strongest defense:** The filters buy reliable solutions and themes, and recall needs
  positions that carry a theme. The committed CC0 sample (D4) can be rerun anywhere.
  [M2 receipt](../receipts/M2.md): dev and test values agree closely.
- **Recommendation:** [PROPOSED]
  - Draw a third sample the same way, with a new hash salt, excluding test and dev IDs
    (about 1.3M eligible IDs are unused). Run it once per approved definition change and
    never inspect it puzzle by puzzle.
  - Build a local, uncommitted CSV of positions from the pinned broadcast months and run
    both oracles on it. Like dev, it stays local because the games are CC BY-SA.
- **Experiment:** [PROPOSED] After the next definition change, compare recall on the third
  sample with test and the estimated ±0.02–0.03 sampling noise (§5.6). Agreement means
  no drift was detected at that resolution, not that overfitting was disproved.

### REF-3 — Book positions do not record which labels the books assert (contract-level concern)

- **Root cause:** [SOURCE] [M3a](../milestones/M3a.md) §8 asks for two things at once:
  each book board "lists the features the book's text names", and "labels come from the
  plain §4 definitions". It also requires a two-pawn `pawn_chain` near miss. Nothing in a
  fixture records which of the two sources a label has. In
  [CONCEPTS](../DBC/CONCEPTS.md), `HandLabel` labels come "from the chess meaning".
- **Evidence:** [SOURCE] In [`positional.json`](../../tests/fixtures/positional.json), the
  ten book boards (P01–P03, P05, P08–P10, P58, P59, P70) cover 5 of the 24 tags. Five
  boards carry pawn-island counts, and four of those check nothing else. Every `reason`
  restates a §4 condition.
  - P10 (`2r3k1/1brnqppp/p1p1p3/1p2P3/3P1Q2/2RB1N2/PP3PPP/2R3K1 b - - 0 19`) excludes
    `pawn_chain`. Its reason: d4 and e5 "defend diagonally, but the maximal run has only
    two pawns, below the three-pawn definition". [INFERENCE] Ordinary usage calls d4–e5 a
    chain, so this records the rule's boundary.
  - P08 is titled the "minority-attack position" but checks island counts; named
    structures are deferred.
  - The M3a receipt reports that "no rule/label mismatch came up".
- **Impact:** [INFERENCE] Because the labels follow the definitions, the checkpoint-2
  mismatch review could not trigger. The P fixtures and `positional_oracle` both check
  conformance, which leaves the positional definitions without a chess-meaning reference.
  This matters most for the vaguer tags (`outpost`, `bad_bishop`, king safety, space),
  which have at most one book board.
- **Strongest defense:** Deterministic tests need labels as precise as the rule. Private
  text cannot be committed. The Captain reviewed the fixture table and the P5 choices.
  Deferring named structures avoids wrong `rule` labels. See also
  [03 — Positional facts](03-positional-facts.md).
- **Recommendation:** [PROPOSED]
  - For each book label, record whether the text states it (no excerpt needed).
  - Keep P10, but present it as a rule-boundary case.
  - Keep a reviewed list of features the text names but the rule omits.
  - Label future book boards for the vaguer tags from the text before reading §4.
- **Experiment:** [PROPOSED] The Captain checks the ten boards against the books. If every
  included label is stated in the text and no text names a feature the rule leaves out,
  REF-3 reduces to wording.

### REF-4 — Planned retrieval: relevance is not applicability (prospective risk)

- **Root cause:** [SOURCE] [Vision](../PROJECT_VISION.md) §8 plans BM25 over chunks, with
  the report's tags expanded by a keyword map as the query and 1–3 excerpts shown as
  general advice. The M5 acceptance (§11) is "relevant pages for a fixed tag set;
  citations resolve". [DEFINITIONS](../DBC/DEFINITIONS.md) §3.1 assigns `book`
  provenance to "reference material, opening names".
- **Evidence:**
  - [SOURCE] Position-level tags are nearly constant across the 53,378 oracle boards:
    `pawn_islands` 53,306, `king_zone_attacker` 46,733, `isolated_pawn` 40,801 (M3a
    receipt §9).
  - [SOURCE] The vision's example tag `isolated_queen_pawn` does not exist. `isolated_pawn`
    covers any file, and IQP-type families are deferred (M3a §3).
  - [OBSERVED, parent probe]
    `chesswhy --fen "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2" --move f4 --budget standard`
    prints on the asked line `+ doubled_pawns: black, f file: p f4, p f7 (ply 2)` and
    `+ material_ahead: black 1 (ply 2)`. f4 is an inaccuracy (loss 0.069).
  - [INFERENCE, unrun] A query for "doubled pawns" or "extra pawn" is on topic here, but
    typical book advice on those topics does not clearly apply to an accepted gambit pawn.
- **Impact:** [INFERENCE] M5 could pass its planned acceptance while the passages shown
  are inapplicable, one-sided or contrary to the verdict. A shared `book` label would merge
  a pinned CC0 table with private, unpinned prose. The local index has no recorded id.
- **Strongest defense:** M5 is neither built nor frozen. Passages are "never … analysis of
  the position", and the fact check keeps notation from other games apart.
- **Recommendation:** [PROPOSED]
  - Judge M5 passages on three labels: on topic, applicable to this report's position, and
    stance toward the report's facts. Use fixed report fixtures.
  - Query only tags tied to printed lines.
  - Review keyword-map entries so that a rule tag maps only to book terms it actually
    implies.
  - Give passages their own provenance, and record a local index id in the report.
  - For how passages enter the prose, see [10 — Explanations](10-explanations.md).
- **Experiment:** [PROPOSED] On F25–F30, if at least 80% of the top three passages are
  applicable and none contradicts a report fact, the risk is low.

### REF-5 — Descriptive data is sound; two provenance caveats (accepted limitation; caveat 1 contract-level)

- **Evidence:**
  - [OBSERVED, parent probes] The f4 report:
    - names the root "C20 King's Pawn Game" and gives 40,708 games: f4 233, Nf3 38,321;
    - prints `games: played in 233 of 40708 games [games]` beside the engine's inaccuracy,
      with no ranking words.
  - [OBSERVED, parent probes] The README position gets `opening.current` null and
    `found: false` rather than a guess. Its `opening` and `games` blocks are byte-identical
    at `quick`, `standard` and `deep`.
  - [SOURCE] [`count_games.py`](../../tools/count_games.py) uses python-chess
    (`fen(en_passant="legal")`). It caught chess-library's en passant key bug (M3b review).
    It is the only reference that uses a different chess library from the code it checks.
  - [OBSERVED, throwaway] In three pinned months, games relayed twice (B3) add 0–2 games
    per month (≤ 0.085%), matched on players, date and the first 40 plies.
- **Caveat 1 — table id:** [SOURCE] [`Table::load`](../../src/games/games.cpp) always
  reports `table = kTable` ([M3b](../milestones/M3b.md) §5), while months and limits come
  from the loaded file. With `--games FILE`, the id therefore names the expected build, not
  the file actually read. `kDataset` and `--openings` behave the same way. For the runtime
  reproduction, see [09 — Reports](09-reports.md).
- **Caveat 2 — time controls:** [OBSERVED, heuristic] In the 2026-08 file, 8,478 games pass
  the rating, result and variant filters. 2,844 of them have no `TimeControl` tag. Of the
  other 5,634, 1,907 (33.8%) are rapid or faster (base time + 60 × increment < 60 min).
  Time controls are an accepted non-goal (M3b §3). Yet the vision dropped online games
  because "fast online games say little about considered opening choices", and the same
  concern applies to fast games over the board.
- **Recommendation:** [PROPOSED]
  - Verify or compute the table hash at load time, within the 200 ms limit. The M3b
    receipt's historical load-plus-find measurements are 108.7–144.0 ms.
  - State "all time controls" in the source text, or let the Captain decide on a split.
- **Experiment:** [PROPOSED] After the change, a custom valid table loaded with `--games`
  must report its own id, and load plus find for the pinned table must stay under 200 ms.
  Counting time-control classes over all 81 months with the same heuristic settles the
  share.

## Strengths to retain

- The oracles are described as conformance checks (DEFINITIONS §5.7). They gate on zero
  mismatches, and each mismatch is fixed at its root. The first M2 run found 74
  mismatching lists, traced to two detector causes, and the text was clarified in two
  places.
- The metric wording is honest: floors, agreement and direct share. The CC0 sample is
  committed.
- Every fact carries provenance. Lookups report "not found" and never guess.
- Months, commit and table are pinned with checksums, and the table oracle uses a
  different chess library.

## Verification receipt and limits

Observed by this review:

- One run of the existing `tactics_puzzle_eval`, for its background columns and
  disagreement rows only. This is not a new gate pass.
- Throwaway Python passes over the pinned puzzle export and four pinned broadcast months.
  Their heuristics differ from the builder's rules, and the scripts are not committed.

Parent probes are cited as parent probes.

Not run: M5, any check against book text, the proposed third sample, the full suites. No
timing claims are made.

Not measured: how many matched stratum puzzles rest only on incidental instances. Direct
share and the §15 experiment bound it.

## Sources

[`tests/tactics/puzzle_eval.cpp`](../../tests/tactics/puzzle_eval.cpp) ·
[`tests/tactics/oracle/reference.cpp`](../../tests/tactics/oracle/reference.cpp) ·
[`tests/positional/oracle/reference.cpp`](../../tests/positional/oracle/reference.cpp) ·
[`tools/sample_puzzles.py`](../../tools/sample_puzzles.py) ·
[`tools/count_games.py`](../../tools/count_games.py) ·
[`tools/fetch_assets.sh`](../../tools/fetch_assets.sh) ·
[`src/games/games.cpp`](../../src/games/games.cpp) ·
[`include/chesswhy/games.hpp`](../../include/chesswhy/games.hpp) ·
[`src/openings/openings.cpp`](../../src/openings/openings.cpp) ·
[`tests/fixtures/positional.json`](../../tests/fixtures/positional.json) ·
[M2](../milestones/M2.md) §7–§8, §15 · [PostM2_EM1](../milestones/PostM2_EM1.md) §7 ·
[M3a](../milestones/M3a.md) §3, §8 · [M3b](../milestones/M3b.md) §1, §3, §5 ·
[M2](../receipts/M2.md), [PostM2_EM1](../receipts/PostM2_EM1.md),
[M3a](../receipts/M3a.md), [M3b](../receipts/M3b.md) receipts ·
[DEFINITIONS](../DBC/DEFINITIONS.md) §3.1, §5, §8 · [CONCEPTS](../DBC/CONCEPTS.md) ·
[PROJECT_VISION](../PROJECT_VISION.md) §5.7, §8, §11 ·
[CURRENT_STATE](../CURRENT_STATE.md)
