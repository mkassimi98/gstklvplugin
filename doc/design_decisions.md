# Design Decision Log

Records key design decisions made in gstklvplugin, the rationale behind each, and the trade-offs accepted.

---

## D001: Flat JSON as the external metadata interface

**Decision:** Use flat JSON objects with numeric string keys for all metadata input and output.

**Rationale:** JSON is easy to generate and inspect, integrates naturally with Python tooling and application-level code, and avoids coupling the plugin API to binary formats. Numeric string keys (`"2"`, `"13"`) directly map to MISB ST 0601 tag IDs.

**Trade-offs:** JSON is not a canonical KLV representation. Binary-to-binary workflows that need to avoid JSON serialization overhead cannot use `klvmetaenc`/`klvmetadec` directly.

---

## D002: Tag registry in an INI file

**Decision:** Store all tag metadata (ID, type, byte width, range, units, encoding notes) in `data/stanag4609_tags.ini` as the single source of truth.

**Rationale:** The registry is human-readable, version-controlled alongside the code, and can be updated without recompilation. Both encoding and decoding behavior are fully driven by this file.

**Trade-offs:** The INI format is simple but not schema-validated. Errors in the INI (e.g., incorrect ranges) will silently produce wrong output.

---

## D003: Local set tags as raw bytes

**Decision:** Tags that represent nested local sets (e.g., Security LS tag 48, RVT tag 73, VMTI tag 74) are treated as opaque byte payloads, not parsed.

**Rationale:** Nested local sets have their own schemas and structure. Implementing a full parser for each would significantly expand scope. Raw byte pass-through lets users inject correct payloads without requiring the plugin to understand the inner structure.

**Impact:** Users must supply `hex:` or `base64:` byte payloads for these tags. Inner structure is not validated.

---

## D004: Per-frame KLV injection via `klvframeinject`

**Decision:** Implement a dedicated `GstElement` with one sink (video) and two source pads (`video_src`, `klv_src`) that emits a matched KLV buffer for every video frame.

**Rationale:** Frame-level synchronization requires tight coupling between video buffer timestamps and the KLV output. A separate element with two source pads is the natural GStreamer idiom for this; it avoids external timing logic in the application.

**Impact:** The element manages tag encoding, checksum insertion, and buffer PTS/DTS stamping internally. The application only needs to set `tags-json` per frame.

---

## D005: BER length encoding (ST 336)

**Decision:** Encode all KLV lengths using BER short/long form as required by SMPTE ST 336.

**Rationale:** ST 336 mandates BER encoding. Short form (1 byte, ≤ 127) is used when possible; long form (2–4 bytes) handles larger payloads.

**Impact:** Payloads larger than 16 MB are rejected with an overflow guard. No indefinite-length BER (not used in KLV).

---

## D006: PMT signaling in-pipeline via `tspmtrewrite`

**Decision:** Rewrite PMT entries in the live pipeline and inject `metadata_descriptor (0x26)` in-pipeline, using `stream_type 0x06 + KLVA` in the current implementation so GStreamer `tsdemux` accepts the KLV PID.

**Rationale:** In-pipeline rewriting means every TS stream produced carries the intended descriptor metadata without requiring any post-processing step. In practice, `0x15` was not decodable by `tsdemux` in this pipeline because `mpegtsmux` was not packaging metadata access units, while `0x06 + KLVA` worked reliably.

**Trade-offs:** This is a deliberate compatibility trade-off between strict ST 1402 wording and working GStreamer interoperability. PMT sections that exceed a single 188-byte TS packet are also not rewritten (they produce a warning).

---

## D007: INI-driven numeric scaling for all tags

**Decision:** All numeric encoding and decoding is driven by the range and type information in `data/stanag4609_tags.ini`, across all four elements.

**Rationale:** Centralizing scaling logic in the INI file ensures consistent behavior between `klvmetaenc`, `klvmetadec`, and `klvframeinject`. It also allows scaling corrections without code changes.

**Previous state (v0.x):** `klvmetaenc` supported numeric scaling only for tags 2/5/13/14/15 with hard-coded logic. This was a known limitation that has been resolved.

---

## D008: Fallback defaults in `klvframeinject`

**Decision:** When `tags-json` is empty, `klvframeinject` falls back to generating tags 2/5/13/14/15 from its individual element properties (`latitude`, `longitude`, `heading`, `altitude`, `timestamp`).

**Rationale:** Enables minimal functional pipelines and quick testing without requiring the application to supply any JSON.

**Impact:** Default pipelines are fully functional with just GStreamer property bindings, with no JSON serialization required on the application side.

---

## D009: Deterministic tag ordering with checksum last

**Decision:** `klvframeinject` sorts tags by ascending tag ID and always appends Tag 1 (BCC-16 checksum) last. `klvmetaenc` preserves JSON input order but also appends Tag 1 last.

**Rationale:** MISB ST 0601 requires Tag 1 to be the last element in the Local Set. Deterministic ordering simplifies debugging and makes binary output reproducible.

**Impact:** `klvmetadec` validates that Tag 1 is the last tag and logs a warning if it is not.

---

## D010: Internal module split — KLV utilities and TS utilities

**Decision:** KLV helpers are isolated in `src/klv/` (with headers in `include/gstklv/internal/`) and TS helpers in `src/ts/`, separately from the plugin implementations in `src/plugins/`.

**Rationale:** Reduces per-file complexity, improves testability of helpers independently of GStreamer element infrastructure, and makes code ownership clearer.

**Impact:** All four elements share the same KLV and TS helper code via internal headers, with no duplication.

---

## D011: C11/GNU11 — no C++ in the plugin

**Decision:** The plugin and all utilities are written in C11/GNU11. No C++ is used in `src/` or `include/`.

**Rationale:** GStreamer is a C library and the GStreamer community recommends C for plugin implementations. Pure C ensures maximum compatibility, simpler build requirements, and suitability for submission to the GStreamer plugin ecosystem.

**Impact:** All C++ STL types (`std::vector`, `std::string`, lambdas) are replaced by GLib equivalents (`GPtrArray`, `GByteArray`, `GHashTable`, named static functions). All internal headers include `extern "C"` guards for optional use from C++ consumers.

---

## D012: PMT rewrite limited to single-packet sections

**Decision:** `tspmtrewrite` only rewrites PMT sections that fit within a single 188-byte TS packet. Multi-packet PMTs are left unchanged.

**Rationale:** Single-packet PMTs cover the common case for standard pipeline configurations. Multi-packet reconstruction adds significant complexity with little practical benefit in typical deployments.

**Impact:** Very large PMTs (with many ES entries) are not rewritten; a `GST_WARNING` is logged. Applications needing this must handle PMT construction externally.

---

## D013: Raw byte injection via `hex:` and `base64:` prefixes

**Decision:** JSON string values starting with `hex:` or `base64:` are decoded to raw bytes before encoding as KLV tag values.

**Rationale:** Allows nested local sets, opaque binary payloads, and non-ASCII string data to be injected via the JSON interface without requiring a separate binary channel.

**Impact:** Users can feed arbitrary byte tags via JSON. The decoder emits `base64:` strings for tags declared as `bytes` in the registry.

---

## D014: GOnce for regex and tag registry initialization

**Decision:** Per-process expensive initialization (GLib GRegex compilation in `klvframeinject`, tag registry loading in all elements) is done exactly once using `GOnce`.

**Rationale:** Compiling regex patterns and parsing the INI file on every buffer or element instantiation would be wasteful. `GOnce` provides thread-safe lazy initialization.

**Impact:** First-call overhead is amortized over the process lifetime. Subsequent calls are lock-free reads of a cached pointer.
