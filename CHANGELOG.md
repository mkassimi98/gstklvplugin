# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

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

[Unreleased]: https://github.com/mkassimi98/gstklvplugin/compare/v1.0.1...HEAD
[1.0.1]: https://github.com/mkassimi98/gstklvplugin/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/mkassimi98/gstklvplugin/releases/tag/v1.0.0
