# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

## [1.0.2] - 2026-09-04

### Fixed

- Fixed the MISB ST 0601.8 metadata registry (`data/stanag4609_tags.ini`) to match the normative UAS Datalink Local Set tag allocation through Tag 95. The previous registry diverged from ST 0601.8 starting at Tag 81, where several non-ST-0601.8 velocity/angle entries (`Sensor Up Velocity`, `Platform North/East/Up Velocity`, `Alternate Platform North/East/Up Velocity`, `Platform Pitch/Roll/Angle of Attack/Sideslip Angle Full`) displaced the `Image Horizon Pixel Pack` (Tag 81), the eight Full Corner coordinate fields (Tags 82-89), the four Full platform angle fields (Tags 90-93), `MIIS Core Identifier` (Tag 94), and `SAR Motion Imagery Metadata` (Tag 95). The registry now follows the ST 0601.8 allocation through Tag 95, with regression coverage for the corrected entries.
- Corrected three additional ST 0601.8 registry entries found outside the 79-95 range during this audit: Tag 12 (`Image Coordinate System`) is a `String`, not `uint8`; Tag 20 (`Sensor Relative Roll Angle`) is `uint32` mapped `0..360`, not `int32` `+/-180`; and Tags 43/44 (`Target Track Gate Width/Height`) are `uint8` `Pixels 0..512`, not `uint16` `Meters 0..10000`. Also corrected Tags 79/80 (`Sensor North/East Velocity`) range from `+/-327.67` to the standard's exact `+/-327`.
- Removed a hard-coded `tag_id > 93` limit in `klvframeinject` that silently dropped any JSON tag above 93, including the new Tags 94 and 95.
- Tags 81 (`Image Horizon Pixel Pack`, an MISB RP 0701 Floating Length Pack), 94 (`MIIS Core Identifier`, an MISB ST 1204 Binary Value), and 95 (`SAR Motion Imagery Metadata`, a nested MISB ST 1206 Local Set) are not semantically decoded; their payloads are preserved byte-exact as opaque `bytes` values, documented in `doc/klv_tags.md` and `doc/standards.md`.
- Documented an inconsistency found in MISB ST 0601.8 (23 Oct 2014) Section 8.93 itself: Tag 93's summary "Range" header states `+/-180` while its own Notes/Conversion Formula state `+/-90` (matching Tags 90-92). `gstklvplugin` implements `+/-90` for Tag 93, consistent with Tags 90-92 and Tag 93's own mapping notes; see `doc/standards.md`.

### Changed

- Documentation and examples no longer claim a "93-tag" ST 0601.8 registry; corrected to reflect MISB ST 0601.8 tags 1-95.
- Renamed example files and docs that referenced the old 93-tag count: `examples/test_93_tags.py` -> `examples/test_95_tags.py`, `examples/*/{cpp,python}/*_93tags.{py,cpp}` -> `*_95tags.{py,cpp}`, and `doc/93_tags.md` -> `doc/95_tags.md`. Their content now generates/consumes the corrected tags 1-95 registry.

## [1.0.1] - 2026-09-04

### Changed

- Project license changed from MIT to GNU AGPL-3.0.

### Fixed

- Corrected the MISB ST 0601.8 UAS Local Set Universal Key, rejected the non-conformant key emitted by v1.0.0, and added full 16-byte conformance coverage ([#2](https://github.com/mkassimi98/gstklvplugin/issues/2)).

## [1.0.0] - 2026-04-06

### Added

- Initial stable release of `gstklvplugin` as a GStreamer 1.x plugin suite written in C11/GNU11 for end-to-end KLV metadata workflows.
- Four production elements: `klvmetaenc`, `klvmetadec`, `klvframeinject`, and `tspmtrewrite`.
- Full MISB ST 0601.8 local-set coverage driven by `data/stanag4609_tags.ini`, including BER lengths, MISB UL handling, checksum generation/validation, INI-driven scaling, and `hex:` / `base64:` support for byte-oriented tags.
- Internal KLV helper libraries in `src/klv/` for BER, checksum, JSON parsing, scaling, tag-definition loading, and UL constants.
- Internal MPEG-TS helper libraries in `src/ts/` for CRC-32/MPEG-2, PAT/PMT parsing, and PMT section rebuilding.
- Meson as the primary build, test, and install workflow, plus CMake support for alternative builds and the C++ example programs.
- Python examples for local TS roundtrip, SRT sender/receiver, and UDP sender/receiver.
- C++ examples for TS roundtrip, SRT sender/receiver, and UDP sender/receiver.
- Bash workflow helpers for development shells, full validation, installation, and strict Doxygen generation.
- Debian-style packaging support via `packaging/deb/build_deb.sh`, including staged installs and `.deb` generation for Debian-family x86 and ARM systems such as Raspberry Pi OS.
- Installed project documentation, Doxygen coverage across code and support scripts, and licensing metadata.

### Changed

- `tspmtrewrite` now documents and implements the transport signaling actually used by this repository: KLV is carried as `stream_type 0x06` with `registration_descriptor` `KLVA` plus `metadata_descriptor (0x26)` for practical GStreamer `tsdemux` compatibility.
- SRT examples and guides were aligned with the live configuration that decodes reliably in practice: `mpegtsmux alignment=7`, `srtsrc blocksize=1316`, and `latency=125`.

### Fixed

- `verify_ts_klv.py` now accepts both legacy `0x15` captures and the current `0x06 + KLVA` signaling used by this repository.
- The TS readers tolerate real-world `tsdemux` behavior where KLV may arrive without the full MISB UL in the demuxed payload, and rebuild the expected KLV buffer before decoding.
- The installed-plugin validation path now covers clean-registry discovery so blacklist and stale-registry issues can be reproduced and checked explicitly.

### Testing

- Meson `gst-check` suites cover the four public elements plus KLV and TS support libraries.
- Smoke coverage includes Python TS roundtrip, UDP loopback, staged installed-plugin discovery with a clean GStreamer registry, and C++ TS roundtrip when `cmake` is available.
- Validation helpers also run Python syntax checks, shell syntax checks, and strict Doxygen generation so the release can be checked end to end with a single command.

[Unreleased]: https://github.com/mkassimi98/gstklvplugin/compare/v1.0.2...HEAD
[1.0.2]: https://github.com/mkassimi98/gstklvplugin/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/mkassimi98/gstklvplugin/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/mkassimi98/gstklvplugin/releases/tag/v1.0.0
