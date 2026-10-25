# 03 — Tactical motifs

Date: 2026-10-24. Baseline: `c601a6b769eb5fba3a92b6f009ae99b88b2832b8`.

Concept nodes: `Motifs`, `BoardAtPly`, `StaticExchange`, `StateMotif`, `MoveMotif`, `TacticalPin`, `Restriction`, `FirstAppearance`, `PositionMotifs`, `LineMotifs`, and all `Tp*` nodes. Resumed from `8b561c4fcfe58a8226910234b40413b9880f62e4`; runtime evidence below was obtained again with the existing Release binary, reporting schema 8. No build or test suite was run.

## Conclusion

The presence-versus-outcome distinction remains the right foundation.
The strongest defect is that a relative pin can forbid a checking escape using a capture that is illegal while answering that check (MOT2-2).
Next, “costly” movement considers what is exposed but forgets what the moving piece already captured (MOT2-1).
A narrower terminology defect equates a rook's presence on the back rank with guarding it (MOT2-3).
EM2's explicit move lists improve the old deletion-only tactical-pin test; these findings concern the new lists' meaning, not a repeat of that criticism.
EM4's repeat merge preserves differing restrictions and therefore neither creates nor fixes these problems.
The explicit first-appearance wording and SEE-labelled numbers are improvements; earlier concerns are not restated as new findings.
No change to the Captain's decided pure-reply fix is proposed here.

## Findings

### MOT2-1 A “costly” move can collect more than it exposes

**Definition — Source.** `Planning/DBC/DEFINITIONS.md:290`: “The held piece's moves that the motif makes illegal (absolute pin) or costly: after it, X wins the rear piece (relative pin), wins a guarded piece (overloaded defender) or can still make the threat (tactical pin).” Line 295 defines no free move as held “at square w”. `PostM3_EM2.md:104–106` tests only the resulting board's target SEE or enabled threat.

**What it claims vs what it computes.** The computation establishes exposure, not a net cost or a reason to stay put. It does not debit the beneficiary for material the held piece just captured. The geometric relative pin itself is real; the stronger movement interpretation is the problem.

**Counterexample — Observed.** Python-chess validated this FEN and both continuations below:

```text
build/release/chesswhy --fen "6k1/8/8/4r3/8/2n5/8/BQ4K1 b - - 0 1" --moves 'Nxb1,Ne2+' --full-report --json
```

Exit 0. At ply 0, White's `pin_relative`, Ba1 → Nc3, Re5, lists `c3b1` as restricted, `free: []`, `held_to: {kind: square, name: c3}`. The matching capture-type tactical pin also restricts `c3b1`. Yet `Nxb1 Bxe5` captures a queen for a rook: Black gains four pawns net. ChessWhy itself selects that continuation for Nxb1 and reports consequence `material`, material `4`, plies `6`. Nxb1 is a mistake compared with the stronger Rg5+; this example does **not** claim it is best. Its material gain nevertheless cannot be described by a test that sees only the rook lost afterward.

**Root cause.** Wrong temporal baseline and proxy versus claim (families 2, 1): evaluation starts after the held move has collected its compensation.

**Inherited by.** Restricted/free partition, `held_to`, existence condition, tactical-pin movement tests, and denied-move contribution consumers. Broader downstream misclassification is **Inference**, not observed here.

**Candidate fixes.** (1) Keep the cheap board rule, but call these “moves exposing the target/enabling the threat”, not moves made costly; no search cost, weaker but accurate claim. (2) For material threats, subtract the held move's captured/promoted material from the subsequent exchange loss; negligible board-accounting cost, still not a proof against intermediate tactics. Use bounded searches if actual move quality is required.

**Severity:** `misleading` — the exposure is true; “costly” and “held at c3” imply more.

### MOT2-2 A checking escape is restricted by an illegal reply

**Definition — Source.** `Planning/DBC/DEFINITIONS.md:242`: the rear piece is one “that X would win material on if the front piece moved”. Restricted moves inherit that claim (line 290). `PostM3_EM2.md:104` requires an off-segment move followed by `see(q's square, X) ≥ 1`; unlike its tactical-pin row, it does not require a legal capture.

**What it claims vs what it computes.** SEE's deliberate blindness to check is promoted into a claim that the exposed target can actually be taken now. This is not merely a speculative later tactical resource: the advertised capture is illegal on the very board being tested.

**Counterexample — Observed.** Same FEN, command and exit status as MOT2-1. The relative pin restricts `c3e2` (Ne2+); the tactical pin correctly puts it in `free`. Python-chess confirms that after Ne2+, White is checked and `a1e5` is illegal. `Ne2+ Kh1 Re8` is a legal escape retaining both black pieces. The observed engine line instead begins `Ne2+ Kf2 Re6`; its six-ply consequence is material `0`. No forced long-term saving claim is needed: the immediate capture justification is already false.

