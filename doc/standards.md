# Standards and Implementation Notes

This document explains which standards are implemented, the rationale behind each choice, and how the codebase maps to concrete standard requirements.

---

## Standards Implemented

| Standard | Scope | Implementation |
|---|---|---|
| **SMPTE ST 336** | KLV Key-Length-Value encoding | `src/klv/klv_ber.c`, `src/klv/klv_ul.c` |
| **MISB ST 0601.8** | UAS Datalink Local Set (tags 1-95) | `data/stanag4609_tags.ini`, `src/plugins/klvencode.c`, `src/plugins/klvdecode.c` |
| **STANAG 4609** | MPEG-TS motion imagery transport | `examples/`, pipeline composition |
| **MISB ST 1402** | MPEG-TS metadata PMT signaling | `src/plugins/tspmtrewrite.c`, `tools/verify_ts_klv.py` |

---

## Why These Standards

- **ST 336 (KLV)** is the foundational binary encoding — compact, self-describing, and extensible.
- **ST 0601.8** is the accepted local set for UAS motion imagery metadata, with well-defined tags 1-95 covering position, attitude, sensor, and mission data.
- **STANAG 4609** defines the transport context (MPEG-TS) and metadata synchronization conventions used across NATO and allied systems.
- **ST 1402** specifies how metadata streams must be signaled in MPEG-TS PMT so that demultiplexers and analyzers can discover and identify the KLV stream without prior knowledge of PIDs.

---

## Source of Truth for Tags

All tag definitions are centralized in:

```
data/stanag4609_tags.ini
```

Installed builds use:

```
${prefix}/share/gstklvplugin/stanag4609_tags.ini
```

This file contains, for each tag:

- Tag ID and name
- Data type (`uint`, `int`, `float`, `String`, `bytes`, `uint64`)
- Byte length
- Physical range (`min..max` or `+/- X`)
- Units
- Encoding notes
- Scale hints

All encoding and decoding behavior is driven by this file. To modify tag behavior, edit the INI and do not need to recompile.

The tag registry table is rendered at [doc/klv_tags.md](klv_tags.md).

---

## KLV Packet Structure (ST 336 + ST 0601.8)

Every KLV packet emitted by this plugin has the following layout:

```mermaid
flowchart TB
    subgraph PKT["KLV Packet"]
        A["Universal Label (16 bytes)<br/>06 0E 2B 34 02 0B 01 01<br/>0E 01 03 01 01 00 00 00"]
        B["BER Length (1-4 bytes)<br/>Short form: 1 byte<br/>Long form: 0x82 / 0x83 + bytes"]
        subgraph C["Local Set TLV entries"]
            D["Tag (1 byte)<br/>Length (BER, 1-4 bytes)<br/>Value (Length bytes)"]
        end
        E["Tag 1 - BCC-16 checksum (last)<br/>01 02 &lt;cs_hi&gt; &lt;cs_lo&gt;"]
        A --> B --> C --> E
    end
```

---

## Encoding Rules

### BER Length Encoding

All KLV lengths use BER encoding as required by ST 336:

- Lengths ≤ 127: encoded as a single byte.
- Lengths 128–255: `0x81 <length>`.
- Lengths 256–65535: `0x82 <hi> <lo>`.
- Lengths > 16 MB: rejected (BER overflow guard).

### Checksum (Tag 1 — BCC-16)

- Tag 1 is always the last TLV in the Local Set.
- BCC-16 is computed over all bytes from the start of the UL through the Tag 1 length byte (inclusive), excluding the two checksum value bytes.
- `klvmetadec` verifies the checksum on decode and logs a `GST_WARNING` on mismatch.
- `klvmetadec` also validates that Tag 1 is the last tag in the Local Set.

### Numeric Scaling

Tags with a defined `range` in the INI are encoded as scaled integers:

```
raw = round((value - min) / (max - min) * (2^bits - 1))   # unsigned
raw = round(value / max * (2^(bits-1) - 1))                # signed (+/- range)
```

Decoding applies the inverse transform. Tags without a range are encoded as raw integers.

