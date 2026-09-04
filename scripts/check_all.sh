#!/usr/bin/env bash
##
# @file scripts/check_all.sh
# @brief Run the full local validation workflow for gstklvplugin.
# @ingroup gstklv_tools
#
# This helper configures the Meson build tree when needed, compiles the
# project, executes the Meson test suite including smoke tests, validates the
# Python scripts with py_compile, validates the Bash helper scripts with
# `bash -n`, and runs strict Doxygen validation.
#
# Author: Mouhsine Kassimi Farhaoui
# Mail: mouhsine98@gmail.com
##

set -euo pipefail

source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/common.sh"

usage() {
  cat <<'EOF'
Usage: ./scripts/check_all.sh [options]

Options:
  --build-dir <dir>         Meson build directory to use (default: build)
  --skip-clang-tools        Skip clang-format validation
  --skip-doxygen            Skip the strict Doxygen validation step
  -h, --help                Show this help message
EOF
}

ensure_build_tree() {
  local build_dir="$1"
  if [[ ! -d "${build_dir}" ]]; then
    gstklv_section "Configuring Meson build tree"
    gstklv_run_in_repo meson setup "${build_dir}"
  fi
}

build_dir="build"
skip_clang_tools=0
skip_doxygen=0

while (($# > 0)); do
  case "$1" in
    --build-dir)
      shift
      if (($# == 0)); then
        gstklv_error "Missing value for --build-dir"
        usage
        exit 1
      fi
      build_dir="$1"
      ;;
    --skip-clang-tools)
      skip_clang_tools=1
      ;;
    --skip-doxygen)
      skip_doxygen=1
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

build_dir="$(gstklv_resolve_path "${build_dir}")"

gstklv_require_command meson
gstklv_require_command bash
gstklv_require_command python3

gstklv_section "Repository Validation"
gstklv_print_var "Repository root" "${GSTKLV_REPO_ROOT}"
gstklv_print_var "Build directory" "${build_dir}"

ensure_build_tree "${build_dir}"

gstklv_section "Meson Build And Tests"
gstklv_run_in_repo meson compile -C "${build_dir}"
gstklv_run_in_repo meson test -C "${build_dir}" --print-errorlogs

gstklv_section "Python Syntax Checks"
gstklv_run_in_repo python3 -m py_compile \
  examples/srt-pipelines/python/srt_sender_95tags.py \
  examples/srt-pipelines/python/srt_receiver_95tags.py \
  examples/ts/python/klv_recorder.py \
  examples/ts/python/klv_video_reader.py \
  examples/udp-pipelines/python/udp_sender_95tags.py \
  examples/udp-pipelines/python/udp_receiver_95tags.py \
  examples/test_95_tags.py \
  tools/capture_ts_from_srt.py \
  tools/ts_pmt_rewrite.py \
  tools/verify_ts_klv.py \
  tests/smoke/smoke_ts_roundtrip.py \
  tests/smoke/smoke_udp_loopback.py \
  tests/smoke/smoke_ts_roundtrip_cpp.py \
  tests/smoke/smoke_staged_install_clean_registry.py

gstklv_section "Bash Syntax Checks"
gstklv_run_in_repo bash -n \
  scripts/common.sh \
  scripts/dev_env.sh \
  scripts/run_doxygen.sh \
  scripts/run_clang_tools.sh \
  scripts/check_all.sh \
  scripts/install_plugin.sh \
  packaging/deb/build_deb.sh

if ((skip_clang_tools == 0)); then
  gstklv_section "Clang Tooling Validation"
  gstklv_run_in_repo ./scripts/run_clang_tools.sh --format-check
else
  gstklv_warn "Skipping clang tooling validation by request"
fi

if ((skip_doxygen == 0)); then
  gstklv_section "Doxygen Validation"
  gstklv_run_in_repo ./scripts/run_doxygen.sh --strict
else
  gstklv_warn "Skipping Doxygen validation by request"
fi

gstklv_success "All checks completed successfully"
