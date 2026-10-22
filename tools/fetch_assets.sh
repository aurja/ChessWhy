#!/usr/bin/env bash
# Fetch pinned, git-ignored runtime assets into assets/.
#   tools/fetch_assets.sh            engine + smoke model + puzzles + openings + games
#   tools/fetch_assets.sh engine     only Stockfish
#   tools/fetch_assets.sh model      only the smoke-test GGUF model
#   tools/fetch_assets.sh puzzles    only the Lichess puzzle database
#   tools/fetch_assets.sh openings   only the lichess-org/chess-openings name tables
#   tools/fetch_assets.sh games      only the pinned Lichess broadcast table
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
# Lichess broadcast PGNs, pinned to sha256sums.txt on 2026-10-21.
BROADCAST_MONTHS=(
    2020-01 2020-02 2020-03 2020-04 2020-05 2020-06
    2020-07 2020-08 2020-09 2020-10 2020-11 2020-12
    2021-01 2021-02 2021-03 2021-04 2021-05 2021-06
    2021-07 2021-08 2021-09 2021-10 2021-11 2021-12
    2022-01 2022-02 2022-03 2022-04 2022-05 2022-06
    2022-07 2022-08 2022-09 2022-10 2022-11 2022-12
    2023-01 2023-02 2023-03 2023-04 2023-05 2023-06
    2023-07 2023-08 2023-09 2023-10 2023-11 2023-12
    2024-01 2024-02 2024-03 2024-04 2024-05 2024-06
    2024-07 2024-08 2024-09 2024-10 2024-11 2024-12
    2025-01 2025-02 2025-03 2025-04 2025-05 2025-06
    2025-07 2025-08 2025-09 2025-10 2025-11 2025-12
    2026-01 2026-02 2026-03 2026-04 2026-05 2026-06
    2026-07 2026-08 2026-09
)
declare -A BROADCAST_SHA256=(
    [2020-01]="9461382d6933dedfbe7bb3de2ac139066d8d8657f2070cb11d59c7686f00f47e"
    [2020-02]="013411015df5a2c07f6005de16cc956b846b1aef36ee56643fb0a909d4a3feb6"
    [2020-03]="51adf1be8b7dc69cb317093ab06aba50131e056d1aa69f849a3c2ca9e1e3babc"
    [2020-04]="8c39dfedff509c36a37a017d2be5dd066b1e1dbb9e62a6bf63b16c018d496725"
    [2020-05]="1e791e6578e434b5e35df1f0780e3e9ddff6b22d047579dba8230ab5c1f7ea7d"
    [2020-06]="2cb57e6681cdaaa18142f3287d0cdb057210638f519b2e65a90374a78acf7cdd"
    [2020-07]="d2d12d73ea2721122a50622ec669ab8a50324fb97faf2a3721d292a9824353e4"
    [2020-08]="bfc3216b85ccdc63b7ffd6261d826347f5b294006ba222e82585433d8fb7fe18"
    [2020-09]="69169589e509b1717faabaa8ff03067af10869642d156b6500f421e2775a798e"
    [2020-10]="23c6f9726b7f50af1297442316dba364c7a19fb2ab5c5cecdb9526a69e6f1664"
    [2020-11]="61a489bba13af03271c0eaa3fc08cda6054bc94e07c82a933810767e63b55e88"
    [2020-12]="91b4ad4576ee1e53aeff3492ccacd25a0e58112befe8058016fff5fb62c32369"
    [2021-01]="48159f1a55c61dca8addf1e37f555ac4a10f129851b0ae5331afbd70b53b7b8f"
    [2021-02]="d68fb32b9a895e2b1acb420ea50414c81ca95a8dca83223cc44e871958113ca2"
    [2021-03]="c8610ce7a1e03093236f8e3ffd6f18642bafc8706f42cc61d48214dc1810432b"
    [2021-04]="ca86c70aff854377423a6cfedd17290e377b47a594bf5f263211441d4f23a42b"
    [2021-05]="4bd8733d23c1318207a1f492e10d176edd1e3a505c637e0442ff82ecd00c7370"
    [2021-06]="99c37b910da4be0d9d8aa1491456c7e08a6e9932e54244cb17b41b7afd754ee6"
    [2021-07]="facfbdeff07d7f36cdcf14bd342ef7b663dcb6cb5c643af920aa3f3076d7861a"
    [2021-08]="da11a29ce22c92e6ad87bc33e005b69e91040bc99ee981db39b99ea50a05f38b"
    [2021-09]="4d21b63b49b1ff458f6011033bb242f3d95138b6ddb5ccc01b0a24213cb92711"
    [2021-10]="22a23de4e16713846452c3a7c2f9071501f3046d2d5f7e54348a3d33551459f3"
    [2021-11]="23b4b06d4309ab726349a89040493cdb0bf1cca524a7ddbf480f41fa7bacf8f9"
    [2021-12]="a88ce4bb96ec670f52716e108743bb629fa70e3fe0dd3d892c042d4c68a799a7"
    [2022-01]="b66e0af21df09713bb7093cac853ef04527d589963e0bcbd39620cf03f20303f"
    [2022-02]="fcca15bd3d0060bade81a799fb42ecfdbaf70bcc8746488b7ad623b45431b805"
    [2022-03]="b5ab992f88d28ed9a225f1fe64ab74485de9811483fbd66edfb9095aac2aa408"
    [2022-04]="c0b2607462be962125535ad4afdce21f362a0d4113aa4db3c6daddacd3ea03f3"
    [2022-05]="752168d1f620544e12e6974d92e1974655687055049b59b86bd98bf99aa3ab72"
    [2022-06]="517cf19858c8fb43f17fa4375f1bb8c6a52a5f21599232e5b62e81341056fc96"
    [2022-07]="fcee46c674066941d46531938511fc7a7de45a5153fc115465dde7f8032a5b96"
    [2022-08]="2cdc93fe77ab43e534214ae6a44462d261bebb10c47ad61d1c4d02b67d313461"
    [2022-09]="e1a80febd895ac08f9be0c405d5c788381084a49af8a991b9a5a4a070ef448a0"
    [2022-10]="316cca64cce9ebf1b31c2f646a63e9ff8122eb18da5c2830a37276d9f829871d"
    [2022-11]="fba1b54c418521dd803e854560dc5c5d338bf815da98ebf82e547e544f118102"
    [2022-12]="698ada93a6474d7c8036759454e4903b86d5d79965ee61f271275e5f58179033"
    [2023-01]="4bb223a574c148a75eccd260bb89c1d748c2ee505e594ad1578cfc1a0b97fe3e"
    [2023-02]="bcb758ec420c5e38a9aedceff21b20b80451e3418ee6893fa10398386981b964"
    [2023-03]="3c4fa359993716557123ca4f6ba9c73c098a407646fbc909e08f4378db15a910"
    [2023-04]="74174b6a3bd4981ac0366cf26258f62f252aabfc790affc35792b16ab04461f8"
    [2023-05]="1a6c0bba0cced51feb50b9e62a85f5713ba4ad957334e056f9713db94f423450"
    [2023-06]="49cff44dcef9877a4b658118f887a954d67a6875a2da4595ad4b5c72cbb37302"
    [2023-07]="895655258162edfbfaa7c81529e481772bfdb85495a2363a84f95c4aed55ddc7"
    [2023-08]="db8fcee8390c2edbf959c197f9da89714ac565d00e10b221afcd769366ea7691"
    [2023-09]="b11402dab04b8270942723660956f9258f51aeea687e4f4b95942bc801336be6"
    [2023-10]="9589177b519c848236fe684d28ad3cc2afa80a360755a31376efd7767a247452"
    [2023-11]="3390ad4bd2f2a5d6ad3a4475723fb591fe8f4bc16926ec9eb1854696f400399c"
    [2023-12]="a2fa6ef6a6d3bc219c6acbbfded72df45f6aa24111a2cb314153e0b88caf0048"
    [2024-01]="edd008e921e495e0f176967eb81434004692c52ae1cd1fe20e3d03bde37d24fb"
    [2024-02]="5b363f1041d893c034f7e349bbe8b290c909b135eca116c0eb1a942bd1d08ae4"
    [2024-03]="1174cfd7900603ab9f435a1f37f0447aa613c3d646e650e8828e0a6eb7766551"
    [2024-04]="8f9081128c29713868e02f7a862cae50274f74f3dad1d9e3af0f6ef2c29f4fef"
    [2024-05]="b367b00f0d594e6a5942947a8ca55fc243f9de5dbf4a16fc851ff8404c596fea"
    [2024-06]="ea871988a15bb72c4e359ee935ceb18103eb978672c86983f5563139e9eefeb1"
    [2024-07]="8b9d0e9f247363d799947232854bd2aee88c27faf80a6a3a2468d72bb6d2853e"
    [2024-08]="1ad8ce6ab83688ff2813b62ae98b0962ab467756c3cf79641c4a7ce7e99f2137"
    [2024-09]="e6b441d0c8994648be6b024e7d14feaaaba675567b76607decf6cc91cfeda447"
    [2024-10]="7d2042e12bfd9e077402865dd4ca0c335d68da31d414a88f2c94842b48971603"
    [2024-11]="354d96760609711ce471c131339a3a999d83929cb790615be4d76fb00bd3a89d"
    [2024-12]="b2ae52c6cacfc078045760e2f4fd5e8e5ae3d24accf6d02fb8692d3fb5b02e9e"
    [2025-01]="301bb574c3aca71d51a070ef7c43954434c10601e90869a262adf21c38801423"
    [2025-02]="f92d0fea95e6c733898a920c0790a8c02f3f8a1e1c9ca0c42f15330cd5430434"
    [2025-03]="840bdb7d640f82032f8373c86efcb69d0801c0d54cf3edf2152e4b8d25b8da87"
    [2025-04]="6b6fd9e9e79a94e80d0f556fb323ad951922fdff1f42089fc18a348e65d79624"
    [2025-05]="ed1b62de4c4f81e19d8e269a0ddbb4c5133361ff942439938f9d20f3d904795c"
    [2025-06]="b226325f2798003e4811dca8ea9556dc91837ec9ae3493f00ba102b50a73a43c"
    [2025-07]="1ffc6b775d27631ac988dd816a137b6dd0ec05561aeee8b45861e0c08dbcb10b"
    [2025-08]="f925d1f3b494517c8eae223976a886bed77a7b8627b17ec287e50cdda766dae8"
    [2025-09]="da3a261e926a2c0f5e79c393818e643d76448edf94cfdaefa3892258a6f9e7ba"
    [2025-10]="77be4c998ca8d3b303928e1619f0f9dc2a03a71d5ab6a59d02d42291ae0524e3"
    [2025-11]="39deb43faf6e2f4569a76f369ca0a3b87aa7787330c9e2ff87705c6f3bf32ff1"
    [2025-12]="fadbe80ee65e9f8499578e934d7600ece69d09b1cf8abc75fdd3bd840f61e63f"
    [2026-01]="9abf10d106a8bf5ecd41359825b3bf55c2f124c8a790a380cb98844c2cca5dca"
    [2026-02]="ea977569917718b33940ba5379db2adad77d58876c29084294d357f15fe6a31b"
    [2026-03]="1a5ad83fc9f440dcdadde17b11cc81a953f4895aaf8b8588470b3078209dc7fc"
    [2026-04]="97b036f3a3639ae3be59c1508b1e18c76ac1b2e8fada4587d1b5c5a93c438d5d"
    [2026-05]="d212987f7972a70268809ff6c726acff8bf0987161aacd7647b160f8564f6a53"
    [2026-06]="65905d611c0d78d90ab57cd9edd3821f4119b46c22b5489fd7e265a74c6767b3"
    [2026-07]="714d0eb99f99fca8d791142038b6c59b5ca6a51b3339bd3891a92f4bdffcbf0c"
    [2026-08]="e227c35c3207ebade754849c1825982e2717ae82c013013d896778e27be77724"
    [2026-09]="27a39d035767a30901b89602f33f2a7b27d4ab5025842071165fa079f8fff1df"
)

