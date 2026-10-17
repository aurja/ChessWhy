#!/usr/bin/env bash
# Fetch pinned, git-ignored runtime assets into assets/.
#   tools/fetch_assets.sh            engine + smoke model + puzzles + opening names
#   tools/fetch_assets.sh engine     only Stockfish
#   tools/fetch_assets.sh model      only the smoke-test GGUF model
#   tools/fetch_assets.sh puzzles    only the Lichess puzzle database
#   tools/fetch_assets.sh openings   only the lichess-org/chess-openings name tables
# Every download is checked against a SHA-256. The Lichess puzzle export is
# regenerated upstream, so its hash is taken from Lichess's published
# sha256sums.txt at fetch time and recorded in assets/MANIFEST.txt.
set -euo pipefail

REPO_ROOT="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
ASSETS="$REPO_ROOT/assets"
MANIFEST="$ASSETS/MANIFEST.txt"

STOCKFISH_VERSION="sf_19"
STOCKFISH_URL="https://github.com/official-stockfish/Stockfish/releases/download/${STOCKFISH_VERSION}/stockfish-linux-x86-64-universal.tar.gz"
STOCKFISH_SHA256="9defc0d4e55d49c65a6d042f3e571a39fcea499ade6dbe741b53b8c65e03611f"

# Smoke-test model only (exercises the libllama build). The product default
# model is chosen later from a benchmark; see Planning/PROJECT_VISION.md.
SMOKE_MODEL_FILE="Qwen_Qwen3.5-0.8B-Q4_K_M.gguf"
SMOKE_MODEL_URL="https://huggingface.co/bartowski/Qwen_Qwen3.5-0.8B-GGUF/resolve/main/${SMOKE_MODEL_FILE}"
SMOKE_MODEL_SHA256="fb044e93939a70469c905781334f5de1e6c8b608ced6cbc8c9249bd4127d9526"

PUZZLES_URL="https://database.lichess.org/lichess_db_puzzle.csv.zst"
PUZZLES_SUMS_URL="https://database.lichess.org/sha256sums.txt"

# Opening names (CC0), pinned to a commit; one TSV per ECO volume.
OPENINGS_COMMIT="a6189a30dc273ccb21fc2536a9a2fefd5592a67a"
OPENINGS_URL="https://raw.githubusercontent.com/lichess-org/chess-openings/${OPENINGS_COMMIT}"
declare -A OPENINGS_SHA256=(
    [a]="3282e4c9155289a29224f9a85fba0decb46efa35c2fa5e39662d2ac48fb0f793"
    [b]="1d5ed134ebbd87915ead5683416e37583bb3e5ae3582b4d8cb5cb4aa7ef4f623"
    [c]="45e843c7b7e79a0d04d5bda879c9bd23f5a10974488bd8615e4f4c5d0749986f"
    [d]="1cf4057a8aece94f27bb3f278ad410ee2844831a642364eeceaafa091dbcceda"
    [e]="48c6f6645b031a5937c8386839debab0e2e48837251687ba1afb6369677f8546"
)

log() { printf '[fetch_assets] %s\n' "$*"; }

record() {
    mkdir -p "$ASSETS"
    touch "$MANIFEST"
    grep -v "^$1 " "$MANIFEST" > "$MANIFEST.tmp" || true
    printf '%s %s %s %s\n' "$1" "$2" "$3" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$MANIFEST.tmp"
    mv "$MANIFEST.tmp" "$MANIFEST"
}

download_verified() { # url dest sha256
    local url="$1" dest="$2" sha="$3"
    if [[ -f "$dest" ]] && printf '%s  %s\n' "$sha" "$dest" | sha256sum --check --status -; then
        log "up to date: ${dest#"$REPO_ROOT"/}"
        return
    fi
    mkdir -p "$(dirname "$dest")"
    log "downloading $url"
    curl --fail --location --silent --show-error \
        --retry 10 --retry-all-errors --retry-delay 5 \
        --speed-limit 1024 --speed-time 60 \
        --continue-at - --output "$dest.part" "$url"
    if ! printf '%s  %s\n' "$sha" "$dest.part" | sha256sum --check --quiet -; then
        rm -f "$dest.part"
        log "checksum mismatch for ${dest#"$REPO_ROOT"/}; partial file removed"
        exit 1
    fi
    mv "$dest.part" "$dest"
}

fetch_engine() {
    local dir="$ASSETS/engines/stockfish-19"
    local tarball="$dir/stockfish-linux-x86-64-universal.tar.gz"
    download_verified "$STOCKFISH_URL" "$tarball" "$STOCKFISH_SHA256"
    tar -xzf "$tarball" -C "$dir"
    local bin
    bin="$(find "$dir" -type f -name 'stockfish*' ! -name '*.tar.gz' -perm -u+x | head -n 1)"
    [[ -n "$bin" ]] || { log "no stockfish executable in $tarball"; exit 1; }
    ln -sfn "${bin#"$ASSETS/engines/"}" "$ASSETS/engines/stockfish"
    log "engine: $(printf 'quit\n' | "$ASSETS/engines/stockfish" | head -n 1)"
    record stockfish "$STOCKFISH_VERSION" "$STOCKFISH_SHA256"
}

fetch_model() {
    download_verified "$SMOKE_MODEL_URL" "$ASSETS/models/$SMOKE_MODEL_FILE" "$SMOKE_MODEL_SHA256"
    record smoke-model "$SMOKE_MODEL_FILE" "$SMOKE_MODEL_SHA256"
}

fetch_puzzles() {
    local sha
    sha="$(curl --fail --silent --location "$PUZZLES_SUMS_URL" | awk '$2 == "lichess_db_puzzle.csv.zst" {print $1}')"
    [[ -n "$sha" ]] || { log "puzzle hash not found in $PUZZLES_SUMS_URL"; exit 1; }
    download_verified "$PUZZLES_URL" "$ASSETS/data/lichess_db_puzzle.csv.zst" "$sha"
    record lichess-puzzles "lichess_db_puzzle.csv.zst" "$sha"
}

fetch_openings() {
    local vol
    for vol in a b c d e; do
        download_verified "$OPENINGS_URL/$vol.tsv" "$ASSETS/data/openings/$vol.tsv" \
            "${OPENINGS_SHA256[$vol]}"
    done
    record chess-openings "$OPENINGS_COMMIT" "per-file-sha256-in-fetch_assets.sh"
}

case "${1:-all}" in
    engine) fetch_engine ;;
    model) fetch_model ;;
    puzzles) fetch_puzzles ;;
    openings) fetch_openings ;;
    all) fetch_engine; fetch_model; fetch_puzzles; fetch_openings ;;
    *) log "unknown target: $1"; exit 2 ;;
esac
