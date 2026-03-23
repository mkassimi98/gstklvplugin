# Python Implementation Notes

This document explains why the Python SRT scripts are the primary reference implementation.

## Pipeline Overview

Sender pipeline:

```mermaid
flowchart LR
    A["videotestsrc"] --> B["capsfilter"]
    B --> C["x264enc"]
    C --> D["h264parse"]
    D --> E["klvframeinject"]
    E -->|video_src| F["mpegtsmux<br/>alignment=7"]
    E -->|klv_src| G["meta/x-klv"]
    G --> F
    F --> H["tspmtrewrite"]
    H --> I["srtsink"]
```

Receiver pipeline:

```mermaid
flowchart LR
    A["srtsrc<br/>blocksize=1316<br/>latency=125"] --> B["tsdemux"]
    B -->|video/x-h264| C["h264parse"]
    C --> D["avdec_h264"]
    D --> E["videoconvert"]
    E --> F["ximagesink / fakesink"]
    B -->|meta/x-klv| G["klvmetadec"]
    G --> H["JSON output"]
```

## Why Python

- Deterministic element wiring with explicit error checks
- Robust logging and structured output
- Easier extension for metadata and local set testing
- The transport tuning that fixed live SRT video (`alignment=7`, `blocksize=1316`) landed here first

## Legacy Bash Scripts

The shell scripts are kept for quick smoke tests, but the Python scripts are the reference for correctness and compliance.

## Related Docs

- `doc/srt_pipelines.md`
- `doc/93_tags.md`
- `README.md`

Author: Mouhsine Kassimi Farhaoui  
Mail: mouhsine98@gmail.com
