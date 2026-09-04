# Test Suite

gstklvplugin now uses a single testing story:

- `Meson` is the canonical build and test runner.
- `gst-check` is the test framework, matching the GStreamer testing docs.
- `GstHarness` is used for single-pad or transform-style element checks.
- Small in-process pipelines are used where the element contract is inherently multi-pad, such as `klvframeinject`.
- Example-level smoke tests run under Meson as short Python workflows.

There are no parallel `pytest` or `GoogleTest` suites anymore. That keeps the repo easier to maintain and makes failures easier to interpret.

---

## Test Structure

```text
tests/
  check/
    elements/
      test_klvmetaenc.c
      test_klvmetadec.c
      test_klvframeinject.c
      test_tspmtrewrite.c
    libs/
      test_klv_utils.c
      test_ts_psi.c
  smoke/
    smoke_ts_roundtrip.py
    smoke_udp_loopback.py
    smoke_staged_install_clean_registry.py
    smoke_ts_roundtrip_cpp.py
```

---

## Official Alignment

This layout follows the GStreamer testing APIs documented in:

- [`gstreamer-check`](https://gstreamer.freedesktop.org/documentation/check/index.html) for unit test infrastructure
- [`GstHarness`](https://gstreamer.freedesktop.org/documentation/check/gstharness.html) for element-level push/pull tests
- [`GstTestClock`](https://gstreamer.freedesktop.org/documentation/check/gsttestclock.html) when deterministic clock control is needed in future timing suites

In this repo that translates to:

- element contracts tested in C under `tests/check/elements/`
- pure helper logic tested in C under `tests/check/libs/`
- Meson owning build, environment setup, and execution

---

## Build And Run

```bash
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

Run a single suite:

```bash
meson test -C build elements/test_klvframeinject --print-errorlogs
meson test -C build libs/test_klv_utils --print-errorlogs
meson test -C build smoke/ts_roundtrip_python --print-errorlogs
meson test -C build smoke/udp_loopback_python --print-errorlogs
```

Run a suite binary directly:

```bash
GST_DEBUG=3 ./build/tests/check/elements_test_tspmtrewrite
```

Meson sets the important environment automatically:

- `GST_PLUGIN_PATH=build/src`
- `KLV_TAGS_INI=data/stanag4609_tags.ini`
- an isolated `GST_REGISTRY` file under `build/tests/check/`

---

## Current Coverage

### Element suites

| Suite | Focus |
|---|---|
| `elements/test_klvmetaenc` | Factory creation, pads, full 16-byte MISB UL, checksum TLV, round-trip of large integers and raw byte tags |
| `elements/test_klvmetadec` | Factory creation, pads, strict MISB UL validation, encode/decode round-trip, JSON recovery for raw tags, `PTS`/`DTS` preservation |
| `elements/test_klvframeinject` | Factory creation, pads, default properties, video passthrough, per-frame KLV generation, fallback-to-properties behaviour |
| `elements/test_tspmtrewrite` | Factory creation, pads, default properties, PMT rewrite to `0x06 + KLVA`, metadata descriptor serialization |

### Utility suites

| Suite | Focus |
|---|---|
| `libs/test_klv_utils` | BER helpers, BCC-16, scaling helpers, tag registry parsing, JSON parsing/formatting |
| `libs/test_ts_psi` | CRC-32/MPEG-2, PAT parsing, PMT build/parse, PSI section extraction |

### Smoke suites

| Suite | Focus |
|---|---|
| `smoke/ts_roundtrip_python` | Record a short `.ts` file, replay it headlessly, and verify that KLV decoding reaches end-of-stream cleanly |
| `smoke/udp_loopback_python` | Start the UDP receiver example, stream a short sender burst, and verify decoded KLV JSON arrives |
| `smoke/staged_install_clean_registry` | Install the plugin into a temporary staging root and verify discovery from a clean `GST_REGISTRY` file outside the build tree |
| `smoke/ts_roundtrip_cpp` | Configure a temporary CMake build, run the C++ TS recorder, and replay the file with the C++ TS reader |

`smoke/ts_roundtrip_cpp` is only added when `cmake` is available on the host.

---

## Why `klvframeinject` Uses A Pipeline Test

`klvframeinject` has one sink pad and two source pads. Its public contract is not just “accept a buffer”, but:

- forward the incoming video buffer unchanged on `video_src`
- emit a matching KLV buffer on `klv_src`
- keep timestamps aligned across both outputs

For that reason the functional tests use a tiny in-process GStreamer pipeline with `appsrc` and `appsink` under `gst-check`, instead of forcing everything through a single `GstHarness`. This stays within the GStreamer testing model while making the assertions much clearer and more robust.

---

## Expected Output

Typical output looks like this when `cmake` is available:

```text
1/10 elements/test_klvmetaenc               OK
2/10 elements/test_klvmetadec               OK
3/10 elements/test_klvframeinject           OK
4/10 elements/test_tspmtrewrite             OK
5/10 libs/test_klv_utils                    OK
6/10 libs/test_ts_psi                       OK
7/10 smoke/ts_roundtrip_python              OK
8/10 smoke/udp_loopback_python              OK
9/10 smoke/staged_install_clean_registry    OK
10/10 smoke/ts_roundtrip_cpp                OK
```

The exact ordering may vary. Without `cmake`, the C++ smoke is skipped and the
expected total becomes nine suites.

---

## What Is Not Covered

| Area | How to validate |
|---|---|
| End-to-end SRT/UDP network behaviour | Run the example sender/receiver workflows in `doc/srt_pipelines.md` and `doc/udp_pipelines.md` |
| Full 93-tag end-to-end exercise | `python3 examples/test_93_tags.py` |
| PMT signaling in captured transport streams | `python3 tools/verify_ts_klv.py capture.ts --list-all` |
| Long-running playback or GUI timing | Manual example workflows and recorded `.ts` reader examples |
| SRT end-to-end transport | Manual workflows in `doc/srt_pipelines.md` |

---

## Helper Commands

Repository helpers that build on top of the Meson test story:

```bash
# Full local validation bundle
./scripts/check_all.sh

# Apply clang-format in place to tracked C/C++ files
./scripts/run_clang_tools.sh --format

# Validate formatting without changing files
./scripts/run_clang_tools.sh --format-check

# Strict Doxygen validation only
./scripts/run_doxygen.sh --strict

# Configure/build and open a shell with the right runtime environment
./scripts/dev_env.sh

# Configure/build/validate/install in one workflow
./scripts/install_plugin.sh --run-checks --install --prefix /usr/local
```

The same formatting checks are available from CMake:

```bash
cmake -S . -B build-cmake -DGSTKLVPLUGIN_BUILD_EXAMPLES=ON
cmake --build build-cmake --target format
cmake --build build-cmake --target format-check
```

---

## Compliance Evidence Package

```bash
# 1. Build
meson setup build
meson compile -C build

# 2. Inspect the plugin
GST_PLUGIN_PATH=build/src gst-inspect-1.0 klvmetaenc
GST_PLUGIN_PATH=build/src gst-inspect-1.0 klvmetadec
GST_PLUGIN_PATH=build/src gst-inspect-1.0 klvframeinject
GST_PLUGIN_PATH=build/src gst-inspect-1.0 tspmtrewrite

# 3. Run the canonical test suite
meson test -C build --print-errorlogs

# 4. Exercise the full 93-tag path
GST_PLUGIN_PATH=build/src python3 examples/test_93_tags.py

# 5. Verify a captured transport stream
python3 tools/verify_ts_klv.py capture.ts --list-all
```

---

## Related Documentation

- [doc/index.md](index.md) — Documentation landing page
- [doc/standards.md](standards.md) — Standards and encoding rules
- [doc/compliance_appendix.md](compliance_appendix.md) — PMT descriptor bytes
- [doc/design_decisions.md](design_decisions.md) — Design rationale
