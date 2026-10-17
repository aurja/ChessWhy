#!/usr/bin/env bash
set -euo pipefail

MACHINE_NAME="${1:-debian-home}"

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
OMP_DIR="$REPO_ROOT/.omp"
LOCAL_ENV_FILE="$OMP_DIR/LOCAL_ENVIRONMENT.md"
GITIGNORE="$REPO_ROOT/.gitignore"

mkdir -p "$OMP_DIR"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

command_path() {
    command -v "$1" 2>/dev/null || printf 'not available'
}

first_line() {
    "$@" 2>/dev/null | head -n 1 || true
}

# Debian can ship only versioned LLVM binaries (clang-19); report the newest.
llvm_command() {
    if command -v "$1" >/dev/null 2>&1; then
        printf '%s' "$1"
        return
    fi

    local candidate suffix newest="" newest_version=-1
    while IFS= read -r candidate; do
        suffix="${candidate#"$1"-}"
        if [[ "$suffix" =~ ^[0-9]+$ ]] && ((suffix > newest_version)); then
            newest="$candidate"
            newest_version="$suffix"
        fi
    done < <(compgen -c -- "$1-" | sort -u)

    printf '%s' "${newest:-$1}"
}

tool_section() {
    local label="$1"
    local command_name="$2"
    shift 2

    local path
    path="$(command_path "$command_name")"

    printf -- "- %s\n" "$label"

    if [[ "$path" == "not available" ]]; then
        printf "  - Status: not available\n"
    else
        printf "  - Command: \`%s\`\n" "$command_name"
        printf "  - Path: \`%s\`\n" "$path"

        local version
        version="$(first_line "$@")"

        if [[ -n "$version" ]]; then
            printf "  - Version: \`%s\`\n" "$version"
        fi
    fi
}

# Stockfish reads UCI from stdin; feed `quit` so the probe cannot hang.
stockfish_version() {
    printf 'quit\n' | "$1" 2>/dev/null | head -n 1
}

asset_line() {
    if [[ -e "$2" ]]; then
        printf -- "- %s: \`%s\`\n" "$1" "${2#"$REPO_ROOT"/}"
    else
        printf -- "- %s: not present (run \`tools/fetch_assets.sh\`)\n" "$1"
    fi
}

# ---------------------------------------------------------------------------
# Machine-local environment description
# ---------------------------------------------------------------------------

