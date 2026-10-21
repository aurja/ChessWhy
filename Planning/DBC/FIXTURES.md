# ChessWhy — Fixtures

Every fixture the tests use, with its board, what it claims, what the code
reports today and the concept nodes of [CONCEPTS.md](CONCEPTS.md) it checks (their
code is in [CODE.md](CODE.md)). Terms are defined in [DEFINITIONS.md](DEFINITIONS.md)
(§6.2 for the fixture kinds); the tests that read each set are in [TESTS.md](TESTS.md).

**Snapshot:** 2026-10-21, PostM2_EM1 milestone gate (commit `b4fbb30`, after batch 3,
`pin_tactical`). Release build, Stockfish 19 from `assets/`, `Threads=1`. F reports now
also list `pin_tactical` instances; the columns below name only the fixture claims.

**Comments column.** Any agent or the Captain may add a dated comment to a
fixture's last column when the fixture looks wrong: a bad position, a claim that
contradicts the engine or the rules, an unclear title. Sign it, e.g.
`2026-10-21 Deputy: …`. Leave the other columns to the orchestrator.

**Keeping it current.** The orchestrator updates this file whenever a fixture is
added, changed or dropped, and checks the "Codebase today" column at each
milestone end. Boards are generated; the tables are written by hand so the
comments survive. Regenerate the boards after any fixture change:

```sh
.venv/bin/python tools/render_fixture_boards.py   # needs `pip install -r tools/requirements-dev.txt`
```

Boards show White at the bottom. The arrow marks the asked move (F) or the first
move of the line (T, C); a red king square marks check. Rendered with
[python-chess](https://github.com/niklasf/python-chess) (GPL-3.0-or-later, a
development tool only, never linked into ChessWhy); piece drawings by Colin M.L.
Burnett, used under their BSD licence option.

**Reading the "Codebase today" column.** ES is the expected score for the side
to move (1 = win, 0.5 = draw, 0 = loss) unless the cell says "White ES". "Tactic"
and "margin" are the tactic judgement of DEFINITIONS §4.5; contributions are
`direct`, `indirect` or `none`. Plies count positions: ply 0 is the start
position, ply 1 the position after the first move of the line.

## 1. F fixtures — full reports (`tests/fixtures/positions.json`, `engine_fixtures`)

**Concepts:** `EngineFixtures`, `Urgency`, `ParryTest`, `Verdict`, `TacticJudgement`, `StateMotif`, `MoveMotif`, `TacticalPin`, `InputFixtures`, `OpeningNames`, `SearchB`

Run end to end through the CLI with the engine by `cli_fixture_<ID>`. Each
fixture also pins exact report fields (`equals`), required fine moves, motifs
and line prefixes; see the JSON for the complete list. F16 was dropped (M1 §9).
All F fixtures pass when run one at a time; under `ctest -j8` on the 8-core
laptop, eight of them exceeded the 12-second timing check (`CHESSWHY_CHECK_TIMING`),
which is a load effect, not a wrong result.

