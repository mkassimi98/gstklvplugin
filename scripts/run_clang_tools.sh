#!/usr/bin/env bash
##
# @file scripts/run_clang_tools.sh
# @brief Run clang-format over the repository C and C++ sources.
# @ingroup gstklv_tools
#
# This helper provides a single entry point for repository formatting. It can
# format files in place or validate formatting in dry-run mode.
#
# Author: Mouhsine Kassimi Farhaoui
# Mail: mouhsine98@gmail.com
##

set -euo pipefail

source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/common.sh"

usage() {
  cat <<'EOF'
Usage: ./scripts/run_clang_tools.sh [options]

Actions:
  --format              Run clang-format in place
  --format-check        Validate formatting with clang-format --dry-run --Werror
Options:
  -h, --help            Show this help message

If no action is selected, the default workflow is:
  --format-check
EOF
}

collect_clang_files() {
  mapfile -t GSTKLV_CLANG_FILES < <(
    cd "${GSTKLV_REPO_ROOT}" &&
      git ls-files '*.c' '*.h' '*.cpp' '*.hpp'
  )

  if ((${#GSTKLV_CLANG_FILES[@]} == 0)); then
    gstklv_error "No tracked C/C++ files were found"
    exit 1
  fi
}

run_clang_format() {
  local mode="$1"
  local -a cmd=(clang-format)

  gstklv_section "clang-format"
  gstklv_print_var "Mode" "${mode}"
  gstklv_print_var "Files" "${#GSTKLV_CLANG_FILES[@]}"

  if [[ "${mode}" == "check" ]]; then
    cmd+=(--dry-run --Werror)
  else
    cmd+=(-i)
  fi

  gstklv_run_in_repo "${cmd[@]}" "${GSTKLV_CLANG_FILES[@]}"
  gstklv_success "clang-format ${mode} completed"
}

do_format=0
do_format_check=0

while (($# > 0)); do
  case "$1" in
    --format)
      do_format=1
      ;;
    --format-check)
      do_format_check=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      gstklv_error "Unknown option: $1"
      usage
      exit 1
      ;;
  esac
  shift
done

if ((do_format == 0 && do_format_check == 0)); then
  do_format_check=1
fi

gstklv_require_command git
gstklv_require_command clang-format

gstklv_section "Clang Tooling"
gstklv_print_var "Repository root" "${GSTKLV_REPO_ROOT}"
gstklv_print_var "clang-format" "$(clang-format --version)"

collect_clang_files

if ((do_format == 1)); then
  run_clang_format "write"
fi

if ((do_format_check == 1)); then
  run_clang_format "check"
fi

gstklv_success "Requested clang tooling steps completed successfully"
