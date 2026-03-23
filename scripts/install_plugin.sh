#!/usr/bin/env bash
##
# @file scripts/install_plugin.sh
# @brief Configure, build, validate, and optionally install gstklvplugin.
# @ingroup gstklv_tools
#
# This helper wraps the standard Meson workflow for this repository. It can
# configure or reconfigure the build tree, compile the project, optionally run
# the full validation bundle, and optionally install the resulting files either
# into the configured prefix or into a DESTDIR staging root.
#
# Author: Mouhsine Kassimi Farhaoui
# Mail: mouhsine98@gmail.com
##

set -euo pipefail

source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/common.sh"

usage() {
  cat <<'EOF'
Usage: ./scripts/install_plugin.sh [options]

Options:
  --build-dir <dir>   Meson build directory to use (default: build)
  --prefix <path>     Installation prefix to pass to Meson setup/reconfigure
  --reconfigure       Force Meson reconfiguration before building
  --skip-build        Skip the compile step
  --run-checks        Run ./scripts/check_all.sh before installation
  --skip-doxygen      Skip Doxygen when --run-checks is used
  --install           Run meson install after a successful build
  --destdir <path>    Optional DESTDIR staging directory for meson install
  -h, --help          Show this help message
EOF
}

ensure_build_tree() {
  local build_dir="$1"
  local reconfigure="$2"
  local prefix="$3"
  local -a cmd=()

  if [[ -d "${build_dir}" ]]; then
    if [[ "${reconfigure}" -eq 1 || -n "${prefix}" ]]; then
      cmd=(meson setup "${build_dir}" --reconfigure)
      if [[ -n "${prefix}" ]]; then
        cmd+=(--prefix "${prefix}")
      fi
      gstklv_section "Reconfiguring Meson build tree"
      gstklv_run_in_repo "${cmd[@]}"
    fi
    return
  fi

  cmd=(meson setup "${build_dir}")
  if [[ -n "${prefix}" ]]; then
    cmd+=(--prefix "${prefix}")
  fi

  gstklv_section "Configuring Meson build tree"
  gstklv_run_in_repo "${cmd[@]}"
}

build_dir="build"
prefix=""
reconfigure=0
skip_build=0
run_checks=0
skip_doxygen=0
do_install=0
destdir=""

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
    --prefix)
      shift
      if (($# == 0)); then
        gstklv_error "Missing value for --prefix"
        usage
        exit 1
      fi
      prefix="$1"
      ;;
    --reconfigure)
      reconfigure=1
      ;;
    --skip-build)
      skip_build=1
      ;;
    --run-checks)
      run_checks=1
      ;;
    --skip-doxygen)
      skip_doxygen=1
      ;;
    --install)
      do_install=1
      ;;
    --destdir)
      shift
      if (($# == 0)); then
        gstklv_error "Missing value for --destdir"
        usage
        exit 1
      fi
      destdir="$1"
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

gstklv_section "Installation Workflow"
gstklv_print_var "Repository root" "${GSTKLV_REPO_ROOT}"
gstklv_print_var "Build directory" "${build_dir}"
gstklv_print_var "Prefix" "${prefix:-<meson default>}"
gstklv_print_var "Install step" "$([[ ${do_install} -eq 1 ]] && printf 'enabled' || printf 'disabled')"
if [[ -n "${destdir}" ]]; then
  gstklv_print_var "DESTDIR" "${destdir}"
fi

ensure_build_tree "${build_dir}" "${reconfigure}" "${prefix}"

if ((skip_build == 0)); then
  gstklv_section "Compiling Project"
  gstklv_run_in_repo meson compile -C "${build_dir}"
else
  gstklv_warn "Skipping build step by request"
fi

if ((run_checks == 1)); then
  gstklv_section "Running Validation Bundle"
  check_cmd=(./scripts/check_all.sh --build-dir "${build_dir}")
  if ((skip_doxygen == 1)); then
    check_cmd+=(--skip-doxygen)
  fi
  gstklv_run_in_repo "${check_cmd[@]}"
fi

if ((do_install == 1)); then
  gstklv_section "Installing Files"
  install_cmd=(meson install -C "${build_dir}")
  if [[ -n "${destdir}" ]]; then
    install_cmd+=(--destdir "${destdir}")
  fi
  gstklv_run_in_repo "${install_cmd[@]}"
else
  gstklv_warn "Install step not requested; build artifacts were left in place"
fi

gstklv_success "Installation workflow completed successfully"