| ID | Board | Position | Fixture claims | Codebase today | Comments |
|---|---|---|---|---|---|
| F01 | <img src="boards/F01.svg" width="200" alt="F01"> | `r4r1k/pppb2bp/2q1n1p1/4p3/8/1BPPBN2/PP2QPP1/R3K2R w KQ - 6 19`<br>Asked: **O-O-O**<br><sub>Steinitz, Wilhelm - Chigorin, Michael, Wch4-Havana 1892, before 19.O-O-O</sub> | quiet; asked queenside castling is the only fine move | Eval: White ES 0.98 (+172 cp).<br>Best **O-O-O**, a tactic (margin 0.38 over Kf1).<br>Fine moves: O-O-O.<br>No concrete Black threat; more urgent: neither (quiet).<br>Asked **O-O-O**: fine. | |
| F02 | <img src="boards/F02.svg" width="200" alt="F02"> | `6R1/p6p/4pR1k/P3P3/8/r6P/2K1b3/8 b - - 2 46`<br>Asked: **Kh5**<br><sub>Alekhine, Alexander - Euwe, Max, Wch16 1935, before 46...Kh5</sub> | in check with exactly one legal move | Eval: White ES 1.00 (+482 cp).<br>Best **Kh5**, forced (only legal move).<br>Fine moves: Kh5.<br>Asked **Kh5**: fine. | |
| F03 | <img src="boards/F03.svg" width="200" alt="F03"> | `r4rk1/2p2pbp/p3bnp1/1p2p3/1q2P1P1/3P1N1P/PPB1QP2/R1B1K2R w KQ - 2 17`<br>Asked: **Kf1**<br><sub>Gunsberg, Isidor - Chigorin, Michael, Havana 1890, before 17.Kf1</sub> | in check; fine, mistake and blunder replies | Eval: White ES 0.48 (-26 cp).<br>Best **Kf1**, not a tactic (margin 0.08 over Qd2).<br>Fine moves: Kf1.<br>Asked **Kf1**: fine. | |
| F04 | <img src="boards/F04.svg" width="200" alt="F04"> | `8/7R/2r5/8/P3n3/R7/3nk1PP/6K1 w - - 3 39`<br>Asked: **Ra1**<br><sub>Korchnoi, Viktor - Karpov, Anatoly, Wch29 1978, before 39.Ra1</sub> | opponent threatens mate in one; asked move parries it but allows another mate | Eval: White ES 0.50 (+0 cp).<br>Best **g3**, not a tactic (margin 0.00 over g4).<br>Fine moves: g3, g4.<br>Black threatens Rc1# (mate); more urgent: black (consequence class).<br>Asked **Ra1**: blunder, loses 0.50 ES. Refutation Nf3+ gxf3 Rg6+ is a tactic (margin 1.00 over Nb3). | |
| F05 | <img src="boards/F05.svg" width="200" alt="F05"> | `2r3k1/Q5pp/1Bq5/P2pb3/8/2n2R1P/2p2PP1/2R3K1 b - - 3 34`<br>Asked: **h6**<br><sub>Anand, Viswanathan - Karpov, Anatoly, FIDE-Wch k.o. final 1998, before 34...h6</sub> | own material threat against the opponent's mate threat | Eval: White ES 0.00 (-616 cp).<br>Best **Ne2+**, not a tactic (margin 0.07 over Qe8).<br>Fine moves: Ne2+.<br>White threatens Qf7+ (mate); more urgent: white (consequence class).<br>Asked **h6**: blunder, loses 1.00 ES. Refutation Qf7+ Kh8 Re3 is a tactic (margin 0.84 over Rxc3); motifs: hanging piece Be5 (direct). | |
| F06 | <img src="boards/F06.svg" width="200" alt="F06"> | `8/pp2R2p/3BNkb1/3P1p2/7p/8/PP1rr2P/5R1K w - - 1 32`<br>Asked: **Bb4**<br><sub>Chigorin, Michael - Steinitz, Wilhelm, Wch4-Havana 1892, before 32.Bb4</sub> | opponent threatens material; asked move allows mate | Eval: White ES 1.00 (+437 cp).<br>Best **Rxb7**, not a tactic (margin 0.02 over Kg1).<br>Fine moves: Rxb7, Kg1.<br>Black threatens Bh5 (material); more urgent: black (consequence class).<br>Asked **Bb4**: blunder, loses 1.00 ES. Refutation Rxh2+ Kg1 Rdg2# is a tactic (margin 1.00 over Rc2). | |
| F07 | <img src="boards/F07.svg" width="200" alt="F07"> | `6k1/2n2ppp/p3q3/1pB1p1P1/1Pr1P3/PQ1r3P/2PR2K1/8 w - - 0 31`<br>Asked: **Qxd3**<br><sub>Alekhine, Alexander - Euwe, Max, The Netherlands 1927, before 31.Qxd3</sub> | equal material threats; the fine move parries by capture | Eval: White ES 0.93 (+135 cp).<br>Best **cxd3**, a tactic (margin 0.43 over Qxd3).<br>Fine moves: cxd3.<br>Black threatens Rxd2+ (material); more urgent: black (parry).<br>Asked **Qxd3**: blunder, loses 0.43 ES. Refutation h6 Qd8+ Ne8 is not a tactic (margin 0.00 over h5). | |
| F08 | <img src="boards/F08.svg" width="200" alt="F08"> | `r1bb1r1k/p4ppp/2p2n1N/7Q/2p5/2B2N2/qPP2PPP/2KR3R w - - 2 19`<br>Asked: **Qxf7**<br><sub>Tal, Mikhail - Smyslov, Vasily, Candidates' tournament 1959, before 19.Qxf7</sub> | equal material threats; checking move does not parry | Eval: White ES 1.00 (+586 cp).<br>Best **Qxf7**, a tactic (margin 0.31 over Qe5).<br>Fine moves: Qxf7.<br>Black threatens Nxh5 (material); more urgent: black (parry).<br>Asked **Qxf7**: fine. | |
| F09 | <img src="boards/F09.svg" width="200" alt="F09"> | `8/6pk/7p/5p2/5Pqn/1P5P/P2Q2P1/2r2RK1 b - - 0 39`<br>Asked: **Qg3**<br><sub>Euwe, Max - Keres, Paul, Wch18 1948, before 39...Qg3</sub> | counterattack: own threat outweighs the opponent's | Eval: White ES 0.00 (-1041 cp).<br>Best **Nf3+**, not a tactic (margin 0.00 over Qg3).<br>Fine moves: Nf3+, Qg3, Qg6, Rxf1+.<br>White threatens hxg4 (material); more urgent: black (counterattack).<br>Asked **Qg3**: fine. | |
| F10 | <img src="boards/F10.svg" width="200" alt="F10"> | `6R1/8/P7/8/1K6/6p1/5pk1/8 w - - 0 58`<br>Asked: **a7**<br><sub>Korchnoi, Viktor - Karpov, Anatoly, Candidates' final 1974, before 58.a7</sub> | rook against pawns: null move unreliable, promotion threat | Eval: White ES 0.54 (+58 cp).<br>Best **a7**, not a tactic (margin 0.04 over Rf8).<br>Fine moves: a7.<br>Black threatens f1=Q (material); more urgent: black (consequence class).<br>Asked **a7**: fine. | |
| F11 | <img src="boards/F11.svg" width="200" alt="F11"> | `1r1q1rk1/4b1pp/4b3/1p1pPp2/3p1P2/8/1PBB2PP/R2Q1RK1 w - f6 0 18`<br>Asked: **exf6**<br><sub>Lasker, Emanuel - Schlechter, Carl, Wch10 1910, before 18.exf6</sub> | en passant capture outside the engine's top lines | Eval: White ES 0.88 (+124 cp).<br>Best **Bd3**, not a tactic (margin 0.06 over Be1).<br>Fine moves: Bd3.<br>No concrete Black threat; more urgent: neither (quiet).<br>Asked **exf6**: blunder, loses 0.37 ES. Refutation Bxf6 Bd3 Qd7 is not a tactic (margin 0.01 over Rxf6). | |
| F12 | <img src="boards/F12.svg" width="200" alt="F12"> | `1r3bk1/5pp1/b4q1p/2nPN3/2p5/N2nR2P/1P3PP1/1BBQ2K1 w - - 1 26`<br>Asked: **Ng4**<br><sub>Kasparov, Gary - Karpov, Anatoly, Wch33-KK3 1986, before 26.Ng4</sub> | mistake against a material threat | Eval: White ES 0.52 (+39 cp).<br>Best **Qc2**, not a tactic (margin 0.03 over Bxd3).<br>Fine moves: Qc2, Bxd3.<br>Black threatens Nxe5 (material); more urgent: black (consequence class).<br>Asked **Ng4**: mistake, loses 0.17 ES. Refutation Qf5 Rg3 Kh8 is not a tactic (margin 0.12 over Qh4). | |
| F13 | <img src="boards/F13.svg" width="200" alt="F13"> | `rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3` | checkmate: terminal, no searches | Terminal: checkmate; no candidates.<br>Opening: Barnes Opening: Fool's Mate. | |
| F14 | <img src="boards/F14.svg" width="200" alt="F14"> | `7k/5Q2/6K1/8/8/8/8/8 b - - 0 1` | stalemate: terminal, no searches | Terminal: stalemate; no candidates. | |
| F15 | <img src="boards/F15.svg" width="200" alt="F15"> | PGN `two_games.pgn` game 2, ply 4 | PGN input: second game, ply 4 | Eval: White ES 0.52 (+28 cp).<br>Best **Nf3**, not a tactic (margin 0.00 over Nc3).<br>Fine moves: Nf3, Nc3, cxd5, g3, e3 and more.<br>No concrete Black threat; more urgent: neither (quiet).<br>Opening: Queen's Gambit Declined; last named: Queen's Gambit Declined. | |
| F17 | <img src="boards/F17.svg" width="200" alt="F17"> | `4K3/3P2r1/4k3/8/8/8/8/8 w - - 3 70`<br>Asked: **d8=N+**<br><sub>Korchnoi, Viktor - Kasparov, Gary, London 1983, round 6, annotator's line 60...Rg6 61.Rg2, before 70.d8=N+</sub> | underpromotion is the only fine move; queening allows mate | Eval: White ES 0.49 (-20 cp).<br>Best **d8=N+**, a tactic (margin 0.49 over Kd8).<br>Fine moves: d8=N+.<br>Black threatens Rg8# (mate); more urgent: black (consequence class).<br>Asked **d8=N+**: fine. | |
| F18 | <img src="boards/F18.svg" width="200" alt="F18"> | `4r3/1p3Pk1/3p4/8/8/pPnn2q1/P7/KN4R1 w - - 0 36`<br>Asked: **fxe8=N+**<br><sub>De la Bourdonnais, Louis Mahe - McDonnell, Alexander, London m1 1834, annotator's line 34.Nb1 Kxg7 35.gxf7+ Qxg3, before 36.fxe8=N+</sub> | underpromotion by capture is the only fine move | Eval: White ES 1.00 (+656 cp).<br>Best **fxe8=N+**, a tactic (margin 0.50 over fxe8=Q).<br>Fine moves: fxe8=N+.<br>Black threatens Qxg1 (mate); more urgent: black (consequence class).<br>Asked **fxe8=N+**: fine. | |
| F19 | <img src="boards/F19.svg" width="200" alt="F19"> | `7k/5K2/6Q1/8/8/8/8/8 w - - 0 1`<br>Asked: **Qg7**<br><sub>constructed: stalemate in the null-move position (search B)</sub> | search B skips a stalemated null-move position | Eval: mate in 1 for White.<br>Best **Qh5#**, not a tactic (margin 0.00 over Qg8#).<br>Fine moves: Qh5#, Qg8#, Qh6#, Qg7#, Qf6+ and more.<br>Asked **Qg7#**: fine. | |
| F20 | <img src="boards/F20.svg" width="200" alt="F20"> | `6K1/8/5kq1/8/8/8/8/8 w - - 0 1`<br>Asked: **Kh8**<br><sub>constructed: stalemate in the null-move position (search E)</sub> | search E skips a stalemated null-move position | Eval: mate in 1 for Black.<br>Best **Kf8**, not a tactic (margin 0.00 over Kh8).<br>Fine moves: Kf8, Kh8.<br>Asked **Kh8**: fine. | |
| F21 | <img src="boards/F21.svg" width="200" alt="F21"> | `4k3/8/5q2/3N4/8/8/8/4K3 w - - 0 1`<br><sub>constructed: knight captures the exposed queen with check</sub> | only-move capture of the exposed queen with check | Eval: White ES 0.50 (+6 cp).<br>Best **Nxf6+**, a tactic (margin 0.50 over Ke2).<br>Fine moves: Nxf6+.<br>No concrete Black threat; more urgent: white (consequence class). | |
| F22 | <img src="boards/F22.svg" width="200" alt="F22"> | `k7/8/8/8/3Q3n/8/2P5/4K3 w - - 0 1`<br>Asked: **c3**<br><sub>constructed: PostM2_EM1 smoke position</sub> | refutation fork wins the queen after the quiet pawn move | Eval: mate in 10 for White.<br>Best **Qxh4**, not a tactic (margin 0.00 over Qe4+).<br>Fine moves: Qxh4, Qe4+, Qh8+, Qa4+, Qd8+ and more.<br>Black threatens Nf3+ (material); more urgent: white (consequence class).<br>Asked **c3**: blunder, loses 0.49 ES. Refutation Nf3+ Kf2 Nxd4 is a tactic (margin 0.49 over Ng2+); motifs: fork Nf3 → Ke1, Qd4 (direct); hanging piece Qd4 (direct). | |
| F23 | <img src="boards/F23.svg" width="200" alt="F23"> | `8/7k/5K2/6Q1/8/8/8/8 w - - 0 1`<br><sub>constructed: mate-in-one versus mate-in-two saturation</sub> | decisive mate-in-one position is saturated | Eval: mate in 1 for White.<br>Best **Qg7#**, not a tactic (margin 0.00 over Qg1).<br>Fine moves: Qg7#, Qg1, Qg4, Qg2, Qg3 and more.<br>No concrete Black threat; more urgent: white (consequence class). | |
| F24 | <img src="boards/F24.svg" width="200" alt="F24"> | `7k/5Q2/6K1/8/8/8/8/8 w - - 0 1`<br><sub>constructed: four legal queen mates</sub> | four equally winning mate-in-one moves | Eval: mate in 1 for White.<br>Best **Qg7#**, not a tactic (margin 0.00 over Qf8#).<br>Fine moves: Qg7#, Qf8#, Qh7#, Qe8#, Qf4 and more. | |

