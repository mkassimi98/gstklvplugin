# Security Policy

## Supported Versions

| Version | Supported |
|---|---|
| 1.0.x | Yes |

---

## Reporting a Vulnerability

**Please do not open a public GitHub issue for security vulnerabilities.**

Report security issues privately by emailing:

**[mouhsine98@gmail.com](mailto:mouhsine98@gmail.com)**

Include as much detail as possible:

- A clear description of the vulnerability and its potential impact.
- Steps to reproduce (minimal reproducer preferred).
- Affected version(s) and platform.
- Proof-of-concept or exploit code if available.
- Any mitigations you are aware of.

---

## Response Timeline

| Stage | Target |
|---|---|
| Acknowledgement | Within 48 hours |
| Initial assessment | Within 5 business days |
| Fix or mitigation | Depends on severity; critical issues are prioritized |
| Public disclosure | Coordinated with reporter after fix is available |

We ask that you give me a reasonable window to assess and address the issue before any public disclosure.

---

## Scope

This policy covers vulnerabilities in:

- The `gstklvplugin` shared library (`gstklvplugin.so`) and all plugin elements.
- Internal KLV and MPEG-TS parsing utilities (`src/klv/`, `src/ts/`).
- Build system scripts and installation procedures.

Out of scope:

- Vulnerabilities in third-party dependencies (GStreamer, GLib). Report those upstream.
- Issues in example scripts used only for development and testing.

---

## Notes

This plugin processes binary KLV/MPEG-TS data from external sources. Malformed or adversarially crafted input could trigger parsing issues. Buffer size checks and BER length validation are implemented, but we encourage security-conscious review of any deployment that ingests untrusted streams.