GAMES_TABLE_SHA256="e0cb7fab32128854d72b158178225ae586c2567a6b79bdfb4bc504390038d7a7"

GAMES_TABLE_MONTHS="2020-01..2026-09"
GAMES_TABLE="lichess_broadcast_2200.tsv"
GAMES_TOOL="${CHESSWHY_GAMES_EXE:-$REPO_ROOT/build/release/chesswhy-games}"

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

fetch_games() {
    local broadcast_dir="$ASSETS/data/games/broadcast"
    local month file
    local files=()
    for month in "${BROADCAST_MONTHS[@]}"; do
        file="$broadcast_dir/lichess_db_broadcast_${month}.pgn.zst"
        download_verified "https://database.lichess.org/broadcast/${file##*/}" "$file" \
            "${BROADCAST_SHA256[$month]}"
        files+=("$file")
    done

    [[ -x "$GAMES_TOOL" ]] || {
        log "games builder not found: $GAMES_TOOL"
        exit 1
    }
    local table="$ASSETS/data/games/$GAMES_TABLE"
    local table_tmp="$table.tmp"
    mkdir -p "$(dirname "$table")"
    log "building ${table#"$REPO_ROOT"/}"
    if ! zstd -dc "${files[@]}" | "$GAMES_TOOL" --max-ply 40 --min-games 5 \
        --months "$GAMES_TABLE_MONTHS" > "$table_tmp"; then
        rm -f "$table_tmp"
        log "games table build failed"
        exit 1
    fi
    if ! printf '%s  %s\n' "$GAMES_TABLE_SHA256" "$table_tmp" |
        sha256sum --check --quiet -; then
        rm -f "$table_tmp"
        log "games table checksum mismatch"
        exit 1
    fi
    mv "$table_tmp" "$table"
    record lichess-broadcast "$GAMES_TABLE" "$GAMES_TABLE_SHA256"
}

case "${1:-all}" in
    engine) fetch_engine ;;
    model) fetch_model ;;
    puzzles) fetch_puzzles ;;
    openings) fetch_openings ;;
    games) fetch_games ;;
    all) fetch_engine; fetch_model; fetch_puzzles; fetch_openings; fetch_games ;;
    *) log "unknown target: $1"; exit 2 ;;
esac