{
    echo "# Local Development Environment"
    echo
    echo "> This file is machine-specific and should not be committed to Git."
    echo

    echo "## Machine"
    echo
    echo "- Machine name: \`$MACHINE_NAME\`"
    echo "- Hostname: \`$(hostname)\`"
    echo "- Architecture: \`$(uname -m)\`"
    echo "- Kernel: \`$(uname -sr)\`"

    if [[ -f /etc/os-release ]]; then
        # shellcheck disable=SC1091
        source /etc/os-release
        echo "- Operating system: \`${PRETTY_NAME:-unknown}\`"
    fi

    echo "- Shell: \`${SHELL:-unknown}\`"
    echo "- Repository root: \`$REPO_ROOT\`"
    echo

    echo "## OMP"
    echo
    tool_section "OMP" omp omp --version
    echo "- Project OMP directory: \`.omp/\`"
    echo "- Project LSP configuration: \`.omp/lsp.json\`"
    echo "- Project session directory: \`.omp/sessions/\`"
    echo "- Local environment file: \`.omp/LOCAL_ENVIRONMENT.md\`"
    echo

    echo "## Python"
    echo
    tool_section "Python" python3 python3 --version
    echo
    tool_section "uv" uv uv --version
    echo
    tool_section "basedpyright" basedpyright basedpyright --version
    echo
    tool_section "basedpyright LSP" basedpyright-langserver basedpyright-langserver --version
    echo
    tool_section "Ruff (CLI and LSP via ruff server)" ruff ruff --version
    echo
    tool_section "pytest" pytest pytest --version
    echo

    echo "## C and C++"
    echo
    tool_section "GCC" gcc gcc --version
    echo
    tool_section "G++" g++ g++ --version
    echo
    for llvm_tool in clang clang++ clangd clang-tidy clang-format; do
        case "$llvm_tool" in
            clang) llvm_label="Clang" ;;
            clang++) llvm_label="Clang++" ;;
            *) llvm_label="$llvm_tool" ;;
        esac
        llvm_cmd="$(llvm_command "$llvm_tool")"
        tool_section "$llvm_label" "$llvm_cmd" "$llvm_cmd" --version
        echo
    done
    tool_section "CMake" cmake cmake --version
    echo
    tool_section "CTest" ctest ctest --version
    echo
    tool_section "Ninja" ninja ninja --version
    echo
    tool_section "Bear" bear bear --version
    echo
    tool_section "ccache" ccache ccache --version
    echo
    tool_section "GDB" gdb gdb --version
    echo
    tool_section "Valgrind" valgrind valgrind --version
    echo

    echo "### Compiler/runtime validation"
    echo
    echo "Typical validation options (support depends on the installed compiler and project):"
    echo
    echo "- Compiler warnings:"
    echo "  - \`-Wall\`"
    echo "  - \`-Wextra\`"
    echo "  - \`-Wpedantic\`"
    echo "- ThreadSanitizer:"
    echo "  - \`-fsanitize=thread\`"
    echo "- Debug-friendly sanitizer option:"
    echo "  - \`-fno-omit-frame-pointer\`"
    echo

    echo "## Shell and command-line utilities"
    echo
    tool_section "ShellCheck" shellcheck shellcheck --version
    echo
    tool_section "jq" jq jq --version
    echo
    tool_section "ripgrep" rg rg --version
    echo
    if command -v fdfind >/dev/null 2>&1; then
        tool_section "fd (Debian name: fdfind)" fdfind fdfind --version
    else
        tool_section "fd" fd fd --version
    fi
    echo

    echo "## Chess engine and language models"
    echo
    tool_section "Stockfish (system, not used by tests)" stockfish stockfish_version stockfish
    echo
    if [[ -x "$REPO_ROOT/assets/engines/stockfish" ]]; then
        printf -- "- Project Stockfish: \`assets/engines/stockfish\` (%s)\n" \
            "$(stockfish_version "$REPO_ROOT/assets/engines/stockfish")"
    else
        asset_line "Project Stockfish" "$REPO_ROOT/assets/engines/stockfish"
    fi
    asset_line "Asset manifest" "$REPO_ROOT/assets/MANIFEST.txt"
    echo "- GGUF models in \`assets/models/\`:"
    if compgen -G "$REPO_ROOT/assets/models/*.gguf" >/dev/null; then
        for model in "$REPO_ROOT"/assets/models/*.gguf; do
            printf "  - \`%s\` (%s)\n" "$(basename "$model")" "$(du -h "$model" | cut -f1)"
        done
    else
        echo "  - none"
    fi
    echo "- CPU flags relevant to llama.cpp/Stockfish: \`$(grep -o -w -E 'avx2|avx512f|avx512_vnni|avxvnni|fma|bmi2' /proc/cpuinfo 2>/dev/null | sort -u | tr '\n' ' ')\`"
    echo "- Logical CPUs: \`$(nproc)\`; memory: \`$(free -h | awk '/^Mem:/{print $2}')\`"
    echo

    echo "## Book text extraction"
    echo
    tool_section "pdftotext (poppler)" pdftotext pdftotext -v
    echo
    tool_section "djvutxt (djvulibre)" djvutxt djvutxt --help
    echo
    tool_section "Tesseract OCR" tesseract tesseract --version
    echo
    tool_section "OCRmyPDF" ocrmypdf ocrmypdf --version
    echo
    tool_section "zstd" zstd zstd --version
    echo

    echo "## Git"
    echo
    tool_section "Git" git git --version
    echo

    echo "## Executable search path"
    echo
    echo '```text'
    printf '%s\n' "$PATH" | tr ':' '\n'
    echo '```'
    echo

    echo "## Notes"
    echo
    echo "- This file describes only the \`$MACHINE_NAME\` development environment."
    echo "- Do not infer that paths, versions, package managers, or installed tools are identical on other machines."
    echo "- Prefer commands available through \`PATH\` rather than hard-coded absolute paths."
    echo "- The counterpart development machine may provide a different \`.omp/LOCAL_ENVIRONMENT.md\`."
} > "$LOCAL_ENV_FILE"

# ---------------------------------------------------------------------------
# Ensure the local environment file is ignored
# ---------------------------------------------------------------------------

IGNORE_ENTRY=".omp/LOCAL_ENVIRONMENT.md"

touch "$GITIGNORE"

if ! grep -Fxq "$IGNORE_ENTRY" "$GITIGNORE"; then
    printf '\n%s\n' "$IGNORE_ENTRY" >> "$GITIGNORE"
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

echo
echo "Local environment generated (.omp/AGENTS.md includes it via @LOCAL_ENVIRONMENT.md):"
echo "  $LOCAL_ENV_FILE"
echo
echo "Machine name: $MACHINE_NAME"
echo
echo "Git status:"
git -C "$REPO_ROOT" status --short -- \
    .omp/LOCAL_ENVIRONMENT.md \
    .gitignore