Special cases:

- **Tag 2** (UNIX Time Stamp) and **Tag 72** (Event Start Time): 8-byte raw `uint64`, microseconds since epoch. No scaling.
- **String tags**: encoded as ASCII, up to 127 bytes.
- **Local set / opaque payload tags** (48, 66, 73, 74, 81, 94, 95): treated as raw byte payloads; must be supplied as `hex:` or `base64:` strings. See "ST 0601.8 Opaque Payload Tags" below.

### Tag Ordering

- `klvframeinject` sorts tags by ascending tag ID, with Tag 1 (checksum) always last.
- `klvmetaenc` preserves JSON input order; Tag 1 is always appended last regardless.

---

## ST 0601.8 Opaque Payload Tags

Three tags in the corrected tags 1-95 registry reference other MISB
standards that gstklvplugin does not semantically decode. Their payloads
are preserved byte-exact (encode input equals decode output) but their
internal structure is opaque to this plugin:

| Tag | Name | References | Structure |
|---|---|---|---|
| 81 | Image Horizon Pixel Pack | MISB RP 0701 (Floating Length Pack) | Variable-length: Start x0/y0, End x1/y1 (Uint8, percent), optionally followed by Start/End Latitude/Longitude (Int32) |
| 94 | MIIS Core Identifier | MISB ST 1204 | ST 1204 Binary Value only (no ST 1204 key/length) |
| 95 | SAR Motion Imagery Metadata | MISB ST 1206 | Nested ST 1206 Local Set |

Supply these as `hex:` or `base64:` strings in JSON input; they are
returned as `base64:` strings on decode. gstklvplugin does not implement
RP 0701, ST 1204, or ST 1206 parsing.

### Known ST 0601.8 Document Inconsistency — Tag 93

MISB ST 0601.8 (23 Oct 2014), Section 8.93, lists Tag 93 (Platform
Sideslip Angle, Full)'s summary "Range" header as `+/- 180`, but the same
section's Notes and Conversion Formula state "Map -(2^31-1)..(2^31-1) to
+/-90" — identical wording and formula intent to Tags 90-92 (Platform
Pitch/Roll/Angle-of-Attack, Full), which are unambiguously `+/-90` in both
places. This is an inconsistency within the normative document itself, not
an implementation choice.

gstklvplugin encodes/decodes Tag 93 with range `+/-90`, for consistency
with Tags 90-92 and Tag 93's own mapping notes. This is documented rather
than silently resolved, per the project's policy of not guessing when the
standard is internally ambiguous.

### Tag 2 Naming — "UNIX Time Stamp" vs. "Precision Time Stamp"

ST 0601.8's tag table names Tag 2 "UNIX Time Stamp" (`uint64`,
microseconds since epoch); "Precision Time Stamp" is used in the
standard's prose only as the conceptual/system-level term for the
timestamping mechanism Tag 2 implements (see ST 0601.8 Section 6.1 and
requirement ST0601.8-10). The registry's "UNIX Time Stamp" name matches
the tag table verbatim; no change was made.

---

## Decoding Rules

`klvmetadec` applies the following logic per tag:

| Tag type | Decode behavior |
|---|---|
| Numeric with `range` | Inverse range scaling to physical units |
| Signed without `range` | Sign extension, emit as integer |
| `uint64` (tags 2, 72) | Raw 8-byte unsigned integer, microseconds |
| `String` | UTF-8 decode; non-UTF-8 → `base64:` string |
| `bytes` / local set | `base64:` encoded string |
| Tag 1 (checksum) | Verified; not included in JSON output |

---

## JSON Format

Input and output JSON is a flat object with numeric string keys:

```json
{
  "2":  1770260492651783,
  "5":  34.5,
  "13": 40.63919,
  "14": -73.92060,
  "15": 486.7
}
```

Raw byte tags use prefixed strings:

```json
{
  "48": "base64:AQEA",
  "73": "hex:01020304"
}
```

---

## MPEG-TS Signaling (MISB ST 1402)

