# Installation And Runtime Discovery

This guide covers the practical lifecycle of `gstklvplugin` on a development
machine or target system:

- local uninstalled development
- validated local builds
- system-wide installation
- staged installation for packaging
- plugin discovery checks
- blacklist and registry-cache recovery

If you only need a quick start, the repository root `README.md` is enough. Use
this page when you need a repeatable build and installation story.

---

## 1. Choose A Workflow

| Workflow | Use it when | Recommended command |
|---|---|---|
| Uninstalled development | You are editing code and want GStreamer to load the plugin straight from the build tree | `meson devenv -C build` |
| Full local validation | You want build + tests + smoke tests + Doxygen in one command | `./scripts/check_all.sh` |
| Automated local install | You want one helper to configure, build, validate, and install | `./scripts/install_plugin.sh --run-checks --install` |
| Docker-based workflow | You want a reproducible container environment for development or deployment | See [doc/docker.md](docker.md) |
| Manual system install | You want explicit control over prefix and install commands | `meson install -C build` |
| Staged packaging install | You want files under a `DESTDIR` staging root | `meson install -C build --destdir ...` |

---

## 2. Prerequisites

On Ubuntu or Debian:

```bash
sudo apt-get install -y \
  meson ninja-build pkg-config python3 \
  libgstreamer1.0-dev \
  libgstreamer-plugins-base1.0-dev
```

Optional but useful:

```bash
sudo apt-get install -y \
  clang-format \
  g++ \
  graphviz
```

Notes:

- `clang-format` is only needed for repository formatting checks.
- `graphviz` is only needed for Doxygen diagrams.
- The Meson test suite uses `gst-check`, which is provided by the GStreamer
  development packages above on common Debian-family systems.

---

## 3. Manual Local Build

Configure, compile, and test with Meson:

```bash
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

Enter a build-local environment with the plugin and tag registry already
exported:

```bash
meson devenv -C build
```

Inside that shell, the following variables are already set by the Meson
configuration in this repo:

- `GST_PLUGIN_PATH=<repo>/build/src`
- `KLV_TAGS_INI=<repo>/data/stanag4609_tags.ini`

Quick visibility check:

```bash
meson devenv -C build gst-inspect-1.0 --plugin klvplugin
```

This is the closest equivalent to the standard uninstalled GStreamer workflow:
you run against the build tree without installing into the system plugin
directory.

---

## 4. Manual System Installation

### 4.1 Recommended Meson flow

Use Meson when you want the plugin and the installed tag registry to land in a
normal system prefix:

```bash
meson setup build --prefix /usr/local
meson compile -C build
meson test -C build --print-errorlogs
sudo meson install -C build
```

The plugin shared object is installed under:

```bash
pkg-config --variable=pluginsdir gstreamer-1.0
```

On the current Debian-family environment used for validation, that resolves to:

```text
/usr/lib/x86_64-linux-gnu/gstreamer-1.0
```

The tag registry is installed under:

```text
${prefix}/share/gstklvplugin/stanag4609_tags.ini
```

### 4.2 CMake alternative

Meson is the canonical workflow in this repo, but CMake remains available:

```bash
cmake -S . -B build
cmake --build build
sudo cmake --install build
```

Use this path when you need the C++ example executables as part of the build.
The install destination now uses `${CMAKE_INSTALL_LIBDIR}/gstreamer-1.0`, which
is the right multiarch location on Debian-family systems.

### 4.3 Raspberry Pi And Other ARM Boards

For a native build on Raspberry Pi OS or another Debian-family ARM system, no
project-side Meson changes are needed.

Why:

- Meson already installs to `libdir/gstreamer-1.0`
- Debian-family ARM systems resolve `libdir` to the correct multiarch path
- the current CMake install path also uses `${CMAKE_INSTALL_LIBDIR}`

That means the normal native workflow is valid on a Raspberry Pi:

```bash
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
sudo meson install -C build
```

Only cross-compiling from another machine would need extra setup such as a
Meson cross file and target sysroot. This repo does not currently ship those
cross files.

---

## 5. Automatic Workflows

### 5.1 Development shell helper

This repo provides a wrapper for the local build environment:

```bash
./scripts/dev_env.sh
```

It configures and builds the default `build/` tree if needed, then opens a
shell with `GST_PLUGIN_PATH`, `KLV_TAGS_INI`, and a local `GST_REGISTRY`
already configured.

You can also execute a command directly:

```bash
./scripts/dev_env.sh -- gst-inspect-1.0 --plugin klvplugin
```

### 5.2 Full validation helper

Run the local validation bundle:

```bash
./scripts/check_all.sh
```

That command performs:

- Meson compile
- Meson tests
- `clang-format --dry-run --Werror` over tracked C/C++ files
- Python bytecode checks for scripts and examples
- strict Doxygen generation

### 5.3 Formatting helper

Apply repository formatting in place:

```bash
./scripts/run_clang_tools.sh --format
```

Validate formatting without modifying files:

```bash
./scripts/run_clang_tools.sh --format-check
```

The same checks are exposed from CMake:

```bash
cmake -S . -B build-cmake -DGSTKLVPLUGIN_BUILD_EXAMPLES=ON
cmake --build build-cmake --target format
cmake --build build-cmake --target format-check
```

### 5.4 Build and install helper

For an automated configure/build/validate/install flow:

```bash
./scripts/install_plugin.sh --run-checks --install --prefix /usr/local
```

Common variations:

```bash
# Reconfigure the existing build tree first
./scripts/install_plugin.sh --reconfigure --run-checks

