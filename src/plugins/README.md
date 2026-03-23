# Plugin Source Directory

This directory contains the GStreamer elements that make up `gstklvplugin.so`.

## Files

| File | Element registered | Description |
|---|---|---|
| `plugin.c` | — | Plugin descriptor and `gst_plugin_desc`; registers all four elements |
| `klvencode.c` | `klvmetaenc` | JSON (`application/json`) → KLV (`meta/x-klv`) encoder |
| `klvdecode.c` | `klvmetadec` | KLV (`meta/x-klv`) → JSON (`application/json`) decoder |
| `klvframeinject.c` | `klvframeinject` | Per-frame KLV injector; two source pads (`video_src`, `klv_src`) |
| `tspmtrewrite.c` | `tspmtrewrite` | MPEG-TS PMT rewriter for MISB ST 1402 metadata signaling |

## Shared Utilities

KLV and TS helpers used by these elements live in:

| Location | Headers |
|---|---|
| `src/klv/` | `include/gstklv/internal/klv_*.h` |
| `src/ts/` | `include/gstklv/internal/ts_*.h` |

## Build Output

The Meson build compiles all source files in this directory (plus `src/klv/` and `src/ts/`) into a single shared library:

```
build/src/gstklvplugin.so
```

Installed to: `${libdir}/gstreamer-1.0/gstklvplugin.so`

## See Also

- [doc/code_reference.md](../../doc/code_reference.md) — Detailed module and function map
- [include/gstklv/](../../include/gstklv/) — Public element headers
