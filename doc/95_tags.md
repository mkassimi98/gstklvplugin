# MISB ST 0601.8 — Full Tags 1-95 Workflow

Documents the complete MISB ST 0601.8 (UAS Datalink Local Set, tags 1-95) workflow, including tag generation, local set handling, network streaming, and PMT verification.

See [doc/klv_tags.md](klv_tags.md) for the full corrected tag registry table.

---

## What Is Covered

- Local end-to-end validation (`examples/test_95_tags.py`)
- SRT and UDP sender/receiver workflows exercising all 95 tags
- Local set tags as raw byte payloads
- MPEG-TS PMT metadata signaling via `tspmtrewrite`

---

## Tag Coverage

The example suite exercises all 95 tags defined in MISB ST 0601.8. Tag values are generated using types and ranges from `data/stanag4609_tags.ini`:

| Tag type | Generation strategy |
|---|---|
| Numeric (uint, int, float) | Random value within the INI-defined range |
| String | Short ASCII value |
| uint64 (tags 2, 72) | Current system time in microseconds |
| Byte / local set | Deterministic demo payload (or user-supplied via `--local-set`) |
| Tag 1 (Checksum) | Always computed and appended by `klvframeinject` |

---

## Prerequisites

```bash
sudo apt-get install -y \
  gstreamer1.0-tools \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-plugins-ugly \
  python3 python3-gi

# Build the plugin
meson setup build && meson compile -C build
export GST_PLUGIN_PATH="$PWD/build/src:$GST_PLUGIN_PATH"
```

---

## Local Validation (No Network)

Validate the full JSON → KLV → JSON roundtrip for all 95 tags without any network:

```bash
python3 examples/test_95_tags.py
```

This encodes a complete tag set using `klvmetaenc`, decodes it with `klvmetadec`, and verifies that all tags survive the roundtrip with correct values (within quantization tolerance for scaled tags).

---

## SRT Streaming — All 95 Tags

### Step 1 — Start the receiver

```bash
python3 examples/srt-pipelines/python/srt_receiver_95tags.py \
  --host 127.0.0.1 --port 5000
```

### Step 2 — Start the sender

```bash
python3 examples/srt-pipelines/python/srt_sender_95tags.py \
  --host 0.0.0.0 --port 5000 --count 50
```

The sender prints a per-frame tag summary. The receiver prints decoded tags with names, values, and units from the INI registry. Each frame is separated by a divider line.

Important live transport details:

- `mpegtsmux alignment=7`
- `srtsrc blocksize=1316 latency=125`
- receiver output is now clock-synchronised with the presented video, so KLV prints follow the frame on screen instead of a separate fast metadata path
- `--print-summary` is the best mode when you want the terminal output to stay visually aligned with playback

Those settings are what made the SRT examples decode live video reliably while still carrying KLV.

---

## Local Set Tags

Several ST 0601.8 tags represent nested local sets. These must be supplied as raw byte payloads:

| Tag | Name |
|---|---|
| 48 | Security Local Set |
| 66 | Target Location Covariance Matrix |
| 73 | RVT Local Data Set |
| 74 | VMTI Local Data Set |
| 81 | Image Horizon Pixel Pack (Floating Length Pack, not semantically decoded) |
| 94 | MIIS Core Identifier (ST 1204 Binary Value, not semantically decoded) |
| 95 | SAR Motion Imagery Metadata (nested ST 1206 Local Set, not semantically decoded) |

### Supply a specific local set tag

```bash
python3 examples/srt-pipelines/python/srt_sender_95tags.py \
  --host 0.0.0.0 --port 5000 \
  --local-set 48=hex:01020304 \
  --local-set 73=hex:AABBCCDD
```

### Load from a binary file

```bash
--local-set 48=@/path/to/security_ls.bin
```

### Use demo payloads for all local set tags

```bash
python3 examples/srt-pipelines/python/srt_sender_95tags.py \
  --host 0.0.0.0 --port 5000 \
  --local-set-demo
```

Demo payloads are minimal, structurally valid byte sequences suitable for signaling and transport testing.

---

## PMT Metadata Signaling (ST 1402)

The sender includes `tspmtrewrite` in its pipeline. In the current implementation the PMT signals KLV as:

- `stream_type = 0x06`
- `registration_descriptor (0x05)`: `KLVA`
- `metadata_descriptor (0x26)`: present with MISB/KLVA identifiers

### Override descriptor fields

```bash
python3 examples/srt-pipelines/python/srt_sender_95tags.py \
  --host 0.0.0.0 --port 5000 \
  --metadata-app-format 0xFFFF \
  --metadata-app-identifier MISB \
  --metadata-format 0xFF \
  --metadata-format-identifier KLVA \
  --metadata-service-id 0x01 \
  --metadata-flags 0x00
```

### Verify PMT signaling

```bash
python3 tools/capture_ts_from_srt.py \
  --host 127.0.0.1 --port 5000 \
  --output examples/ts/recordings/capture.ts --duration 5

python3 tools/verify_ts_klv.py examples/ts/recordings/capture.ts --list-all
```

For expected byte values, see [doc/compliance_appendix.md](compliance_appendix.md).

---

## Verification Checklist

| Check | Expected result |
|---|---|
| Sender prints per-frame tag list | 95 tags per frame with values |
| Receiver prints per-frame tag list | 95 decoded tags with names and units |
| Tag names match INI registry | Names from `data/stanag4609_tags.ini` |
| PMT `stream_type` | `0x06` in the current repo |
| `metadata_descriptor` | Present with MISB/KLVA identifiers |
| Roundtrip validation | `examples/test_95_tags.py` passes |

---

## Troubleshooting

| Symptom | Resolution |
|---|---|
| No tags on receiver | Start receiver before sender; check `GST_PLUGIN_PATH` |
| Tags appear truncated | Receiver may have been terminated before EOS |
| Receiver prints KLV but no video | Check `mpegtsmux alignment=7`, `srtsrc blocksize=1316 latency=125`, and keep the receiver decode path as `h264parse ! avdec_h264 ! videoconvert ! videosink` |
| Prints do not look aligned with the visible frame | Prefer `--print-summary` and keep the current receiver sync settings; do not reintroduce a leaky video-only queue |
| PMT shows unexpected signaling | Verify `tspmtrewrite` is in the sender pipeline and inspect with `tools/verify_ts_klv.py` |
| Local set tags missing | Supply byte payloads with `--local-set` or `--local-set-demo` |
| Tag value mismatch | Scaled tags may differ by quantization error — this is expected |
| INI not found | Set `KLV_TAGS_INI=$PWD/data/stanag4609_tags.ini` |

---

## Related Documentation

- [doc/srt_pipelines.md](srt_pipelines.md) — SRT pipeline composition and options
- [doc/udp_pipelines.md](udp_pipelines.md) — UDP pipeline composition and options
- [doc/compliance_appendix.md](compliance_appendix.md) — PMT descriptor bytes
- [doc/standards.md](standards.md) — Encoding and decoding rules
- [doc/klv_tags.md](klv_tags.md) — Full tag registry table
