#!/usr/bin/env bash
##
# @file packaging/deb/build_deb.sh
# @brief Build a Debian package for gstklvplugin using the Meson install layout.
# @ingroup gstklv_tools
#
# This helper creates a `.deb` package from a Meson build tree, computes shared
# library dependencies with `dpkg-shlibdeps`, and produces an architecture-
# specific package suitable for Debian, Ubuntu, and Raspberry Pi OS.
#
# Author: Mouhsine Kassimi Farhaoui
# Mail: mouhsine98@gmail.com
##

set -euo pipefail

source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../scripts" && pwd)/common.sh"

usage() {
  cat <<'EOF'
Usage: ./packaging/deb/build_deb.sh [options]

Options:
  --build-dir <dir>     Meson build directory to use (default: build)
  --output-dir <dir>    Directory for generated .deb files (default: packaging/out)
  --prefix <path>       Meson install prefix inside the package (default: /usr)
  --version <value>     Override package version (default: parsed from meson.build)
  --arch <value>        Override Debian architecture (default: dpkg-architecture)
  --package-name <name> Override package name (default: gstklvplugin)
  --maintainer <value>  Override Maintainer field
  --run-checks          Run ./scripts/check_all.sh before packaging
  --skip-doxygen        Skip Doxygen when --run-checks is used
  -h, --help            Show this help message
EOF
}

parse_version() {
  sed -n "s/.*version : '\\([^']*\\)'.*/\\1/p" "${GSTKLV_REPO_ROOT}/meson.build" | head -n1
}

ensure_build_tree() {
  local build_dir="$1"
  local prefix="$2"

  if [[ -d "${build_dir}" ]]; then
    gstklv_section "Reconfiguring Meson build tree for packaging"
    gstklv_run_in_repo meson setup "${build_dir}" --reconfigure --prefix "${prefix}"
    return
  fi

  gstklv_section "Configuring Meson build tree for packaging"
  gstklv_run_in_repo meson setup "${build_dir}" --prefix "${prefix}"
}