# Skip Doxygen during validation
./scripts/install_plugin.sh --run-checks --skip-doxygen

# Stage files into a packaging root instead of the live filesystem
./scripts/install_plugin.sh \
  --run-checks \
  --install \
  --prefix /usr \
  --destdir /tmp/gstklvplugin-stage
```

The helper does not call `sudo` for you. If you are installing into a protected
system prefix, run the command with the privilege model you prefer.

For packaged distribution, see [doc/packaging.md](packaging.md).

---

## 6. How To Check Whether The Plugin Is Available

### 6.1 Check a single element

```bash
gst-inspect-1.0 --exists klvmetaenc && echo "klvmetaenc is available"
gst-inspect-1.0 --exists klvmetadec && echo "klvmetadec is available"
gst-inspect-1.0 --exists klvframeinject && echo "klvframeinject is available"
gst-inspect-1.0 --exists tspmtrewrite && echo "tspmtrewrite is available"
```

`gst-inspect-1.0 --exists` returns exit code `0` when the element or plugin is
visible.

### 6.2 Inspect the plugin as a whole

```bash
gst-inspect-1.0 --plugin klvplugin
```

That prints:

- plugin metadata
- installed/shared-object filename
- the four exported elements

### 6.3 Check an uninstalled build

For a build-tree check without system installation:

```bash
GST_PLUGIN_PATH="$PWD/build/src" \
KLV_TAGS_INI="$PWD/data/stanag4609_tags.ini" \
gst-inspect-1.0 --plugin klvplugin
```

Or use the standard Meson environment:

```bash
meson devenv -C build gst-inspect-1.0 --plugin klvplugin
```

---

## 7. How To Check Whether It Is Blacklisted

GStreamer exposes blacklist status directly in `gst-inspect-1.0`:

```bash
gst-inspect-1.0 -b
```

To search for this plugin specifically:

```bash
gst-inspect-1.0 -b | rg 'gstklvplugin|klvplugin|gstklv'
```

If the plugin is blacklisted, GStreamer found the file before, tried to load
it, and recorded a failure in the registry cache. Typical causes are:

- missing runtime dependency
- plugin init failure
- ABI mismatch after rebuilding against a different GStreamer
- stale cache entry after replacing the shared object

For deeper diagnostics:

```bash
GST_DEBUG=GST_PLUGIN_LOADING:6,gst_registry:6 \
gst-inspect-1.0 --plugin klvplugin
```

---

## 8. How To Clear The Blacklist And Rebuild The Registry

The blacklist is stored in the GStreamer registry cache. The normal cache
directory is:

```bash
echo "${XDG_CACHE_HOME:-$HOME/.cache}/gstreamer-1.0"
```

To remove cached registry files:

```bash
rm -f "${XDG_CACHE_HOME:-$HOME/.cache}/gstreamer-1.0"/registry.*.bin
```

Then re-run discovery:

```bash
gst-inspect-1.0 --plugin klvplugin
```

For a one-off clean registry without touching the normal cache:

```bash
GST_REGISTRY=/tmp/gst-registry-clean.bin \
GST_PLUGIN_PATH="$PWD/build/src" \
gst-inspect-1.0 --plugin klvplugin
```

If the plugin becomes visible with a clean `GST_REGISTRY`, the problem was the
cached registry, not the current shared object.

If it is still blacklisted after clearing the cache, the next step is not to
delete more files, but to fix the actual load failure under
`GST_DEBUG=GST_PLUGIN_LOADING:6,gst_registry:6`.

---

## 9. Recommended Day-To-Day Commands

Fast local developer loop:

```bash
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
meson devenv -C build
```

One-command validation:

```bash
./scripts/check_all.sh
```

One-command staged install:

```bash
./scripts/install_plugin.sh \
  --run-checks \
  --install \
  --prefix /usr \
  --destdir /tmp/gstklvplugin-stage
```

Visibility check after install:

```bash
gst-inspect-1.0 --plugin klvplugin
```

Blacklist check:

```bash
gst-inspect-1.0 -b
```

---

## 10. Related Documentation

- [doc/index.md](index.md) — Documentation landing page
- [doc/docker.md](docker.md) — Docker development and runtime integration guide
- [doc/plugin_usage_guide.md](plugin_usage_guide.md) — Integration patterns and runtime behavior
- [doc/tests.md](tests.md) — Test and smoke-test coverage
- [doc/doxygen_main.md](doxygen_main.md) — API reference entry point