`tspmtrewrite` is placed immediately after `mpegtsmux` in the sender pipeline. In the current implementation it ensures:

- The KLV elementary stream uses `stream_type = 0x06`.
- A `registration_descriptor (0x05)` is present with identifier `KLVA`.
- A `metadata_descriptor (0x26)` is present with the configured fields.
- The PMT version number is incremented on each rewrite.
- The continuity counter is maintained and incremented modulo 16.

Why `0x06` and not `0x15` here: GStreamer `mpegtsmux` is not wrapping these KLV buffers as MPEG metadata access units, so `tsdemux` rejects plain `0x15` streams. `0x06 + KLVA` keeps the KLV PID consumable by GStreamer while preserving the descriptor fields.

### Default Descriptor Fields

| Field | Default |
|---|---|
| `metadata_application_format` | `0xFFFF` (use identifier) |
| `metadata_application_format_identifier` | `MISB` |
| `metadata_format` | `0xFF` (use identifier) |
| `metadata_format_identifier` | `KLVA` |
| `metadata_service_id` | `0x01` |
| `metadata_flags` | `0x00` |

All fields are configurable via `tspmtrewrite` element properties.

### Expected `metadata_descriptor` Bytes (Defaults)

Payload (13 bytes):
```
FF FF 4D 49 53 42 FF 4B 4C 56 41 01 00
```

Full descriptor (tag + length + payload):
```
26 0D FF FF 4D 49 53 42 FF 4B 4C 56 41 01 00
```

See [doc/compliance_appendix.md](compliance_appendix.md) for the full PMT ES entry bytes.

---

## Conformance Scope and Limitations

| Area | Status |
|---|---|
| MISB ST 0601.8 UL | Fully implemented |
| BER length encoding (ST 336) | Fully implemented (short and long form) |
| BCC-16 checksum (Tag 1) | Generated by encoder; verified by decoder |
| ST 0601.8 Local Set (tags 1-95) | All tags supported by `klvframeinject`; see INI for types |
| Numeric scaling | Fully INI-driven for all tags |
| Local set (nested) tags | Raw bytes only; inner parsing not implemented (Tags 48, 66, 73, 74, 81, 94, 95) |
| RP 0701 / ST 1204 / ST 1206 | Not implemented; Tags 81/94/95 preserved as opaque bytes |
| Tag 93 range | `+/-90`; ST 0601.8 Section 8.93 has an internal `+/-180` vs. `+/-90` inconsistency, see above |
| Multi-byte tag IDs | Not implemented (all current ST 0601 tags are 1 byte) |
| PMT sections > 188 bytes | Not rewritten; a warning is logged |
| STANAG 4609 multi-stream | Single KLV PID per PMT |

---

## Verification

### PMT signaling

```bash
# Capture from running SRT sender
python3 tools/capture_ts_from_srt.py \
  --host 127.0.0.1 --port 5000 \
  --output capture.ts --duration 5

# Verify PMT entries
python3 tools/verify_ts_klv.py capture.ts --list-all
```

Expected output indicators:

- `stream_type: 0x06` for the current repo
- `registration_descriptor: KLVA`
- `metadata_descriptor: present`

### Operational checklist

- Build succeeds and `gstklvplugin.so` is produced.
- `gst-inspect-1.0` shows all four elements with correct caps and properties.
- SRT sender/receiver example runs end-to-end.
- PMT verification confirms KLV signaling and `metadata_descriptor`.
- `meson test` passes all gst-check unit tests.

---

## Official References

- GStreamer: https://gstreamer.freedesktop.org/
- MISB / Motion Imagery Standards Board: https://gwg.nga.mil/gwg/focus-groups/Motion_Imagery_Standards_Board_%28MISB%29.html
- SMPTE standards index (includes ST 336): https://www.smpte.org/standards/document-index/st
- RFC 6597 (public KLV overview): https://www.rfc-editor.org/info/rfc6597
- SRT Alliance: https://www.srtalliance.org/
- Haivision SRT reference implementation: https://github.com/Haivision/srt
