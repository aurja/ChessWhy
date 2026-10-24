# PostM4_EM3 receipt — tempo, initiative and the impact rating

Date: 2026-10-24. Machine: `debian-home` (i7-8565U, 15 GiB, Debian 13). Branch
`postm4-em3`. Contract: `Planning/milestones/PostM4_EM3.md` (acceptance §16, contract
changes §17). Terms: `Planning/DBC/DEFINITIONS.md` §3.6.

## Delivered

- **Tempo value** (§6.1, `engine`): at the root for the side to move and after every
  detailed move for the opponent; labels tempo-dependent (> 0.10), non-tempo (< 0.05) and
  zugzwang (< 0). The root label decides whether the position is a tempo-dependent
  position.
- **Tempo ledger** (§6.2): every walked ply of every detailed line is labelled
  `threatening`, `neither`, `counter_threat` or `pure_reply` from a threat search (null
  move) and a reply search, both at budget `L` (30,000 nodes at `standard`, the timing
  fallback of §17); a ply answered by a pure reply gains a tempo. Skipped in check, where
  the null move is unreliable, and on lines that are not detailed.
- **Initiative runs** (§6.3, `rule`): holder, plies, tempi, ending (`spent`, `contested`,
  `runs`, `unfinished`) and outcome (`converted`, `evaporated`).
- **Use test** (§6.4–6.6): favoured side, net change and the eleven-step use test; a move
  report keeps only net changes in use.
- **Impact order** (§6.7): positional order (dynamic first in a tempo-dependent
  position) and the move report's group order by consequence rank with both base orders.
- **Report v7** (§7): `schema/report.v7.schema.json`, `schema/moves.v7.schema.json`; the
  move report engine header and the Budget text row carry `L`. Text templates (§12)
  print the tempo value, ledger, tempo gained and initiative rows and the use-test marks.
- **Checker** (WP5): `tools/validate_report.py` re-implements §6.1–6.7 in python-chess
  and the v7 projection.
- **Fixtures**: F40–F44, PU01–PU18, F25's move report, F41 as confirmed at checkpoint 2.
- **Timing check** moved into one serial test, `cli_timing` (15 s per F fixture, no
  other test running); the F fixture tests run in parallel.
