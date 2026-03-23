# C++ Examples

This page describes the C++ network and TS reference implementations.

## Overview

The C++ examples now cover three workflows:

- SRT sender/receiver in `examples/srt-pipelines/cpp/`
- UDP sender/receiver in `examples/udp-pipelines/cpp/`
- TS recorder/reader in `examples/ts/cpp/`

All of them use the same core plugin contract:

- Generate or consume flat JSON with numeric string keys
- Inject metadata with `klvframeinject`
- Carry KLV in MPEG-TS
- Decode JSON with `klvmetadec`

---

## Quick Start

Build the examples:

```bash
cmake -S . -B build -DGSTKLVPLUGIN_BUILD_EXAMPLES=ON
cmake --build build
```

These example binaries are currently CMake targets. Meson builds the plugin
and tests, but does not emit the C++ example executables.

### SRT

```bash
./build/gstklv_srt_receiver --host 127.0.0.1 --port 5000 --print-all
./build/gstklv_srt_sender   --host 0.0.0.0   --port 5000 --count 50
```

### UDP

```bash
./build/gstklv_udp_receiver --host 0.0.0.0   --port 5000 --print-all
./build/gstklv_udp_sender   --host 127.0.0.1 --port 5000 --count 50
```

### TS Files

```bash
./build/gstklv_ts_recorder --output examples/ts/recordings/capture.ts --count 50
./build/gstklv_ts_reader   --input examples/ts/recordings/capture.ts --print-all
```

---

## Network Notes

### SRT

The C++ SRT examples were updated to match the Python fixes that made video playback reliable:

- Sender sets `mpegtsmux alignment=7`
- Sender configures the encoder/parser for repeated SPS/PPS and AUD
- Receiver uses `srtsrc blocksize=1316 latency=125`
- Receiver uses `avdec_h264` directly instead of `decodebin`
- Receiver keeps the video sink and KLV handoff sink clock-synchronised, so terminal prints track the frame being shown

### UDP

The C++ UDP examples are transport-equivalent to the SRT ones:

- Sender uses `udpsink`
- Receiver uses `udpsrc` with MPEG-TS caps
- PMT/KLV handling is identical to the SRT examples
- Receiver-side sync behavior is also identical to the SRT examples

---

## PMT Signaling

The current implementation of `tspmtrewrite` advertises KLV as:

- `stream_type = 0x06`
- `registration_descriptor = KLVA`
- `metadata_descriptor (0x26) = present`

That is intentional for GStreamer compatibility. The examples still expose the ST 1402 descriptor fields through the `--metadata-*` options on the senders.

---

## Options Reference

### Network senders

| Option | Description | Typical default |
| --- | --- | --- |
| `--host` | Destination host (UDP) or bind/listener host (SRT) | transport-specific |
| `--port` | Network port | `5000` |
| `--fps` | Video framerate | `2` |
| `--bitrate` | H.264 bitrate (kbps) | `2000` |
| `--count` | Frame count (0 = infinite) | `0` |
| `--tags-ini` | Path to `stanag4609_tags.ini` | `data/stanag4609_tags.ini` |
| `--plugin-path` | Override `GST_PLUGIN_PATH` | `<repo>/build` |
| `--seed` | RNG seed for deterministic output | unset |
| `--metadata-app-format` | `metadata_application_format` | `0xFFFF` |
| `--metadata-app-identifier` | `metadata_application_format_identifier` | `MISB` |
| `--metadata-format` | `metadata_format` | `0xFF` |
| `--metadata-format-identifier` | `metadata_format_identifier` | `KLVA` |
| `--metadata-service-id` | `metadata_service_id` | `0x01` |
| `--metadata-flags` | `decoder_config_flags` | `0x00` |

### Network receivers

| Option | Description | Typical default |
| --- | --- | --- |
| `--host` | Connect host (SRT) or listen host (UDP) | transport-specific |
| `--port` | Network port | `5000` |
| `--headless` | Disable video output | disabled |
| `--video-sink` | Video sink element override | auto / preferred X11 sink |
| `--print-all` | Print all tags | enabled |
| `--output` | Append raw JSON to a file | `-` |
| `--tags-ini` | Path to `stanag4609_tags.ini` | `data/stanag4609_tags.ini` |
| `--plugin-path` | Override `GST_PLUGIN_PATH` | `<repo>/build` |

---

## TS Recording and Playback

Source files live in `examples/ts/cpp/`.

The recorder writes MPEG-TS with KLV and the reader:

- plays video via `tsdemux -> h264parse -> decoder -> videosink`
- scans the TS file PMT/PES directly to find the KLV PID
- restores the UL prefix when PES payloads arrive without it
- paces the recovered KLV from PES PTS before feeding it into `klvmetadec`

Practical note:

- On some X11 sinks the reader can keep the window open after the last frame has been shown. For scripted checks, wrap it with `timeout`; for interactive use, `Ctrl+C` is fine after the last frame.

Useful TS reader options:

| Option | Description | Default |
| --- | --- | --- |
| `--video-sink` | Video sink element | `autovideosink` |
| `--video-decoder` | Video decoder element | `avdec_h264` |
| `--no-pace` | Disable playback-timed KLV pacing and dump metadata as fast as possible | disabled |

---

## Notes

- The senders emit demo values based on INI ranges; they are test/reference programs, not platform telemetry models.
- Byte tags are represented as deterministic `hex:` payloads for clarity.
- If you use a non-default build directory, pass `--plugin-path` or export `GST_PLUGIN_PATH`.
