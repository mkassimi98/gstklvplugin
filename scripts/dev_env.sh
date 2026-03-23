#!/usr/bin/env bash
##
# @file scripts/dev_env.sh
# @brief Bootstrap a local development environment for gstklvplugin.
# @ingroup gstklv_tools
#
# This helper configures and builds the Meson tree when needed, exports the
# environment variables required by the plugin examples, and launches either an
# interactive shell or a user-specified command inside that environment.
#
# Author: Mouhsine Kassimi Farhaoui
# Mail: mouhsine98@gmail.com
##

set -euo pipefail

source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/common.sh"

usage() {
  cat <<'EOF'
Usage: ./scripts/dev_env.sh [options] [-- <command>...]

Options:
  --build-dir <dir>   Meson build directory to use (default: build)
  --reconfigure       Force Meson reconfiguration before building
  --skip-build        Do not run meson compile
  -h, --help          Show this help message
EOF
}

ensure_build_tree() {
  local build_dir="$1"
  local reconfigure="$2"

  if [[ "${reconfigure}" -eq 1 && -d "${build_dir}" ]]; then
    gstklv_section "Reconfiguring Meson build tree"
    gstklv_run_in_repo meson setup "${build_dir}" --reconfigure
    return
  fi

  if [[ ! -d "${build_dir}" ]]; then
    gstklv_section "Configuring Meson build tree"
    gstklv_run_in_repo meson setup "${build_dir}"
  fi
}

build_dir="build"
reconfigure=0
skip_build=0
command_args=()

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
    --reconfigure)
      reconfigure=1
      ;;
    --skip-build)
      skip_build=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      command_args=("$@")
      break
      ;;
    *)
      command_args=("$@")
      break
      ;;
  esac
  shift
done

build_dir="$(gstklv_resolve_path "${build_dir}")"

gstklv_require_command meson

gstklv_section "Preparing Development Environment"
gstklv_print_var "Repository root" "${GSTKLV_REPO_ROOT}"
gstklv_print_var "Build directory" "${build_dir}"

ensure_build_tree "${build_dir}" "${reconfigure}"

if ((skip_build == 0)); then
  gstklv_section "Compiling Project"
  gstklv_run_in_repo meson compile -C "${build_dir}"
else
  gstklv_warn "Skipping build step by request"
fi

export GST_PLUGIN_PATH="${build_dir}/src${GST_PLUGIN_PATH:+:${GST_PLUGIN_PATH}}"
export KLV_TAGS_INI="${GSTKLV_REPO_ROOT}/data/stanag4609_tags.ini"
export GST_REGISTRY="${GST_REGISTRY:-${build_dir}/gst-registry-dev.bin}"

gstklv_section "Environment Summary"
gstklv_print_var "GST_PLUGIN_PATH" "${GST_PLUGIN_PATH}"
gstklv_print_var "KLV_TAGS_INI" "${KLV_TAGS_INI}"
gstklv_print_var "GST_REGISTRY" "${GST_REGISTRY}"

if ((${#command_args[@]} == 0)); then
  shell_bin="${SHELL:-$(command -v bash || command -v sh || printf '/bin/sh')}"
  gstklv_info "Launching interactive shell: ${shell_bin}"
  exec "${shell_bin}" -i
fi

gstklv_info "Executing command: $(gstklv_join_command "${command_args[@]}")"
exec "${command_args[@]}"