- **Side tooling** (Captain's requests, not milestone scope): long test drivers print
  progress lines; `tools/run_tests.py` streams them, writes
  `<build-dir>/test_progress.json`, and `.omp/extensions/test-progress.ts` shows them in
  omp.
- **Docs**: README, DBC files, `CURRENT_STATE.md`, this receipt.

## Verification

| # | Check | Result |
|---|---|---|
| 1 | Release and ASan/UBSan builds; CTests | On the gate as redefined by the test redesign (below): Release clean build 0 warnings; `unit\|engine\|games\|corpus` 205/205 in 376 s, then `timing` alone pass in 261 s. ASan/UBSan (Debug `-O1`) build 0 warnings; `unit\|sanitize` 168/168 in 101 s, no `runtime error` or `AddressSanitizer` line. Before the redesign: Release 204/204 in 1,359 s; the `-O0` ASan run of every label did not finish within its timeouts |
| 2 | §6.1–6.3 | WP1 unit tests (label boundaries 99/100, 200/201, −1/0; ledger labels; every run ending; both outcome arms) pass. F40–F43 as in §11.1 with F41 as approved at checkpoint 2; F44 chosen (F28's root, non-tempo position) and passing |
| 3 | §6.4–6.6 | PU01–PU18 pass; F25's move report keeps `doubled_pawns` (Black, c-file) and the loss of White's `bishop_pair` |
| 4 | §6.7 | Move report unit tests of both base orders and rank sorting pass; every F move report document's `impact_order` passes the projection check |
| 5 | §7 | Every F full report validates against `report.v7.schema.json` and passes the semantic checks; every F move report document validates against `moves.v7.schema.json` and passes the projection check (`report_schema`); 30 mutants (24 semantic, 6 projection) each fail |
| 6 | Timing and identity | `cli_timing` passes: slowest F fixture F12 12.0 s, F32 10.5 s (limit 15 s); F32 `--moves playable` 27.93 s (limit 30 s), `--moves all` 68.46 s (limit 90 s); `cli_rerun_identity` byte-identical for all four outputs |
| 7 | Facts first | Every tempo row prints a report field (render golden tests); tempo, ledger and run fields carry `engine` or `rule` |
| 8 | Gates | clang-format and `clang-tidy --quiet -p build/release` 0 diagnostics on the 17 changed C++ units; Ruff and basedpyright 0/0 on `tools/run_tests.py`, `tools/validate_report.py`, `tests/checker/unit_tests.py`; no shell script changed; `check_dbc` ok |
| 9 | Docs | README (tempo, ledger, initiative, use test, impact order, v7, checker relations, `cli_timing`), DBC, `CURRENT_STATE.md`, this receipt |

The first clang-tidy run found 21 diagnostics in the changed units, one a real
use-after-move: `analyse` read `threats.null_move_reliable` for the asked moves after
moving `threats` into the report (harmless only because a moved-from `optional<bool>`
keeps its value). Fixed in `bcf47a3`; the Release gate above ran after it.

Smoke (`--fen '1br3k1/5pp1/p3p2p/1bBn4/N7/1P3PP1/5R1P/2R3K1 b - - 0 1' --moves Bxa4,Ba7`,
10.2 s): "Tempo value: 29.6% for Black (tempo-dependent position)"; under Bxa4 "Tempo:
White gains a tempo with Bxa7 (ply 6)" and "Initiative: White from ply 6, 1 tempo;
unfinished at ply 7; evaporated"; Ba7 has no tempo gained row.

## Test redesign

Approved by the Captain on 2026-10-24 (sanitizers check safety, Release checks the facts;
contract §17). Fixture tests build each report once (`cli::build_report`) and write all four
outputs from it; the 106 saved JSON reports are byte-identical to the old harness. The
oracles run the oracle sample (every 31st puzzle, 300 of 9,300) under `unit` and the whole
corpus under the new `corpus` label; `cli_timing` has its own `timing` label; 15 F fixtures
carry `sanitize`, which with `unit` is the ASan/UBSan run; ASan builds at `-O1`; engine CLI
tests take two CTest slots. AGENTS.md gate rows updated with her approved text.

What the redesign gives up, as the review noted: full and move JSON used to come from two
analyses, so the projection check also compared reruns on all 53 runs; determinism is now
covered by `cli_rerun_identity` (six fixtures plus the smoke text). ASan covers 15 F fixtures
and the oracle sample, not every test. The F tests call `build_report` and `write_output`,
not `cli::run`; the smoke run exercises `run`.

## Review

One completion review (reviewer on Opus 5.5, because the OpenAI usage window was spent; the
first run on Sol 6.1 was paused part-way). Verdict: no blockers; every §16 row holds and the
test redesign keeps what Release proved. Nit fixed: DEFINITIONS named the v6 schema files.
Left as recorded: `Tempo ledger: skipped (S in check)` prints the contract symbol, as §12
specifies (word for word, pinned by a render golden test). Scope proposals for the Captain:
a `timing` CTest for F32's 30 s and 90 s limits; derive `CLI_SANITIZE_FIXTURES` from
`positions.json` or have `check_dbc` compare it with DEFINITIONS; report the B and E evals so
the checker re-derives `passes`; a long-ledger fixture (F41 or F42) in `cli_rerun_identity`;
mutants for root `tempo_value.moves`, `tempi.black` and run `holder`/`start_ply`/`tempi`.

## Open after EM3

The Captain found the definition of pure reply flawed after the gate: V2's `...Nxc2` wins
the rook `Nb4` attacked, yet counts as pure, so White's `Bxa7` gains a tempo. The built
ledger is as she confirmed it at checkpoint 2; the fix ("forced and gains nothing") goes to
PostM4_EM4, after a conceptual critique of all definitions and concepts.

## Contract changes

See §17 of the contract. In short: WP1's v7 cutover renamed the move schema early; §6.3
rule 3's precondition stated (no change of meaning); the timing fallback applied (`L` at
`standard` 40,000 → 30,000; at 40k F12 took 14.6 s alone and 15.2 s in the gate); at
checkpoint 2 V2 and F41 follow the built ledger (Bxa4 is `neither`; the only tempo is
White's Bxa7).

## Checkpoints

1. Contract, diagrams and terms approved as drafted (2026-10-24).
2. The Captain confirmed the built V2 ledger (2026-10-24). The pre-build probes had read
   Bxa4 as threatening; Black's best reply to a pass after Bxa4 wins no material.
3. Milestone end: the Captain tries the smoke commands.
