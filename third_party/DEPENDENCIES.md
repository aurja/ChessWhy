# Pinned dependencies

## Submodules (`third_party/`)

| Dependency | Pin | License | Role | Milestone |
|---|---|---|---|---|
| Disservin/chess-library | `53e6a841dcda7059a2af363d85f785ef1817304a` (master, 2026-07-26) | MIT | board, legal moves, FEN, PGN stream parser, SAN/UCI | M1 |
| nlohmann/json | `v3.12.0` | MIT | report serialisation | M1 |
| catchorg/Catch2 | `v3.16.1` | BSL-1.0 | tests | M1 |
| yhirose/cpp-httplib | `v0.61.0` | MIT | local HTTP API | M6 |
| ggml-org/llama.cpp | `b11712` | MIT | `libllama` inference | M4 |

Clone with `git clone --recurse-submodules`, or run
`git submodule update --init --recursive`. Change a pin only as an explicit task and
update this table.

## Development-only Python packages (`.venv/`, git-ignored, `tools/requirements-dev.txt`)

| Package | Pin | License | Role | Milestone |
|---|---|---|---|---|
| python-jsonschema/jsonschema with attrs, jsonschema-specifications, referencing, rpds-py | `4.26.0`; others pinned in `tools/requirements-dev.txt` | MIT (all five) | validates reports against `schema/report.v1.schema.json` in CTest | M1 |

## Fetched assets (`assets/`, git-ignored, `tools/fetch_assets.sh`)

| Asset | Version | SHA-256 | License | Use |
|---|---|---|---|---|
| Stockfish Linux x86-64 universal | `sf_19` | `9defc0d4e55d49c65a6d042f3e571a39fcea499ade6dbe741b53b8c65e03611f` (tarball) | GPL-3.0 | separate UCI process; never linked or redistributed |
| Qwen3.5-0.8B Q4_K_M GGUF (bartowski) | — | `fb044e93939a70469c905781334f5de1e6c8b608ced6cbc8c9249bd4127d9526` | Apache-2.0 | libllama build smoke test only |
| Lichess puzzle database | regenerated upstream | recorded in `assets/MANIFEST.txt` at fetch time | CC0 | M2 motif validation |
| lichess-org/chess-openings `a.tsv`–`e.tsv` | commit `a6189a30dc273ccb21fc2536a9a2fefd5592a67a` | per file in `tools/fetch_assets.sh` | CC0 | M1 opening names |
| Lichess broadcast PGNs (over the board) | months pinned in M3 | pinned in M3 | CC0 (Lichess export header) | M3 opening statistics |
| Lichess Elite Database PGNs (online) | months pinned in M3 | pinned in M3 | none stated; filtered from CC0 Lichess exports | M3 opening statistics; built table stays local until licensing is settled |
