# Compliance Appendix: MPEG-TS Metadata Signaling

This appendix documents the PMT descriptor bytes produced by `tspmtrewrite` in its default configuration and clarifies the difference between the standards target and the current implementation used by this repo.

---

## Scope

- MPEG-TS PMT metadata signaling for KLV streams
- Current repo behavior: `stream_type 0x06`
- `registration_descriptor (0x05)` with identifier `KLVA`
- `metadata_descriptor (0x26)` with MISB/KLVA identifiers

Standards note:

- MISB ST 1402 / ISO 13818-1 describe metadata carriage with `stream_type 0x15`
- This repo currently emits `0x06 + KLVA + metadata_descriptor` because that is what GStreamer `tsdemux` accepts for the raw KLV emitted by these pipelines

---

## Default `tspmtrewrite` Property Values

| Property | Default |
|---|---|
| `metadata-app-format` | `0xFFFF` |
| `metadata-app-identifier` | `MISB` |
| `metadata-format` | `0xFF` |
| `metadata-format-identifier` | `KLVA` |
| `metadata-service-id` | `0x01` |
| `metadata-flags` | `0x00` |

---

## Descriptor Bytes

### `metadata_descriptor (0x26)` payload

```
FF FF 4D 49 53 42 FF 4B 4C 56 41 01 00
```

### Full `metadata_descriptor`

```
26 0D FF FF 4D 49 53 42 FF 4B 4C 56 41 01 00
```

### `registration_descriptor (0x05)` for KLVA

```
05 04 4B 4C 56 41
```

---

## Example PMT ES Entry

For a KLV stream on PID `0x0042`, the current repo emits:

```
06 E0 42 F0 15 05 04 4B 4C 56 41 26 0D FF FF 4D 49 53 42 FF 4B 4C 56 41 01 00
```

Breakdown:

- `06`: `stream_type`
- `E0 42`: elementary PID `0x0042`
- `F0 15`: ES info length `0x15`
- `05 04 4B 4C 56 41`: `registration_descriptor(KLVA)`
- `26 0D ...`: `metadata_descriptor`

---

## Verification Procedure

### Step 1 — Start the sender

```bash
export GST_PLUGIN_PATH="$PWD/build/src:$GST_PLUGIN_PATH"
python3 examples/srt-pipelines/python/srt_sender_95tags.py \
  --host 0.0.0.0 --port 5000 --count 0
```

### Step 2 — Capture a short TS

```bash
python3 tools/capture_ts_from_srt.py \
  --host 127.0.0.1 --port 5000 \
  --output examples/ts/recordings/capture.ts --duration 5
```

### Step 3 — Verify PMT signaling

```bash
python3 tools/verify_ts_klv.py examples/ts/recordings/capture.ts --list-all
```

Expected indicators with the current repo:

```text
KLV metadata streams:
  PID 0x00XX stream_type 0x06 (private PES + KLVA)
    registration: KLVA
    metadata_descriptor: present
```

`verify_ts_klv.py` also accepts legacy `0x15` captures.

---

## Custom Descriptor Configuration

To verify with non-default values, pass `--metadata-*` options to the sender:

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

The descriptor payload bytes change accordingly when any field is modified.

---

## Notes

- Descriptor values are controlled entirely by `tspmtrewrite` properties.
- The standards-facing `0x15` wording still matters for interoperability discussions, but the repo's current implementation is intentionally `0x06 + KLVA`.
- The `registration_descriptor` (`KLVA`) is what lets `tsdemux` recognize and route the KLV PID in these examples.
