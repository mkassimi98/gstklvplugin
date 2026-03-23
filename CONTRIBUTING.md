# Contributing to gstklvplugin

Thank you for considering a contribution. This guide describes how to set up a development environment, run tests, and submit changes.

---

## Table of Contents

- [Prerequisites](#prerequisites)
- [Building](#building)
- [Running Tests](#running-tests)
- [Code Style](#code-style)
- [Documentation](#documentation)
- [Submitting a Pull Request](#submitting-a-pull-request)
- [Environment Variables](#environment-variables)
- [License](#license)

---

## Prerequisites

| Tool | Minimum version | Purpose |
|---|---|---|
| Meson | 0.56 | Primary build system |
| Ninja | 1.8 | Build backend for Meson |
| GCC or Clang | C11 support | Compiler |
| GStreamer dev headers | 1.20 | Core, base, check packages |
| Python 3 | 3.9 | Reference scripts and integration tests |

Install on Ubuntu/Debian:

```bash
sudo apt-get install -y \
  meson ninja-build \
  gcc \
  libgstreamer1.0-dev \
  libgstreamer-plugins-base1.0-dev \
  libgstreamer-plugins-bad1.0-dev \
  python3 python3-gi python3-pytest \
  gstreamer1.0-tools \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-bad
```

CMake is also supported (see [Building with CMake](#building-with-cmake)) but Meson is preferred for development.

---

## Building

### Meson (recommended)

```bash
meson setup build
ninja -C build
```

Enable tests and examples:

```bash
meson setup build -Dtests=enabled
ninja -C build
```

Load the plugin locally:

```bash
export GST_PLUGIN_PATH="$PWD/build/src:$GST_PLUGIN_PATH"
export KLV_TAGS_INI="$PWD/data/stanag4609_tags.ini"
gst-inspect-1.0 klvmetaenc
```

### Building with CMake

```bash
cmake -S . -B build \
  -DGSTKLVPLUGIN_BUILD_TESTS=ON \
  -DGSTKLVPLUGIN_BUILD_EXAMPLES=ON
cmake --build build
```

---

## Running Tests

### gst-check unit tests (Meson)

This is the primary test suite. It tests all four GStreamer elements using the `gst-check` framework:

```bash
meson setup build -Dtests=enabled
ninja -C build
meson test -C build --print-errorlogs
```

Run a single test suite:

```bash
meson test -C build elements/test_klvmetaenc --print-errorlogs
```

Test binaries are located in `build/tests/check/`.

### Python integration tests

```bash
export GST_PLUGIN_PATH="$PWD/build/src:$GST_PLUGIN_PATH"
export KLV_TAGS_INI="$PWD/data/stanag4609_tags.ini"
python3 -m pytest tests/ -v
```

### C++ GoogleTest (CMake only)

```bash
cmake -S . -B build -DGSTKLVPLUGIN_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### Smoke checks

```bash
# Local 93-tag roundtrip
python3 examples/test_93_tags.py

# SRT streaming (two terminals)
python3 examples/srt-pipelines/python/srt_receiver_93tags.py --host 127.0.0.1 --port 5000
python3 examples/srt-pipelines/python/srt_sender_93tags.py   --host 0.0.0.0   --port 5000 --count 10

# File-based TS
python3 examples/ts/python/klv_recorder.py \
  --output examples/ts/recordings/capture.ts --count 10
python3 examples/ts/python/klv_video_reader.py \
  examples/ts/recordings/capture.ts --print-all
```

If you cannot run tests locally, note it in your pull request.

---

## Code Style

The codebase is written in **C11/GNU11**. Keep new code consistent with the existing style:

- Use GLib types (`gint`, `guint8`, `gboolean`, `gchar *`, etc.) inside GStreamer plugin code.
- Use standard C types in standalone utility modules.
- Prefer early returns for error handling; avoid deeply nested conditionals.
- Keep functions small and focused on a single responsibility.
- Use `GST_DEBUG_OBJECT`, `GST_WARNING_OBJECT`, `GST_ERROR_OBJECT` for element-level logging.
- Add Doxygen `@brief`, `@param`, and `@return` comments to all public functions.
- Keep paths relative to the repo root in documentation and examples.

Naming conventions:

- GStreamer element types: `GstKlvEncode`, `GstTsPmtRewrite`
- Element factory names: lowercase with no separators — `klvmetaenc`, `tspmtrewrite`
- Internal helper functions: `snake_case` prefixed by module — `klv_ber_encode_length`, `ts_parse_pmt_section`

---

## Documentation

Update documentation when behavior changes:

| File | When to update |
|---|---|
| `doc/code_reference.md` | New modules, renamed functions, or major refactors |
| `doc/standards.md` | Changes to standard compliance scope or encoding behavior |
| `doc/examples.md` | New examples or changes to example paths or options |
| `doc/design_decisions.md` | New architectural decisions or reversals of existing ones |
| `doc/klv_tags.md` | Changes to `data/stanag4609_tags.ini` |
| `CHANGELOG.md` | Every change, following the Keep a Changelog format |

Keep Doxygen comments in public headers accurate. Regenerate and verify:

```bash
doxygen Doxyfile
```

---

## Submitting a Pull Request

1. Fork the repository and create a feature branch from `devel`.
2. Make focused, reviewable changes. One logical change per PR where possible.
3. Ensure all tests pass (`meson test -C build`).
4. Update `CHANGELOG.md` under `[Unreleased]`.
5. Open a pull request against `devel` with:
   - A clear description of what changed and why.
   - The test commands you ran and their output (or a note if untestable locally).
   - Any known limitations or follow-up work.

---

## Environment Variables

| Variable | Purpose |
|---|---|
| `GST_PLUGIN_PATH` | Directory containing `gstklvplugin.so` |
| `KLV_TAGS_INI` | Override the tag registry path |
| `KLV_DEBUG=1` | Verbose KLV logging in `klvframeinject` |
| `GST_DEBUG=3` | Standard GStreamer debug output |

---

## License

By contributing, you agree that your contributions are licensed under the **MIT License**.

See [LICENSE](LICENSE) for the full license text.
