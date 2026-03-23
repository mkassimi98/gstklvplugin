#!/usr/bin/env bash
##
# @file scripts/common.sh
# @brief Shared logging and utility helpers for repository Bash scripts.
# @ingroup gstklv_tools
#
# This file centralizes path discovery, colored logging, and small helper
# functions used by the repository maintenance scripts.
#
# Author: Mouhsine Kassimi Farhaoui
# Mail: mouhsine98@gmail.com
##

GSTKLV_SCRIPT_DIR="${GSTKLV_SCRIPT_DIR:-$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)}"
GSTKLV_REPO_ROOT="${GSTKLV_REPO_ROOT:-$(cd -- "${GSTKLV_SCRIPT_DIR}/.." && pwd)}"

if [[ -t 1 && -z "${NO_COLOR:-}" ]]; then
  GSTKLV_COLOR_RESET=$'\033[0m'
  GSTKLV_COLOR_INFO=$'\033[1;34m'
  GSTKLV_COLOR_STEP=$'\033[1;36m'
  GSTKLV_COLOR_WARN=$'\033[1;33m'
  GSTKLV_COLOR_ERROR=$'\033[1;31m'
  GSTKLV_COLOR_SUCCESS=$'\033[1;32m'
  GSTKLV_COLOR_SECTION=$'\033[1;35m'
else
  GSTKLV_COLOR_RESET=''
  GSTKLV_COLOR_INFO=''
  GSTKLV_COLOR_STEP=''
  GSTKLV_COLOR_WARN=''
  GSTKLV_COLOR_ERROR=''
  GSTKLV_COLOR_SUCCESS=''
  GSTKLV_COLOR_SECTION=''
fi

gstklv_timestamp() {
  date '+%Y-%m-%d %H:%M:%S'
}

gstklv_log() {
  local level="$1"
  local color="$2"
  shift 2
  printf '%s[%s] [%s]%s %s\n' \
    "${color}" \
    "$(gstklv_timestamp)" \
    "${level}" \
    "${GSTKLV_COLOR_RESET}" \
    "$*"
}

gstklv_info() {
  gstklv_log "INFO" "${GSTKLV_COLOR_INFO}" "$@"
}

gstklv_step() {
  gstklv_log "STEP" "${GSTKLV_COLOR_STEP}" "$@"
}

gstklv_warn() {
  gstklv_log "WARN" "${GSTKLV_COLOR_WARN}" "$@"
}

gstklv_error() {
  gstklv_log "ERROR" "${GSTKLV_COLOR_ERROR}" "$@" >&2
}

gstklv_success() {
  gstklv_log " OK " "${GSTKLV_COLOR_SUCCESS}" "$@"
}

gstklv_section() {
  printf '\n'
  gstklv_log "SECTION" "${GSTKLV_COLOR_SECTION}" "$@"
}

gstklv_join_command() {
  local joined=''
  local arg
  for arg in "$@"; do
    joined+="$(printf '%q ' "${arg}")"
  done
  printf '%s' "${joined% }"
}

gstklv_run() {
  gstklv_step "Running: $(gstklv_join_command "$@")"
  "$@"
}

gstklv_run_in_repo() {
  gstklv_step "Running: $(gstklv_join_command "$@")"
  (
    cd "${GSTKLV_REPO_ROOT}"
    "$@"
  )
}

gstklv_require_command() {
  local cmd="$1"
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    gstklv_error "Required command not found: ${cmd}"
    exit 1
  fi
}

gstklv_resolve_path() {
  local path="$1"
  if [[ "${path}" = /* ]]; then
    printf '%s\n' "${path}"
  else
    printf '%s\n' "${GSTKLV_REPO_ROOT}/${path}"
  fi
}

gstklv_print_var() {
  printf '  %-18s %s\n' "$1" "$2"
}