## 2. U fixtures — opening names (`positions.json`, `unit_fixtures`)

**Concepts:** `InputFixtures`, `GameMainline`, `PositionKey`, `OpeningNames`, `LastNamed`

Read by `PGN unit fixtures preserve mainline positions` and `PGN opening
fixtures describe current and last named positions`.

| ID | Board | Position | Fixture claims | Codebase today | Comments |
|---|---|---|---|---|---|
| U01 | <img src="boards/U01.svg" width="200" alt="U01"> | PGN `three_games.pgn` game 1, ply 6<br>`rnbqk2r/pppp1ppp/4pn2/8/1bPP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 1 4` | **transposition reaches a named position**<br>Current: Nimzo-Indian Defense; last named: Nimzo-Indian Defense (ply 6) | Matches. | |
| U02 | <img src="boards/U02.svg" width="200" alt="U02"> | PGN `three_games.pgn` game 2, ply 4<br>`rnbqkbnr/1pppppp1/p6p/8/8/P6P/1PPPPPP1/RNBQKBNR w KQkq - 0 3` | **unnamed position: last named ancestor, never a guess**<br>Current: unnamed; last named: Anderssen's Opening (ply 1) | Matches. | |
| U03 | <img src="boards/U03.svg" width="200" alt="U03"> | PGN `two_games.pgn` game 2, ply 6<br>`rnbqkb1r/ppp2ppp/4pn2/3p4/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 2 4` | **PGN default ply is the final position; comments, variations, NAGs ignored**<br>Current: Queen's Gambit Declined: Normal Defense; last named: Queen's Gambit Declined: Normal Defense (ply 6) | Matches. | |
| U04 | <img src="boards/U04.svg" width="200" alt="U04"> | PGN `fen_tag.pgn` game 1, ply 2<br>`Q7/8/8/8/8/3k4/8/4K3 w - - 1 2` | **PGN with FEN and SetUp tags; no names off the standard start**<br>Current: unnamed; last named: none | Matches. | |
| U05 | <img src="boards/U05.svg" width="200" alt="U05"> | Input `rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1`<br>Normalised `rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1` | **en-passant square without a legal capture is not part of the key**<br>Current: King's Pawn Game; last named: none | Matches. | |
| U06 | <img src="boards/U06.svg" width="200" alt="U06"> | `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1` | **start position is unnamed**<br>Current: unnamed; last named: none | Matches. | |

## 3. E fixtures — input errors (`positions.json`, `error_fixtures`)

**Concepts:** `InputFixtures`, `InputError`, `FenInput`, `PgnInput`, `AskedMove`, `CommandLine`, `StockfishProcess`, `OpeningNames`

Read by `CLI error fixtures return the required status and one diagnostic`;
E01–E11 also by `input error fixtures report the required function-level fragments`. Exit 2 is a
usage or input error, exit 3 a missing engine or data file. PGN names refer to
files in `tests/fixtures/`.

| ID | Arguments | Fixture claims | Codebase today | Comments |
|---|---|---|---|---|
| E01 | `--fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBN w KQkq - 0 1` | Exit 2; message contains “invalid FEN” | Matches. | |
| E02 | `--fen 8/8/8/8/8/8/8/8 w - - 0 1` | Exit 2; message contains “invalid FEN” | Matches. | |
| E03 | `--fen 4k3/8/8/8/8/8/4R3/4K3 w - - 0 1` | Exit 2; message contains “invalid FEN” | Matches. | |
| E04 | `--fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 --move Ke2` | Exit 2; message contains “illegal move”, “Ke2” | Matches. | |
| E05 | `--fen rnbqkb1r/ppp1pppp/5n2/3p4/3P4/5N2/PPP1PPPP/RNBQKB1R w KQkq - 2 3 --move Nd2` | Exit 2; message contains “ambiguous move”, “Nd2” | Matches. | |
| E06 | `--fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 --move Zz9` | Exit 2; message contains “cannot parse move”, “Zz9” | Matches. | |
| E07 | `--fen rnbqkbnr/ppppp1pp/5p2/7Q/4P3/8/PPPP1PPP/RNB1KBNR b KQkq - 1 2 --move Nf6` | Exit 2; message contains “illegal move”, “Nf6” | Matches. | |
| E08 | `--pgn three_games.pgn --game 3` | Exit 2; message contains “game 3”, “move 2”, “Ke3” | Matches. | |
| E09 | `--pgn two_games.pgn --game 3` | Exit 2; message contains “game 3” | Matches. | |
| E10 | `--pgn two_games.pgn --game 2 --ply 99` | Exit 2; message contains “ply 99” | Matches. | |
| E11 | `--pgn does_not_exist.pgn` | Exit 2; message contains “does_not_exist.pgn” | Matches. | |
| E12 | `--fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 --pgn two_games.pgn` | Exit 2; message contains “exactly one of --fen or --pgn” | Matches. | |
| E13 | `(no arguments)` | Exit 2; message contains “exactly one of --fen or --pgn” | Matches. | |
| E14 | `--fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 --level expert` | Exit 2; message contains “--level” | Matches. | |
| E15 | `--fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 --engine /nonexistent/stockfish` | Exit 3; message contains “engine”, “/nonexistent/stockfish” | Matches. | |
| E16 | `--fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 --openings /nonexistent/openings` | Exit 3; message contains “openings”, “/nonexistent/openings” | Matches. | |
| E17 | `--fen 7k/5Q2/6K1/8/8/8/8/8 b - - 0 10000` | Exit 2; message contains “invalid FEN”, “counter” | Matches. | |
| E18 | `--fen 7k/5Q2/6K1/8/8/8/8/8 b - - 151 40` | Exit 2; message contains “invalid FEN”, “counter” | Matches. | |