build_dir="build"
output_dir="packaging/out"
prefix="/usr"
version=""
arch=""
package_name="gstklvplugin"
maintainer="Mouhsine Kassimi Farhaoui <mouhsine98@gmail.com>"
run_checks=0
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
      build_dir="${1:-}"
      ;;
    --output-dir)
      shift
      if (($# == 0)); then
        gstklv_error "Missing value for --output-dir"
        usage
        exit 1
      fi
      output_dir="${1:-}"
      ;;
    --prefix)
      shift
      if (($# == 0)); then
        gstklv_error "Missing value for --prefix"
        usage
        exit 1
      fi
      prefix="${1:-}"
      ;;
    --version)
      shift
      if (($# == 0)); then
        gstklv_error "Missing value for --version"
        usage
        exit 1
      fi
      version="${1:-}"
      ;;
    --arch)
      shift
      if (($# == 0)); then
        gstklv_error "Missing value for --arch"
        usage
        exit 1
      fi
      arch="${1:-}"
      ;;
    --package-name)
      shift
      if (($# == 0)); then
        gstklv_error "Missing value for --package-name"
        usage
        exit 1
      fi
      package_name="${1:-}"
      ;;
    --maintainer)
      shift
      if (($# == 0)); then
        gstklv_error "Missing value for --maintainer"
        usage
        exit 1
      fi
      maintainer="${1:-}"
      ;;
    --run-checks)
      run_checks=1
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
output_dir="$(gstklv_resolve_path "${output_dir}")"

gstklv_require_command meson
gstklv_require_command dpkg-deb
gstklv_require_command dpkg-architecture
gstklv_require_command dpkg-shlibdeps

if [[ -z "${version}" ]]; then
  version="$(parse_version)"
fi

if [[ -z "${version}" ]]; then
  gstklv_error "Failed to determine package version from meson.build"
  exit 1
fi

if [[ -z "${arch}" ]]; then
  arch="$(dpkg-architecture -qDEB_HOST_ARCH)"
fi

mkdir -p "${output_dir}"

stage_root="$(mktemp -d "${TMPDIR:-/tmp}/gstklvplugin-deb-stage.XXXXXX")"
shlibdeps_root="$(mktemp -d "${TMPDIR:-/tmp}/gstklvplugin-shlibdeps.XXXXXX")"
trap 'rm -rf "${stage_root}" "${shlibdeps_root}"' EXIT

gstklv_section "Debian Package Build"
gstklv_print_var "Repository root" "${GSTKLV_REPO_ROOT}"
gstklv_print_var "Build directory" "${build_dir}"
gstklv_print_var "Output directory" "${output_dir}"
gstklv_print_var "Prefix" "${prefix}"
gstklv_print_var "Package name" "${package_name}"
gstklv_print_var "Version" "${version}"
gstklv_print_var "Architecture" "${arch}"

ensure_build_tree "${build_dir}" "${prefix}"

gstklv_section "Compiling Project"
gstklv_run_in_repo meson compile -C "${build_dir}"

if ((run_checks == 1)); then
  gstklv_section "Running Validation Bundle"
  check_cmd=(./scripts/check_all.sh --build-dir "${build_dir}")
  if ((skip_doxygen == 1)); then
    check_cmd+=(--skip-doxygen)
  fi
  gstklv_run_in_repo "${check_cmd[@]}"
fi

gstklv_section "Installing Into Staging Root"
gstklv_run_in_repo meson install -C "${build_dir}" --destdir "${stage_root}"

package_root="${stage_root}"
chmod 755 "${package_root}"
plugin_file="$(find "${package_root}" -type f -name 'gstklvplugin.so' | head -n1)"

if [[ -z "${plugin_file}" ]]; then
  gstklv_error "Installed plugin was not found in staging root"
  exit 1
fi

mkdir -p "${package_root}/DEBIAN"

doc_dir="${package_root}${prefix}/share/doc/${package_name}"
mkdir -p "${doc_dir}"
cp -f "${GSTKLV_REPO_ROOT}/LICENSE" "${doc_dir}/"
cp -f "${GSTKLV_REPO_ROOT}/README.md" "${doc_dir}/"
cp -f "${GSTKLV_REPO_ROOT}/CHANGELOG.md" "${doc_dir}/"

mkdir -p "${shlibdeps_root}/debian"
cat > "${shlibdeps_root}/debian/control" <<EOF
Source: ${package_name}
Section: libs
Priority: optional
Maintainer: ${maintainer}
Standards-Version: 4.6.2

Package: ${package_name}
Architecture: ${arch}
Description: temporary control file for dependency scanning
EOF

depends="$(
  cd "${shlibdeps_root}" &&
  dpkg-shlibdeps -O -e"${plugin_file}" | sed -n 's/^shlibs:Depends=//p'
)"

control_file="${package_root}/DEBIAN/control"
{
  printf 'Package: %s\n' "${package_name}"
  printf 'Version: %s\n' "${version}"
  printf 'Section: libs\n'
  printf 'Priority: optional\n'
  printf 'Architecture: %s\n' "${arch}"
  printf 'Maintainer: %s\n' "${maintainer}"
  if [[ -n "${depends}" ]]; then
    printf 'Depends: %s\n' "${depends}"
  fi
  printf 'Description: GStreamer KLV plugin with MISB ST 0601.8 support\n'
  printf ' GStreamer plugin suite for KLV metadata workflows, including\n'
  printf ' JSON/KLV transforms, per-frame metadata injection, and STANAG 4609\n'
  printf ' MPEG-TS signaling helpers.\n'
} > "${control_file}"

output_file="${output_dir}/${package_name}_${version}_${arch}.deb"

gstklv_section "Building Debian Package"
gstklv_run dpkg-deb --root-owner-group --build "${package_root}" "${output_file}"

gstklv_success "Debian package created successfully"
gstklv_print_var "Package file" "${output_file}"
