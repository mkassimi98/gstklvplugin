# API Reference — gstklvplugin

\defgroup gstklv Project Overview
\brief High-level documentation for the gstklvplugin repository.

\defgroup gstklv_plugins Plugin Elements
\ingroup gstklv
\brief GStreamer elements provided by the plugin.

\defgroup gstklv_internal Internal Utilities
\ingroup gstklv
\brief Shared internal helpers used by the plugin implementation.

\defgroup gstklv_internal_klv KLV Utilities
\ingroup gstklv_internal
\brief BER, checksum, scaling, JSON, and tag registry helpers.

\defgroup gstklv_internal_ts TS Utilities
\ingroup gstklv_internal
\brief MPEG-TS PSI and CRC helper utilities.

\defgroup gstklv_examples_cpp C++ Examples
\ingroup gstklv
\brief Standalone C++ example applications.

\defgroup gstklv_examples_python Python Examples
\ingroup gstklv
\brief Standalone Python example applications.

\defgroup gstklv_tools Tools
\ingroup gstklv
\brief Helper scripts for validation, documentation, and development workflows.

\defgroup gstklv_tests Tests
\ingroup gstklv
\brief Unit and smoke tests executed from Meson.

This project uses Doxygen to generate a structured API reference from the public headers, internal utility headers, and key source files.

---

## Generating the API Reference

```bash
./scripts/run_doxygen.sh --strict
```

Open the output in a browser:

```
doc/doxygen/html/index.html
```

---

## Element Summary

| Element | Role | Sink Caps | Source Caps | Key Properties |
|---|---|---|---|---|
| `klvmetaenc` | JSON → KLV encoder | `application/json` | `meta/x-klv` | — |
| `klvmetadec` | KLV → JSON decoder | `meta/x-klv` | `application/json` | — |
| `klvframeinject` | Per-frame KLV injector | `video/x-h264`, `video/x-h265` | `video_src`, `klv_src` | `latitude`, `longitude`, `heading`, `altitude`, `timestamp`, `use-system-time`, `tags-json` |
| `tspmtrewrite` | PMT metadata rewriter | `video/mpegts` | `video/mpegts` | `metadata-app-format`, `metadata-app-identifier`, `metadata-format`, `metadata-format-identifier`, `metadata-service-id`, `metadata-flags` |

---

## Module Map

| Module | Location | Purpose |
|---|---|---|
| Plugin elements | `src/plugins/` | GStreamer element implementations |
| KLV utilities | `src/klv/` | BER, checksum, scaling, JSON, tag registry, UL |
| TS utilities | `src/ts/` | PAT/PMT parsing and CRC-32 |
| Public API | `include/gstklv/` | Element type declarations |
| Internal API | `include/gstklv/internal/` | Shared utility headers used by plugins |

---

## Data Flow

### Codec path

\dot
digraph codec_path {
  rankdir=LR;
  node [shape=box, style="rounded,filled", fillcolor="#f2f2f2"];
  json_in [label="application/json"];
  enc [label="klvmetaenc"];
  klv [label="meta/x-klv"];
  dec [label="klvmetadec"];
  json_out [label="application/json"];
  json_in -> enc -> klv -> dec -> json_out;
}
\enddot

### Frame injection path

\dot
digraph frame_injection_path {
  rankdir=LR;
  node [shape=box, style="rounded,filled", fillcolor="#f2f2f2"];
  video [label="video/x-h264 or video/x-h265"];
  inject [label="klvframeinject"];
  video_src [label="video_src"];
  klv_src [label="klv_src (meta/x-klv)"];
  video -> inject;
  inject -> video_src;
  inject -> klv_src;
}
\enddot

### MPEG-TS signaling path

\dot
digraph mpegts_signaling_path {
  rankdir=LR;
  node [shape=box, style="rounded,filled", fillcolor="#f2f2f2"];
  input [label="video + KLV"];
  mux [label="mpegtsmux"];
  rewrite [label="tspmtrewrite"];
  out [label="MPEG-TS\n(current impl: 0x06 + KLVA + metadata_descriptor 0x26)"];
  input -> mux -> rewrite -> out;
}
\enddot

---

## Constraint Table

| Area | Constraint | Location |
|---|---|---|
| KLV Universal Label | MISB ST 0601 16-byte UL required | `include/gstklv/internal/klv_ul.h` |
| BER Lengths | Short and long form (ST 336) | `src/klv/klv_ber.c` |
| Tag 1 (Checksum) | BCC-16, always last in Local Set | `src/klv/klv_checksum.c` |
| Tag registry | INI-driven, loaded lazily | `data/stanag4609_tags.ini` |
| PMT signaling | current impl: `stream_type 0x06` + `KLVA` + `metadata_descriptor (0x26)` | `src/plugins/tspmtrewrite.c` |
| PMT single-packet limit | Sections > 188 bytes not rewritten | `src/plugins/tspmtrewrite.c` |

---

## Doxygen Groups

- `gstklv` — project overview
- `gstklv_plugins` — plugin elements
- `gstklv_internal` — shared internal helpers
- `gstklv_internal_klv` — KLV-focused internal helpers
- `gstklv_internal_ts` — MPEG-TS-focused internal helpers
- `gstklv_examples_cpp` — C++ example applications
- `gstklv_examples_python` — Python example applications
- `gstklv_tools` — helper tools and scripts
- `gstklv_tests` — unit and smoke tests

---

## Doxygen Configuration

| Setting | Value |
|---|---|
| Configuration file | `Doxyfile` |
| Main page | `doc/doxygen_main.md` |
| Output format | HTML only (LaTeX disabled) |
| Input scanning | Recursive over `src/`, `include/`, `doc/`, `tests/`, `examples/`, `tools/`, `scripts/`, and `packaging/` |
| Diagrams | Graphviz (dot) for class and call graphs |

---

## Adding New Modules

1. Create a new `@defgroup` entry in this file.
2. Add `@ingroup <group>` to the relevant headers or source files.
3. Re-run `./scripts/run_doxygen.sh --strict` to update the output.

---

## Related Documentation

- [doc/code_reference.md](code_reference.md) — Prose code map with data flow descriptions
- [doc/standards.md](standards.md) — Standards implemented and conformance scope
- [doc/examples.md](examples.md) — Example workflows
- [doc/docker.md](docker.md) — Container integration and deployment guidance