## 4. Parse fixtures — move notation (`positions.json`, `parse_fixtures`)

**Concepts:** `InputFixtures`, `FenInput`, `AskedMove`

These entries have no IDs in the JSON; P01–P13 is their order in the file, used
here only so comments can name them. Each says a SAN or UCI move in the position
resolves to the given UCI move. Read by `all move parsing fixtures resolve to
their expected UCI moves` and `FEN and SAN/UCI parse fixtures identify the same
moves`.

| # | FEN | Move → UCI | Codebase today | Comments |
|---|---|---|---|---|
| P01 | `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1` | `Nf3` → `g1f3` | Matches. | |
| P02 | `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1` | `g1f3` → `g1f3` | Matches. | |
| P03 | `r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4` | `O-O` → `e1g1` | Matches. | |
| P04 | `r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4` | `e1g1` → `e1g1` | Matches. | |
| P05 | `rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3` | `exf6` → `e5f6` | Matches. | |
| P06 | `rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3` | `e5f6` → `e5f6` | Matches. | |
| P07 | `8/P7/8/8/8/8/2k5/4K3 w - - 0 1` | `a8=N` → `a7a8n` | Matches. | |
| P08 | `8/P7/8/8/8/8/2k5/4K3 w - - 0 1` | `a7a8n` → `a7a8n` | Matches. | |
| P09 | `8/P7/8/8/8/8/2k5/4K3 w - - 0 1` | `a8=Q` → `a7a8q` | Matches. | |
| P10 | `rnbqkb1r/ppp1pppp/5n2/3p4/3P4/5N2/PPP1PPPP/RNBQKB1R w KQkq - 2 3` | `Nbd2` → `b1d2` | Matches. | |
| P11 | `rnbqkb1r/ppp1pppp/5n2/3p4/3P4/5N2/PPP1PPPP/RNBQKB1R w KQkq - 2 3` | `Nfd2` → `f3d2` | Matches. | |
| P12 | `6k1/1q3ppp/8/8/5n2/8/5PPP/4R1K1 w - - 0 1` | `Re8#` → `e1e8` | Matches. | |
| P13 | `6k1/1q3ppp/8/8/5n2/8/5PPP/4R1K1 w - - 0 1` | `Re8` → `e1e8` | Matches. | |

## 5. T fixtures — motif recognition (`tests/fixtures/motifs.json`)

**Concepts:** `MotifFixtures`, `StateMotif`, `MoveMotif`, `FirstAppearance`, `StaticExchange`, `TacticalPin`, `TpKings`, `TpNewThreat`

Each lists motif instances the detectors must report and instances they must not
report, on the position and along the line. Read by `motif fixtures exercise
every frozen include and exclude`, and compared against the reference
implementation by `tactics_oracle` (zero mismatches today). Piece letters are
the piece on the actor square at that ply; pawns have none. T33 and T34 left this file
in M2 when `removal_of_defender` stopped being a motif (IDs not reused); their
positions now live in the test `removal-of-defender labels revive T33 and reject T34`.
T42–T56 freeze `pin_tactical` (PostM2_EM1 §4.5, "tactical pin" below), written as
actor → p, then the threat's targets, then the threat ("fork on f4").