**Root cause.** Inherited exclusion and perspective mismatch (families 3, 6). `src/tactics/tactic.cpp:675–678` reads SEE after the move without checking the capturing side's legal replies. The overloaded-defender branch does likewise (`690–697`). SEE is intentionally square-local; M2 already adds a legal-capture gate to hanging pieces in check.

**Inherited by.** `Restriction`, `held_to`, denied answers and contribution. The two motif labels describe the same Ba1–Nc3–Re5 relation but disagree about this escape. EM4 correctly leaves them unmerged because their restrictions differ.

**Candidate fixes.** Require a legal capture on the threatened square before classing relative-pin/overload moves as denied. Reuse the hanging-piece/tactical-pin gate; move-generation cost, no searches. If eventual recovery rather than immediate capture is intended, require a bounded continuation and report that different premise; extra searches, horizon qualification needed.

**Severity:** `wrong claim` — the stated material-taking restriction relies on an illegal response.

### MOT2-3 Being on the back rank is not guarding it

**Definition — Source.** `Planning/DBC/DEFINITIONS.md:243`: “Y king on its first rank, boxed in by its own pieces or X's attacks, and no Y rook or queen guards the rank.” M2's actual rule (`Planning/milestones/M2.md:88`) instead requires “Y has no rook or queen on its first rank”.

**What it claims vs what it computes.** Effective defence is replaced by occupancy. An obstructed rook suppresses the weakness even when it cannot meet a back-rank invasion.

**Counterexample — Observed.** Python-chess validated the position and confirmed Re8# is legal checkmate:

```text
build/release/chesswhy --fen "r1n3k1/5ppp/8/8/8/8/8/4R1K1 w - - 0 1" --moves 'Re8#' --full-report --json
```

Exit 0. `position.motifs` contains no `back_rank_weakness`; the move has `back_rank_mate`. Black's own Nc8 blocks Ra8 from e8. All other weakness conditions hold.

**Root cause.** Proxy versus claim (family 1): rank membership is not a guard relation. This is contract fidelity, not a detector implementation bug.

**Inherited by.** Position motifs, first-appearance events and the availability of a back-rank explanation/contribution witness.

**Candidate fixes.** (1) Say “boxed-in king with no rook/queen on its back rank”; zero runtime cost, explicitly narrower motif. (2) Test legal defensive captures/interpositions against candidate rank invasions; modest board-rule cost, more faithful but requires a defined threat square rather than a vague whole-rank guard.

**Severity:** `misleading` — an omitted witness, not an observed false mate statement.

## Sound definitions

- **Motif, tag, instance, provenance:** deterministic pattern identity and no independent gain claim are appropriate; verdicts remain separate.
- **BoardAtPly, position/line motifs, ply, actor, targets, threat:** explicit board/participant bookkeeping; threat landing squares handle en passant separately from victims.
- **State/move distinction:** useful operational separation, but “board alone” needs its documented exception: hanging-piece trade suppression reads the producing move (M2:75–76).
- **Trade in progress:** crediting the preceding capture prevents calling an immediately compensated exchange a gift; “recapture” need not literally describe every such capture.
- **Absolute pin:** off-ray movement exposes the king; hypothetical pawn captures are explicitly counterfactual, not additional legal chess moves.
- **Free moves, move set, existence condition, held-to geometry:** coherent set operations relative to each motif; not global safety guarantees. Their input predicates have the defects above.
- **Tactical-pin gates/removal/actor and threat witnesses:** explicit conditional experiment; testing actual candidate movement improves deletion alone. Restrictions remain motif-local.
- **Hanging/trapped pieces, relative pins, overloaded defenders:** coherent local SEE predicates, not certified whole-board winnings; MOT2-1/2 identify newly demonstrated overextensions.
- **SEE and `gain`:** a specified least-attacker exchange model with stopping and pin/king rules; “best” must stay within that model, not mean best legal chess play.
- **Fork, skewer, discovered attack:** specified geometric/material filters define narrow presence patterns, not guaranteed gains.
- **Discovered check, double check, back-rank mate, promotion:** directly checked move/board events; overlapping tags are legitimate, not exclusive categories.
- **Not-detectors and removed line relevance:** correctly separate future engine consequences from board patterns.
- **First appearance:** accurately specifies adjacent-ply absence/presence, not first-ever occurrence; identity changes and gaps are explicit.
- **EM4 repeats:** same-ply equality includes restrictions and contribution, while `also` preserves alternative labels; this is presentation deduplication, not semantic equivalence of different threats.
