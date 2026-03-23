#!/usr/bin/env bash
##
# @file scripts/run_doxygen.sh
# @brief Run Doxygen with automatic fallback to the official binary release.
# @ingroup gstklv_tools
#
# This helper uses the system `doxygen` executable when available. If it is
# not installed, the script downloads the official Linux binary release,
# verifies the SHA-256 checksum, caches it under the user's cache directory,
# and runs Doxygen from there.
#
# Author: Mouhsine Kassimi Farhaoui
# Mail: mouhsine98@gmail.com
##

set -euo pipefail

source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/common.sh"

readonly DOXYGEN_VERSION="1.16.1"
readonly DOXYGEN_URL="https://www.doxygen.nl/files/doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz"
readonly DOXYGEN_SHA256="a56f885d37e3aae08a99f638d17bbb381224c03a878d9e2dda4f9fa4baf1d8bd"

usage() {
  cat <<'EOF'
Usage: ./scripts/run_doxygen.sh [--strict] [--doxyfile <path>]

Options:
  --strict            Treat Doxygen warnings as errors
  --doxyfile <path>   Doxygen configuration file to use (default: Doxyfile)
  -h, --help          Show this help message
EOF
}

cache_root() {
  printf '%s\n' "${XDG_CACHE_HOME:-$HOME/.cache}/gstklvplugin/tools"
}

sha256_file() {
  local file_path="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "${file_path}" | awk '{print $1}'
    return
  fi
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "${file_path}" | awk '{print $1}'
    return
  fi
  if command -v openssl >/dev/null 2>&1; then
    openssl dgst -sha256 "${file_path}" | awk '{print $NF}'
    return
  fi

  gstklv_error "No SHA-256 tool found. Install sha256sum, shasum, or openssl."
  exit 1
}

download_file() {
  local url="$1"
  local destination="$2"

  if command -v curl >/dev/null 2>&1; then
    gstklv_run curl -fsSL -A "gstklvplugin-doc-helper/1.0 (+https://gstreamer.freedesktop.org/)" \
      -o "${destination}" "${url}"
    return
  fi

  if command -v wget >/dev/null 2>&1; then
    gstklv_run wget --user-agent="gstklvplugin-doc-helper/1.0 (+https://gstreamer.freedesktop.org/)" \
      -O "${destination}" "${url}"
    return
  fi

  gstklv_error "Neither curl nor wget is available to download Doxygen."
  exit 1
}

validate_archive_members() {
  local archive="$1"
  local entry

  while IFS= read -r entry; do
    [[ -z "${entry}" ]] && continue
    case "${entry}" in
      /*|../*|*/../*|..)
        gstklv_error "Refusing to extract unsafe archive member: ${entry}"
        exit 1
        ;;
    esac
  done < <(tar -tzf "${archive}")
}

ensure_cached_doxygen() {
  local root archive extracted binary digest

  root="$(cache_root)"
  archive="${root}/doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz"
  extracted="${root}/doxygen-${DOXYGEN_VERSION}"
  binary="${extracted}/bin/doxygen"

  mkdir -p "${root}"

  if [[ -x "${binary}" ]]; then
    printf '%s\n' "${binary}"
    return
  fi

  gstklv_section "Fetching Doxygen"
  gstklv_print_var "Version" "${DOXYGEN_VERSION}"
  gstklv_print_var "Cache root" "${root}"
  gstklv_print_var "Source URL" "${DOXYGEN_URL}"

  download_file "${DOXYGEN_URL}" "${archive}"

  digest="$(sha256_file "${archive}")"
  if [[ "${digest}" != "${DOXYGEN_SHA256}" ]]; then
    gstklv_error "Doxygen archive checksum mismatch"
    gstklv_print_var "Expected" "${DOXYGEN_SHA256}"
    gstklv_print_var "Actual" "${digest}"
    exit 1
  fi

  validate_archive_members "${archive}"
  rm -rf "${extracted}"
  gstklv_run tar -xzf "${archive}" -C "${root}"

  if [[ ! -x "${binary}" ]]; then
    gstklv_error "Doxygen binary not found after extraction: ${binary}"
    exit 1
  fi

  printf '%s\n' "${binary}"
}

strict=0
doxyfile="Doxyfile"

while (($# > 0)); do
  case "$1" in
    --strict)
      strict=1
      ;;
    --doxyfile)
      shift
      if (($# == 0)); then
        gstklv_error "Missing value for --doxyfile"
        usage
        exit 1
      fi
      doxyfile="$1"
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

doxyfile="$(gstklv_resolve_path "${doxyfile}")"

if [[ ! -f "${doxyfile}" ]]; then
  gstklv_error "Doxyfile not found: ${doxyfile}"
  exit 1
fi

if command -v doxygen >/dev/null 2>&1; then
  doxygen_bin="$(command -v doxygen)"
else
  doxygen_bin="$(ensure_cached_doxygen)"
fi

tmp_log="$(mktemp "${TMPDIR:-/tmp}/gstklv-doxygen.XXXXXX.log")"
trap 'rm -f "${tmp_log}"' EXIT

gstklv_section "Running Doxygen"
gstklv_print_var "Doxygen binary" "${doxygen_bin}"
gstklv_print_var "Doxyfile" "${doxyfile}"
gstklv_step "Running: $(gstklv_join_command "${doxygen_bin}" "${doxyfile}")"

set +e
(
  cd "${GSTKLV_REPO_ROOT}"
  "${doxygen_bin}" "${doxyfile}"
) >"${tmp_log}" 2>&1
status=$?
set -e

cat "${tmp_log}"

if ((status != 0)); then
  gstklv_error "Doxygen exited with status ${status}"
  exit "${status}"
fi

if ((strict == 1)) && grep -qi 'warning:' "${tmp_log}"; then
  gstklv_error "Doxygen emitted warnings while running in strict mode"
  exit 1
fi

gstklv_success "Doxygen completed successfully"
