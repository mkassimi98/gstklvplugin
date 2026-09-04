# gstklvplugin — Documentation Index

<p align="center">
  <img src="assets/gstklvplugin_icon_v2.png" alt="gstklvplugin icon" width="220" />
</p>

A GStreamer 1.x plugin suite for end-to-end KLV metadata workflows. Implements SMPTE ST 336, MISB ST 0601.8 (tags 1-95), STANAG 4609, and MISB ST 1402. Written in C11.

---

## Documentation Map

| Document | Purpose |
|---|---|
| `README.md` (repository root) | Project overview, quick start, build instructions |
| [doc/installation.md](installation.md) | Manual and automatic install flows, plugin discovery, blacklist recovery |
| [doc/docker.md](docker.md) | Docker integration patterns, container networking, and troubleshooting |
| [doc/packaging.md](packaging.md) | Debian/Raspberry Pi packaging workflow |
| [doc/releasing.md](releasing.md) | GitFlow branches, versioning, tagging, and GitHub release workflow |
| [doc/standards.md](standards.md) | Standards implemented, encoding rules, compliance scope |
| [doc/code_reference.md](code_reference.md) | Module layout, key functions, data flow |
| [doc/design_decisions.md](design_decisions.md) | Rationale behind key design choices |
| [doc/klv_tags.md](klv_tags.md) | Full MISB ST 0601.8 tag registry table |
| [doc/compliance_appendix.md](compliance_appendix.md) | PMT descriptor bytes and verification steps |
| [doc/examples.md](examples.md) | Example workflows — Python and C++ |
| [doc/95_tags.md](95_tags.md) | Full ST 0601.8 tags 1-95 workflow |
| [doc/srt_pipelines.md](srt_pipelines.md) | SRT pipeline composition and options |
| [doc/udp_pipelines.md](udp_pipelines.md) | UDP pipeline composition and options |
| [doc/tests.md](tests.md) | Test suite — Meson + gst-check |
| [doc/doxygen_main.md](doxygen_main.md) | Doxygen API reference entry point |
| [doc/plugin_usage_guide.md](plugin_usage_guide.md) | Integration guide for Python and C++ applications |

---

## Architecture Overview

```
include/gstklv/             Public plugin headers (element type declarations)
include/gstklv/internal/    Internal shared utilities (KLV, TS, JSON, BER)

src/plugins/                GStreamer element implementations
  klvencode.c               klvmetaenc — JSON → KLV
  klvdecode.c               klvmetadec — KLV → JSON
  klvframeinject.c          klvframeinject — per-frame KLV injection
  tspmtrewrite.c            tspmtrewrite — MPEG-TS PMT signaling

src/klv/                    KLV utility implementations
  klv_ber.c                 BER length encode/decode
  klv_checksum.c            BCC-16 (Tag 1)
  klv_json.c                Flat JSON parser
  klv_scaling.c             Range-based scaling
  klv_tag_defs.c            Tag registry loader
  klv_ul.c                  MISB ST 0601 Universal Label constant

src/ts/                     MPEG-TS PSI utilities
  ts_crc32.c                CRC-32/MPEG-2
  ts_psi.c                  PAT/PMT parsing and PMT builder

data/stanag4609_tags.ini    Authoritative tag registry (tags 1-95)
```

---

## Build

### Meson (recommended)

```bash
meson setup build
meson compile -C build

# Run tests and smoke tests
meson test -C build --print-errorlogs
```

Use the standard Meson uninstalled environment:

```bash
meson devenv -C build
```

### CMake (alternative)

```bash
cmake -S . -B build
cmake --build build
```

### Load the plugin

```bash
export GST_PLUGIN_PATH="$PWD/build/src:$GST_PLUGIN_PATH"
export KLV_TAGS_INI="$PWD/data/stanag4609_tags.ini"
gst-inspect-1.0 --plugin klvplugin
gst-inspect-1.0 klvmetaenc
gst-inspect-1.0 klvmetadec
gst-inspect-1.0 klvframeinject
gst-inspect-1.0 tspmtrewrite
```

### Automatic helpers

```bash
./scripts/check_all.sh
./scripts/dev_env.sh
./scripts/install_plugin.sh --run-checks --install --prefix /usr/local
```

---

## Quick Commands

Local tags 1-95 roundtrip:

```bash
python3 examples/test_95_tags.py
```

SRT streaming:

```bash
# Terminal 1 — receiver
python3 examples/srt-pipelines/python/srt_receiver_95tags.py \
  --host 127.0.0.1 --port 5000

# Terminal 2 — sender
python3 examples/srt-pipelines/python/srt_sender_95tags.py \
  --host 0.0.0.0 --port 5000 --count 50
```

UDP streaming:

```bash
# Terminal 1 — receiver
python3 examples/udp-pipelines/python/udp_receiver_95tags.py \
  --host 0.0.0.0 --port 5000

# Terminal 2 — sender
python3 examples/udp-pipelines/python/udp_sender_95tags.py \
  --host 127.0.0.1 --port 5000 --count 50
```

TS record and playback:

```bash
python3 examples/ts/python/klv_recorder.py \
  --output examples/ts/recordings/capture.ts --count 50

python3 examples/ts/python/klv_video_reader.py \
  examples/ts/recordings/capture.ts --print-all
```

PMT verification:

```bash
python3 tools/capture_ts_from_srt.py \
  --host 127.0.0.1 --port 5000 \
  --output capture.ts --duration 5

python3 tools/verify_ts_klv.py capture.ts --list-all
```

---

## Compliance Evidence Checklist

| Check | Command |
|---|---|
| Build succeeds | `meson compile -C build` |
| All elements visible | `GST_PLUGIN_PATH=build/src gst-inspect-1.0 klvmetaenc` (× 4) |
| Unit tests pass | `meson test -C build` |
| Smoke tests pass | `meson test -C build smoke/ts_roundtrip_python smoke/udp_loopback_python` |
| tags 1-95 roundtrip passes | `python3 examples/test_95_tags.py` |
| PMT signals KLV (`0x06 + KLVA` in current repo) | `python3 tools/verify_ts_klv.py capture.ts --list-all` |
| `metadata_descriptor (0x26)` present | (same verify command) |

---

## Documentation Conventions

- All paths are relative to the repository root.
- `data/stanag4609_tags.ini` is the authoritative source for tag definitions. Edit it to change encoding/decoding behavior.
- JSON examples use numeric string keys: `{"2": ..., "13": ...}`.
- Local set / byte tags use `hex:AABB` or `base64:...` prefixes.

---

## Author

**Mouhsine Kassimi Farhaoui** — [mouhsine98@gmail.com](mailto:mouhsine98@gmail.com)