| ID | Board | Position | Fixture claims | Codebase today | Comments |
|---|---|---|---|---|---|
| T01 | <img src="boards/T01.svg" width="200" alt="T01"> | `7k/8/8/4n3/8/8/8/K3R3 w - - 0 1`<br><sub>constructed</sub> | **rook attacks undefended knight**<br>Must report: hanging piece Ne5 (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T02 | <img src="boards/T02.svg" width="200" alt="T02"> | `7k/2b5/8/4n3/8/8/8/K3R3 w - - 0 1`<br><sub>constructed</sub> | **attacked knight is defended by bishop**<br>Must not report: hanging piece (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T03 | <img src="boards/T03.svg" width="200" alt="T03"> | `4k3/8/4n3/8/8/8/8/K3R3 w - - 0 1`<br><sub>constructed</sub> | **rook pins knight to king**<br>Must report: pin absolute Re1 → e6, e8 (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T04 | <img src="boards/T04.svg" width="200" alt="T04"> | `8/6k1/8/4n3/8/8/1B6/K7 w - - 0 1`<br><sub>constructed</sub> | **bishop pins knight to king**<br>Must report: pin absolute Bb2 → e5, g7 (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T05 | <img src="boards/T05.svg" width="200" alt="T05"> | `4q2k/8/4n3/8/8/8/8/K3R3 w - - 0 1`<br><sub>constructed</sub> | **rook lines up pieces with a queen behind the knight**<br>Must not report: pin absolute (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T06 | <img src="boards/T06.svg" width="200" alt="T06"> | `k3r3/8/4n3/8/8/8/8/K3R3 w - - 0 1`<br><sub>constructed</sub> | **rook lines up pieces with a rook behind the knight**<br>Must not report: pin absolute (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T07 | <img src="boards/T07.svg" width="200" alt="T07"> | `4q2k/8/4p3/8/8/8/8/K3R3 w - - 0 1`<br><sub>constructed</sub> | **pawn is pinned in front of queen**<br>Must report: pin relative Re1 → e6, e8 (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T08 | <img src="boards/T08.svg" width="200" alt="T08"> | `7k/6r1/8/4n3/8/8/1B6/K7 w - - 0 1`<br><sub>constructed</sub> | **bishop pins knight in front of rook**<br>Must report: pin relative Bb2 → e5, g7 (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T09 | <img src="boards/T09.svg" width="200" alt="T09"> | `4n2k/8/4q3/8/8/8/8/K3R3 w - - 0 1`<br><sub>constructed</sub> | **queen behind a more valuable front piece**<br>Must not report: pin relative (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T10 | <img src="boards/T10.svg" width="200" alt="T10"> | `4n2k/8/4n3/8/8/8/8/K3R3 w - - 0 1`<br><sub>constructed</sub> | **equal knights do not make a relative pin**<br>Must not report: pin relative (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T11 | <img src="boards/T11.svg" width="200" alt="T11"> | `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`<br><sub>Lasker, Emanuel - Schlechter, Carl, Wch10 1910, before 1.e4</sub> | **both kings have an opposing rook or queen on their first rank**<br>Must not report: back rank weakness (white, ply 0); back rank weakness (black, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T12 | <img src="boards/T12.svg" width="200" alt="T12"> | `4k2r/8/8/8/8/8/PPP5/1K6 b - - 0 1`<br><sub>constructed</sub> | **white king's three front squares are occupied**<br>Must report: back rank weakness Kb1 (black, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T13 | <img src="boards/T13.svg" width="200" alt="T13"> | `6k1/6pp/8/8/8/8/8/R3K3 w - - 0 1`<br><sub>constructed</sub> | **black king has luft on f7**<br>Must not report: back rank weakness (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T14 | <img src="boards/T14.svg" width="200" alt="T14"> | `4k2r/8/8/8/8/8/P1P5/1K6 b - - 0 1`<br><sub>constructed</sub> | **white king has luft on b2**<br>Must not report: back rank weakness (black, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T15 | <img src="boards/T15.svg" width="200" alt="T15"> | `7k/8/8/2n1n3/3q4/8/8/2R1RK2 w - - 0 1`<br><sub>constructed</sub> | **queen defends both attacked knights**<br>Must report: overloaded defender Qd4 → c5, e5 (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T16 | <img src="boards/T16.svg" width="200" alt="T16"> | `7k/8/8/2n1n3/3b4/8/8/2R1RK2 w - - 0 1`<br><sub>constructed</sub> | **bishop defends both attacked knights**<br>Must report: overloaded defender Bd4 → c5, e5 (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T17 | <img src="boards/T17.svg" width="200" alt="T17"> | `7k/8/1b6/2n1n3/8/8/8/2R1RK2 w - - 0 1`<br><sub>constructed</sub> | **defender only protects one attacked knight**<br>Must not report: overloaded defender (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T18 | <img src="boards/T18.svg" width="200" alt="T18"> | `1b5k/8/8/2n1n3/3q4/8/8/2R1RK2 w - - 0 1`<br><sub>constructed</sub> | **second defender survives removal of queen**<br>Must not report: overloaded defender (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T19 | <img src="boards/T19.svg" width="200" alt="T19"> | `r3k3/pQB5/8/8/8/8/8/K7 b - - 0 1`<br><sub>constructed</sub> | **rook's file exit is blocked and rank exits are attacked**<br>Must report: trapped piece Ra8 (white, ply 0)<br>Must not report: hanging piece (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T20 | <img src="boards/T20.svg" width="200" alt="T20"> | `r7/2k5/8/8/8/8/8/Q3K3 b - - 0 1`<br><sub>constructed</sub> | **corner rook can escape to b8**<br>Must not report: trapped piece (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T21 | <img src="boards/T21.svg" width="200" alt="T21"> | `6k1/8/6q1/5N2/1B6/8/8/K7 w - - 0 1`<br>Line: Ne7+<br><sub>constructed</sub> | **knight forks queen and king from a safe square**<br>Must report: fork Ne7 → g6, g8 (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T22 | <img src="boards/T22.svg" width="200" alt="T22"> | `6k1/8/6q1/2b2N2/8/8/8/K7 w - - 0 1`<br>Line: Ne7+<br><sub>constructed</sub> | **forking knight can be taken on its landing square**<br>Must not report: fork (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T23 | <img src="boards/T23.svg" width="200" alt="T23"> | `4n2k/8/4q3/8/R7/8/8/KB6 w - - 0 1`<br>Line: Re4<br><sub>constructed</sub> | **rook skewers queen in front of knight**<br>Must report: skewer Re4 → e6, e8 (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T24 | <img src="boards/T24.svg" width="200" alt="T24"> | `4q2k/8/4n3/8/R7/8/8/KB6 w - - 0 1`<br>Line: Re4<br><sub>constructed</sub> | **front knight is no more valuable than rear queen**<br>Must not report: skewer (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T25 | <img src="boards/T25.svg" width="200" alt="T25"> | `K7/8/8/8/8/8/8/R2B3k w - - 0 1`<br>Line: Be2+<br><sub>constructed</sub> | **bishop vacates rook's line to king**<br>Must report: discovered check Ra1 → h1 (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T26 | <img src="boards/T26.svg" width="200" alt="T26"> | `K7/8/8/8/8/8/7k/R2B4 w - - 0 1`<br>Line: Be2<br><sub>constructed</sub> | **bishop vacates rook's line but king is off it**<br>Must not report: discovered check (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T27 | <img src="boards/T27.svg" width="200" alt="T27"> | `4k2r/8/8/8/3P4/8/8/BK6 w - - 0 1`<br>Line: d5<br><sub>constructed</sub> | **pawn uncovers bishop attack on a more valuable rook**<br>Must report: discovered attack Ba1 → h8 (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T28 | <img src="boards/T28.svg" width="200" alt="T28"> | `4k2n/7r/8/8/3P4/8/8/BK6 w - - 0 1`<br>Line: d5<br><sub>constructed</sub> | **unmasked bishop attacks a defended equal-value knight**<br>Must not report: discovered attack (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T29 | <img src="boards/T29.svg" width="200" alt="T29"> | `4k3/8/8/8/4B3/8/8/K3R3 w - - 0 1`<br>Line: Bg6+<br><sub>constructed</sub> | **bishop move reveals rook check as double check**<br>Must report: double check Bg6 → e8 (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T30 | <img src="boards/T30.svg" width="200" alt="T30"> | `4k3/8/8/8/4B3/8/8/K3R3 w - - 0 1`<br>Line: Bf3+<br><sub>constructed</sub> | **bishop opens a single rook check**<br>Must not report: double check (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T31 | <img src="boards/T31.svg" width="200" alt="T31"> | `7k/6pp/8/8/2B5/8/Q7/1R2K3 w - - 0 1`<br>Line: Rb8#<br><sub>constructed</sub> | **rook delivers back-rank mate**<br>Must report: back rank mate Rb8 → h8 (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T32 | <img src="boards/T32.svg" width="200" alt="T32"> | `7k/7p/8/6Q1/8/8/8/1R2K3 w - - 0 1`<br>Line: Rb8#<br><sub>constructed</sub> | **mate remains but one front square is empty**<br>Must not report: back rank mate (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T35 | <img src="boards/T35.svg" width="200" alt="T35"> | `7k/3P4/8/8/8/8/8/K7 w - - 0 1`<br>Line: d8=Q+<br><sub>constructed</sub> | **pawn promotes to queen**<br>Must report: promotion Qd8 (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T36 | <img src="boards/T36.svg" width="200" alt="T36"> | `7k/8/3P4/8/8/8/8/K7 w - - 0 1`<br>Line: d7<br><sub>constructed</sub> | **pawn advances without promoting**<br>Must not report: promotion (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T37 | <img src="boards/T37.svg" width="200" alt="T37"> | `4k3/7r/8/8/8/8/PPP5/1K6 b - - 0 1`<br><sub>constructed</sub> | **white king has occupied front squares and no home-rank rook or queen**<br>Must report: back rank weakness Kb1 (black, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T38 | <img src="boards/T38.svg" width="200" alt="T38"> | `6k1/5ppp/8/8/8/8/8/K1B5 w - - 0 1`<br><sub>constructed</sub> | **opponent has no rook or queen**<br>Must not report: back rank weakness (white, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T39 | <img src="boards/T39.svg" width="200" alt="T39"> | `r1bqkbnr/1ppp1ppp/p1n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4`<br>Line: Bxc6 dxc6<br><sub>constructed</sub> | **equal-value recapture is a trade in progress**<br>Must not report: hanging piece (black, ply 0); hanging piece (black, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T40 | <img src="boards/T40.svg" width="200" alt="T40"> | `7k/8/8/8/8/5b2/4q3/K3R3 b - - 0 1`<br>Line: Bh1<br><sub>constructed</sub> | **black bishop abandons its queen's defense**<br>Must report: hanging piece Qe2 (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T41 | <img src="boards/T41.svg" width="200" alt="T41"> | `3k4/8/8/8/8/8/8/R3K3 w Q - 0 1`<br>Line: O-O-O+<br><sub>constructed</sub> | **castling rook gives the check but is not a discovered checker**<br>Must not report: discovered check (white, ply 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T42 | <img src="boards/T42.svg" width="200" alt="T42"> | `2r3k1/p4pp1/Pp2pn1p/3nN1q1/3P1P1R/3B2P1/1P2Q1KP/8 b - - 0 30`<br>Line: Qxh4<br><sub>lichess:obWQv</sub> | **C09 fork restriction, plus capture threats**<br>Must report: tactical pin Nd5 → h4, e2, g2, fork on f4 (black, ply 0); tactical pin Nd5 → h4, f4, capture on f4 (black, ply 0); tactical pin Nd5 → g3, e2, g2, fork on f4 (black, ply 1); tactical pin Nd5 → g3, f4, capture on f4 (black, ply 1); tactical pin Qh4 → f4, d4, capture on d4 (black, ply 1)<br>Must not report: tactical pin (white, ply 0 and 1) | Matches: every listed instance reported, none of the excluded ones. | |
| T43 | <img src="boards/T43.svg" width="200" alt="T43"> | `8/7p/6k1/3N4/8/8/4q3/K7 w - - 0 1`<br><sub>constructed</sub> | **fork already exists with the pawn on the board**<br>Must not report: tactical pin (white or black, ply 0) | Matches: none of the excluded ones. | |
| T44 | <img src="boards/T44.svg" width="200" alt="T44"> | `8/6k1/8/8/Q2p3b/8/K7/B7 w - - 0 1`<br><sub>constructed</sub> | **absolutely pinned pawn blocks a fork route but is skipped**<br>Must not report: tactical pin (white or black, ply 0) | Matches: none of the excluded ones. | |
| T45 | <img src="boards/T45.svg" width="200" alt="T45"> | `8/8/6k1/3Np1p1/8/8/4q3/K7 w - - 0 1`<br><sub>constructed</sub> | **second pawn still guards the fork square**<br>Must not report: tactical pin (white or black, ply 0) | Matches: none of the excluded ones. | |
| T46 | <img src="boards/T46.svg" width="200" alt="T46"> | `8/8/6k1/3N2p1/8/8/4q3/K7 b - - 0 1`<br><sub>constructed</sub> | **Y to move: the pawn on g5 still holds the fork square for X**<br>Must report: tactical pin Nd5 → g5, e2, g6, fork on f4 (white, ply 0)<br>Must not report: tactical pin (black, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T47 | <img src="boards/T47.svg" width="200" alt="T47"> | `q7/8/k7/8/p7/8/7K/1R6 w - - 0 1`<br><sub>constructed</sub> | **pawn blocks a rook skewer on the a-file**<br>Must report: tactical pin Rb1 → a4, a6, a8, skewer on a1 (white, ply 0)<br>Must not report: tactical pin (black, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T48 | <img src="boards/T48.svg" width="200" alt="T48"> | `k7/8/p7/8/N7/8/8/R6K w - - 0 1`<br><sub>constructed</sub> | **pawn blocks discovered and double check**<br>Must report: tactical pin Na4 → a6, a8, discovered check on b2 (white, ply 0); tactical pin Na4 → a6, a8, double check on b6 (white, ply 0)<br>Must not report: tactical pin (black, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T49 | <img src="boards/T49.svg" width="200" alt="T49"> | `q6k/8/p7/8/N7/8/7K/R7 w - - 0 1`<br><sub>constructed</sub> | **pawn blocks a discovered attack on the queen**<br>Must report: tactical pin Na4 → a6, a8, discovered attack on b2 (white, ply 0)<br>Must not report: tactical pin (black, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T50 | <img src="boards/T50.svg" width="200" alt="T50"> | `4k3/1N1ppp2/4N3/8/7p/8/8/K6R w - - 0 1`<br><sub>constructed</sub> | **pawn blocks a back-rank mate on h8**<br>Must report: tactical pin Rh1 → h4, e8, back-rank mate on h8 (white, ply 0); tactical pin Rh1 → h4, e8, checkmate on h8 (white, ply 0)<br>Must not report: tactical pin (black, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T51 | <img src="boards/T51.svg" width="200" alt="T51"> | `r6k/P7/8/8/8/8/8/7K w - - 0 1`<br><sub>constructed</sub> | **rook blocks the promotion square**<br>Must report: tactical pin a7 → a8, promotion on a8 (white, ply 0)<br>Must not report: tactical pin (black, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T52 | <img src="boards/T52.svg" width="200" alt="T52"> | `7k/8/5K2/8/6p1/8/8/6Q1 w - - 0 1`<br><sub>constructed</sub> | **pawn blocks a queen mate on g7**<br>Must report: tactical pin Qg1 → g4, h8, checkmate on g7 (white, ply 0)<br>Must not report: tactical pin (black, ply 0) | Matches: every listed instance reported, none of the excluded ones. | |
| T53 | <img src="boards/T53.svg" width="200" alt="T53"> | `7k/p7/8/8/8/8/8/K6R b - - 0 1`<br><sub>constructed</sub> | **no pin_tactical for X while Y is in check**<br>Must not report: tactical pin (white or black, ply 0) | Matches: none of the excluded ones. | |
| T54 | <img src="boards/T54.svg" width="200" alt="T54"> | `4k3/8/8/8/3pP3/8/8/4K3 b - e3 0 1`<br><sub>constructed</sub> | **removing the pawn that just made a double step clears en passant**<br>Must not report: tactical pin (white or black, ply 0). Guards the §4.5 rule that B−p has no en passant square once the double-stepped pawn is removed (an ASan/Debug build aborts otherwise) | Matches: none of the excluded ones. | |
| T55 | <img src="boards/T55.svg" width="200" alt="T55"> | `k7/8/K7/8/8/r2R4/8/8 w - - 0 1`<br><sub>constructed</sub> | **no pin_tactical for X while X is in check**<br>Must not report: tactical pin (white or black, ply 0). Without the §4.5 check rule, Rd3 → a3, a8, checkmate on d8 (white) would be reported: Rd8 mates once the checking rook is gone | Matches: none of the excluded ones (the detector before the gate fix reported Rd3 → a3, a8, checkmate on d8). | |
| T56 | <img src="boards/T56.svg" width="200" alt="T56"> | `2k1q3/3rR3/2K5/8/8/8/2pR4/8 w - - 0 1`<br><sub>constructed</sub> | **removing the rook would discover check on X**<br>Must not report: tactical pin (white, ply 0). Removing Rd7 opens Qe8 → c6; without the §4.5 rule Re7 → d7, c8, checkmate on e8 would be reported | Matches: none of the excluded ones (the detector before the gate fix reported Re7 → d7, c8, checkmate on e8). | |

## 6. C fixtures — tactic contributions and labels (`tests/fixtures/tactics.json`)

**Concepts:** `ContributionFixtures`, `Contribution`, `CoDirect`, `CoIndirect`, `CoNotUsed`, `TacticLabel`, `TacticSequence`, `TacticalPin`, `StateMotif`, `MoveMotif`

Hand-labelled tactic lines: for each motif on the line, whether it contributed to
the tactic (DEFINITIONS §4.5), plus required or forbidden tactic labels. Read by
`tactic fixtures exercise contributions and labels`, which passes on all of them
(since 2026-10-21, after the Captain's checkpoint 2 decisions; PostM2_EM1 §15).

| ID | Board | Position | Fixture claims | Codebase today | Comments |
|---|---|---|---|---|---|
| C01 | <img src="boards/C01.svg" width="200" alt="C01"> | `rn1qk2r/ppp2ppp/5n2/3P2B1/1b2p1b1/2N5/PP3PPP/R2QKBNR w KQkq - 1 8`<br>Line: Qa4+ Nc6 dxc6 Bxc3+ bxc3<br><sub>lichess:SPCxc</sub> | **Queen check-fork guards the knight-capture square**<br>Contributions: pin relative Bg5 → f6, d8 (white, ply 0) **none**; pin relative Qa4 → a7, a8 (white, ply 1) **none**; fork Qa4 → b4, e8 (white, ply 1) **direct**; hanging piece Nc6 (white, ply 2) **direct**; pin absolute Qa4 → c6, e8 (white, ply 2) **direct**; trapped piece Nc6 (white, ply 2) **direct**<br>Label forbidden: removal of defender | Matches: every contribution and label as hand-labelled. | 2026-10-21 Deputy: the a7→a8 pin was labelled indirect until checkpoint 2; the Captain set it to none (the pin does nothing; Qa4's guard of c6 is the queen's own work). |
| C02 | <img src="boards/C02.svg" width="200" alt="C02"> | `5rk1/pp3pp1/7p/n2q4/4N1b1/3B4/P2Q1PPP/2R3K1 w - - 0 20`<br>Line: Nf6+ gxf6 Bh7+ Kxh7 Qxd5<br><sub>lichess:NNFmu</sub> | **Discovered attack wins the exposed queen after checks**<br>Contributions: discovered attack Qd2 → d5 (white, ply 3) **direct**; hanging piece Qd5 (white, ply 4) **direct**<br>Label forbidden: removal of defender | Matches: every contribution and label as hand-labelled. | |
| C03 | <img src="boards/C03.svg" width="200" alt="C03"> | `r1b1k2r/ppp2p1p/3p1Q2/2q1n2P/4P3/3PN3/PPPK2P1/R4B1R b kq - 0 18`<br>Line: Qxe3+ Kxe3 Ng4+ Kf4 Nxf6<br><sub>lichess:AAdbR</sub> | **Queen sacrifice clears the knight fork**<br>Contributions: fork Ng4 → e3, f6 (black, ply 3) **direct**; hanging piece Qf6 (black, ply 4) **direct**<br>Label forbidden: removal of defender | Matches: every contribution and label as hand-labelled. | |
| C04 | <img src="boards/C04.svg" width="200" alt="C04"> | `4R3/1pk1bQpp/2p3n1/8/3r4/1P2N1q1/1PP5/2B2KN1 b - - 0 20`<br>Line: Rf4+ Qxf4+ Qxf4+<br><sub>lichess:wKuEl</sub> | **Rook fork forces the queen exchange**<br>Contributions: fork Rf4 → f1, f7 (black, ply 1) **direct**; hanging piece Qf4 (black, ply 2) **direct**<br>Label forbidden: removal of defender | Matches: every contribution and label as hand-labelled. | |
| C05 | <img src="boards/C05.svg" width="200" alt="C05"> | `1n1r4/2R2pk1/p4r1p/2pN2p1/1p1b4/1P3B1P/P1P2PP1/3R2K1 b - - 11 31`<br>Line: Rxf3 gxf3 Rxd5<br><sub>lichess:xeP1l</sub> | **Capture of the overloaded bishop wins the knight**<br>Contributions: pin absolute Bd4 → f2, g1 (black, ply 0) **none**; overloaded defender Bf3 → f2, d5 (black, ply 0) **direct**; hanging piece Nd5 (black, ply 2) **direct**<br>Label required: removal of defender Rf3 → f3, d5 (black, ply 1) | Matches: every contribution and label as hand-labelled. | |
| C06 | <img src="boards/C06.svg" width="200" alt="C06"> | `5rk1/pp3ppp/2p2n2/q7/2n2Q2/P1N2B1P/1PP2PP1/3R2K1 b - - 0 20`<br>Line: Nxb2 Rb1 Qxc3<br><sub>lichess:xeZ2j</sub> | **Knight capture removes the pawn guarding a knight**<br>Contributions: overloaded defender b2 → a3, c3 (black, ply 0) **direct**; hanging piece Nc3 (black, ply 2) **direct**<br>Label required: removal of defender Nb2 → b2, c3 (black, ply 1) | Matches: every contribution and label as hand-labelled. | |
| C07 | <img src="boards/C07.svg" width="200" alt="C07"> | `2r3k1/p2q1pp1/1p5p/4n3/Q3P3/6PP/5P1K/2R5 w - - 3 33`<br>Line: Qxd7 Nxd7 Rxc8+<br><sub>lichess:SLgPd</sub> | **Queen sacrifice removes the rook's defender**<br>Contributions: overloaded defender Qd7 → a7, c8 (white, ply 0) **direct**; hanging piece Rc8 (white, ply 2) **direct**<br>Label required: removal of defender Qd7 → d7, c8 (white, ply 1) | Matches: every contribution and label as hand-labelled. | |
| C08 | <img src="boards/C08.svg" width="200" alt="C08"> | `3r2k1/4rppb/1BBp3p/b3p3/P1P4q/1Q1P3P/5PP1/R4RK1 b - - 0 22`<br>Line: Rb8 c5 dxc5 Qd5 Bxb6<br><sub>lichess:9qopA</sub> | **Overloaded queen leaves the bishop on b6 exposed**<br>Contributions: overloaded defender Qb3 → d3, b6 (black, ply 0) **direct**; pin relative Rb8 → b6, b3 (black, ply 1) **direct**; overloaded defender Qb3 → d3, b6 (black, ply 2) **direct**; hanging piece Bb6 (black, ply 4) **direct**<br>Label required: removal of defender c5 → c5, b6 (black, ply 3) | Matches: every contribution and label as hand-labelled. | 2026-10-21 Deputy: label changed from forbidden to required at checkpoint 2 (Captain; Lichess tags `capturingDefender`); title fixed: the queen is overloaded, not the bishop. |
| C09 | <img src="boards/C09.svg" width="200" alt="C09"> | `2r3k1/p4pp1/Pp2pn1p/3nN1q1/3P1P1R/3B2P1/1P2Q1KP/8 b - - 0 30`<br>Line: Qxh4 gxh4 Nxf4+ Kf3 Nxe2<br><sub>lichess:obWQv</sub> | **Queen and knight overwhelm the pawn's two duties**<br>Contributions: pin absolute Qg5 → g3, g2 (black, ply 0) **none**; overloaded defender g3 → f4, h4 (black, ply 0) **direct**; tactical pin Nd5 → h4, e2, g2, fork on f4 (black, ply 0) **direct**; tactical pin Nd5 → g3, e2, g2, fork on f4 (black, ply 1) **direct**; fork Nf4 → e2, g2 (black, ply 3) **direct**; hanging piece Qe2 (black, ply 4) **direct**<br>Label required: removal of defender h4 → h4, f4 (black, ply 1) | Matches: every contribution and label as hand-labelled. | 2026-10-21 Deputy: checkpoint 2 (Captain): the Qg5 pin set to none; label changed from forbidden to required. The Captain reads the g3 pawn as held by Nd5's threat Nxf4+ (a tactical pin); the two `pin_tactical` rows were added when the tag was frozen (TERMINOLOGY `pin_tactical`). Other `pin_tactical` instances on C fixtures are not listed (exempt from the exact set). |
| C10 | <img src="boards/C10.svg" width="200" alt="C10"> | `3r1r1k/1pp1q1pp/8/3B1p2/1pPn1P2/b3B3/P3QP2/1K5R w - - 0 29`<br>Line: Rxh7+ Kxh7 Qh5#<br><sub>lichess:Fvug7</sub> | **Sacrificial rook check leads to mate**<br>Contributions: hanging piece Nd4 (white, ply 0) **none**; pin absolute Rh1 → h7, h8 (white, ply 0) **direct**; pin relative Rh7 → g7, e7 (white, ply 1) **none**; hanging piece Nd4 (white, ply 2) **none**<br>Label forbidden: removal of defender | Matches: every contribution and label as hand-labelled (since the 2026-10-21 rule change: Black's Kxh7 no longer makes the h7 pin direct). | |
| C11 | <img src="boards/C11.svg" width="200" alt="C11"> | `r1b2rk1/1p3ppp/p7/4p3/4nq2/PNP5/2P2QBP/R3K2R w KQ - 0 19`<br>Line: Qxf4 exf4 Bxe4<br><sub>lichess:7hB3U</sub> | **Queen trade removes the knight's defender**<br>Contributions: hanging piece Ne4 (white, ply 2) **direct**; pin relative Be4 → b7, a8 (white, ply 3) **none**<br>Label required: removal of defender Qf4 → f4, e4 (white, ply 1) | Matches: every contribution and label as hand-labelled. | |
| C12 | <img src="boards/C12.svg" width="200" alt="C12"> | `4k3/4p3/3q4/8/8/6B1/8/4R1K1 w - - 0 1`<br>Line: Bxd6 e5 Bb4 e4 Kf2 e3+<br><sub>constructed; margin 0.501</sub> | **Pinned pawn cannot recapture the bishop on d6**<br>Contributions: hanging piece Qd6 (white, ply 0) **direct**; pin absolute Re1 → e7, e8 (white, ply 0) **indirect**; pin absolute Re1 → e5, e8 (white, ply 2) **none**; pin absolute Re1 → e4, e8 (white, ply 4) **none**; pin absolute Re1 → e3, e8 (white, ply 6) **none** | Matches: every contribution and label as hand-labelled (since the 2026-10-21 rule change: Black's pawn moves no longer make the pins direct). | |
| C13 | <img src="boards/C13.svg" width="200" alt="C13"> | `4k3/4n3/8/5q2/8/7B/8/K3R3 w - - 0 1`<br>Line: Bxf5 Kd8 Be4 Kd7 Kb2 Kd6<br><sub>constructed; margin 0.5</sub> | **Pinned knight cannot recapture the bishop on f5**<br>Contributions: hanging piece Qf5 (white, ply 0) **direct**; pin absolute Re1 → e7, e8 (white, ply 0) **indirect** | Matches: every contribution and label as hand-labelled (since the 2026-10-21 rule change: Kd8 no longer makes the pin direct). | |
| C14 | <img src="boards/C14.svg" width="200" alt="C14"> | `2k5/2p5/3q4/4B3/8/8/8/K1R5 w - - 0 1`<br>Line: Bxd6 Kd7 Ba3 c5 Re1 Kc6<br><sub>constructed; margin 0.5</sub> | **Pinned pawn cannot recapture the bishop on d6 from c7**<br>Contributions: hanging piece Qd6 (white, ply 0) **direct**; pin absolute Rc1 → c7, c8 (white, ply 0) **indirect** | Matches: every contribution and label as hand-labelled (since the 2026-10-21 rule change: Kd7 no longer makes the pin direct). | |
| C15 | <img src="boards/C15.svg" width="200" alt="C15"> | `4k3/8/3b4/2P1n3/8/8/8/K3R3 w - - 0 1`<br>Line: cxd6 Kd7 Rxe5 Kxd6 Rh5 Ke6<br><sub>constructed, after seed T33; margin 0.499</sub> | **T33 revival: pawn removes the bishop defending a knight**<br>Contributions: hanging piece Bd6 (white, ply 0) **direct**; pin absolute Re1 → e5, e8 (white, ply 0) **direct**; trapped piece Ne5 (white, ply 1) **direct**; hanging piece Ne5 (white, ply 2) **direct**<br>Label required: removal of defender d6 → d6, e5 (white, ply 1) | Matches: every contribution and label as hand-labelled. | |
| C16 | <img src="boards/C16.svg" width="200" alt="C16"> | `k3r3/8/3b4/2P1n3/8/8/8/K3R3 w - - 0 1`<br>Line: cxd6 Kb7 Re2 Kc6 Kb1 Kxd6<br><sub>constructed, after seed T34; margin 0.499</sub> | **T34 near miss: the rook still defends the knight**<br>Contributions: hanging piece Bd6 (white, ply 0) **direct**; pin relative Re1 → e5, e8 (white, ply 0) **none**; pin relative Re2 → e5, e8 (white, ply 3) **none**<br>Label forbidden: removal of defender | Matches: every contribution and label as hand-labelled. | 2026-10-21 Deputy: the Re1 pin was labelled direct until checkpoint 2; the Captain set it to none (Re2 regroups without using the pin). |

## 7. Data files

**Concepts:** `InputFixtures`, `GameMainline`, `PgnInput`, `OpeningNames`, `PuzzleSample`, `MotifFixtures`, `ContributionFixtures`, `ReferenceImplementation`

| File | Content | Read by | Comments |
|---|---|---|---|
| `tests/fixtures/two_games.pgn` | Two short games | F15, E09, E10, E12, PGN input tests | |
| `tests/fixtures/three_games.pgn` | Three games; game 3 has an illegal second move | U fixtures, E08, PGN input tests | |
| `tests/fixtures/annotated.pgn` | Five games with comments, NAGs and headers | PGN reader tests, `pgntool` tests | |
| `tests/fixtures/fen_tag.pgn` | A game starting from a `FEN`/`SetUp` tag | U04 | |
| `tests/fixtures/puzzles/test.csv` | The committed **test** split of the Lichess puzzle sample: 9,300 puzzles from the 2026-10-05 Lichess export (CC0), columns `PuzzleId, FEN, Moves, Themes, Stratum`, see DEFINITIONS §5.1 | `tactics_puzzles` (recall, Lichess agreement), `tactics_oracle` | |

## 8. P fixtures — positional features and changes (`tests/fixtures/positional.json`, planned)

**Concepts:** `PositionalFixtures`, `PawnStructure`, `KingSafety`, `PieceActivity`, `SpaceDevelopment`, `MaterialBalance`, `PositionalChange`

Planned. `features` entries (P01…) give a position with features that must or must not
be reported; `lines` entries (PL01…) give a line with changes that must or must not be
reported. Every tag gets a positive and a near miss, and at least ten positions come from
the Captain's books, cited by game. Rows arrive with the fixtures, hand-labelled before the
detectors run; the Captain reviews them at the checkpoint after the first batch. New F
rows (`positional_include`, game counts) are added to section 1 then.
