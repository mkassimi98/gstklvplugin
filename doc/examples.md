# Examples

Working end-to-end example workflows for gstklvplugin. The focus is on reproducible pipelines rather than minimal snippets.

---

## Directory Map

```
examples/
├── test_93_tags.py                      Local 93-tag roundtrip validation
├── setup-env.sh                         Optional shell environment helper
├── srt-pipelines/
│   ├── python/
│   │   ├── srt_sender_93tags.py         SRT sender — Python
│   │   └── srt_receiver_93tags.py       SRT receiver — Python
│   └── cpp/
│       ├── srt_sender_93tags.cpp        SRT sender — C++
│       └── srt_receiver_93tags.cpp      SRT receiver — C++
├── udp-pipelines/
│   ├── python/
│   │   ├── udp_sender_93tags.py         UDP sender — Python
│   │   └── udp_receiver_93tags.py       UDP receiver — Python
│   └── cpp/
│       ├── udp_sender_93tags.cpp        UDP sender — C++
│       └── udp_receiver_93tags.cpp      UDP receiver — C++
└── ts/
    ├── python/
    │   ├── klv_recorder.py              File-based TS recorder — Python
    │   └── klv_video_reader.py          File-based TS reader — Python
    ├── cpp/
    │   ├── ts_recorder_93tags.cpp       File-based TS recorder — C++
    │   └── ts_video_reader_93tags.cpp   File-based TS reader — C++
    └── recordings/                      Output directory for .ts captures
```

---

## 1. Local Validation (No Network)

**`examples/test_93_tags.py`**

Validates the full JSON -> KLV -> JSON roundtrip for all 93 MISB ST 0601.8 tags using `klvmetaenc` and `klvmetadec`.

```bash
export GST_PLUGIN_PATH="$PWD/build/src:$GST_PLUGIN_PATH"
python3 examples/test_93_tags.py
```

---

## 2. SRT Streaming — Python

Receiver first:

```bash
python3 examples/srt-pipelines/python/srt_receiver_93tags.py \
  --host 127.0.0.1 --port 5000
```

Then sender:

```bash
python3 examples/srt-pipelines/python/srt_sender_93tags.py \
  --host 0.0.0.0 --port 5000 --count 50
```

Important transport details:

- Sender uses `mpegtsmux alignment=7`.
- Sender uses `srtsink latency=125 sync=false async=false wait-for-connection=false`.
- Receiver uses `srtsrc blocksize=1316 latency=125`.
- Receiver keeps the video sink and KLV handoff sink clocked, so printed tags follow the displayed frame.
- `--print-summary` is the best live-inspection mode when you care about visual sync.

That combination is what made live H.264 video decode reliably while still receiving KLV.

---

## 3. SRT Streaming — C++

```bash
cmake -S . -B build -DGSTKLVPLUGIN_BUILD_EXAMPLES=ON
cmake --build build
```

These binaries are currently produced by CMake. Meson remains the primary
build for the plugin and tests, but it does not generate the C++ examples.

Receiver first:

```bash
./build/gstklv_srt_receiver --host 127.0.0.1 --port 5000
```

Then sender:

```bash
./build/gstklv_srt_sender --host 0.0.0.0 --port 5000 --count 50
```

The C++ SRT examples now mirror the same transport tuning as the Python ones: `alignment=7` on `mpegtsmux`, `blocksize=1316` on `srtsrc`, explicit H.264 parser/decoder, and clock-synchronised KLV printing on the receiver side.

---

## 4. UDP Streaming — Python

Receiver first:

```bash
python3 examples/udp-pipelines/python/udp_receiver_93tags.py \
  --host 0.0.0.0 --port 5000
```

Then sender:

```bash
python3 examples/udp-pipelines/python/udp_sender_93tags.py \
  --host 127.0.0.1 --port 5000 --count 50
```

The UDP examples are transport-equivalent to the SRT ones, but use `udpsink`/`udpsrc` instead of `srtsink`/`srtsrc`. The receivers use the same clock-synchronised print path.

---

## 5. UDP Streaming — C++

Receiver first:

```bash
./build/gstklv_udp_receiver --host 0.0.0.0 --port 5000
```

Then sender:

```bash
./build/gstklv_udp_sender --host 127.0.0.1 --port 5000 --count 50
```

The new C++ UDP examples follow the same JSON generation, PMT rewrite, KLV decoding flow, and presentation-timed receiver output as the SRT C++ examples.

---

## 6. File-Based TS Recording and Playback — Python

Record a `.ts` file with KLV:

```bash
python3 examples/ts/python/klv_recorder.py \
  --output examples/ts/recordings/capture.ts --count 50
```

Read it back:

```bash
python3 examples/ts/python/klv_video_reader.py \
  examples/ts/recordings/capture.ts --print-all
```

The file reader accepts both `stream_type 0x15` captures and the current repo behavior of `0x06 + KLVA`, and paces KLV output against playback instead of dumping metadata immediately.

---

## 7. File-Based TS Recording and Playback — C++

```bash
cmake --build build --target gstklv_ts_recorder gstklv_ts_reader
```

Record:

```bash
./build/gstklv_ts_recorder \
  --output examples/ts/recordings/capture.ts --count 50
```

Read:

```bash
./build/gstklv_ts_reader \
  --input examples/ts/recordings/capture.ts --print-all
```

The C++ TS reader now scans the TS file PMT/PES directly for KLV, restores the UL prefix when needed, and paces metadata from PES PTS by default. Use `--no-pace` only when you want a fast metadata dump.

On some X11 sinks the C++ reader may keep the window open after the last frame has been shown. For scripted runs, wrap it with `timeout`; for interactive inspection, stop it with `Ctrl+C` after the final frame.

---

## Common Parameters

| Parameter | Applies to | Description |
|---|---|---|
| `--host` | SRT/UDP examples | Network endpoint host or bind address |
| `--port` | SRT/UDP examples | Transport port number |
| `--count` | Sender examples | Frames to send (0 = infinite) |
| `--output` | Receiver/reader examples | Output file path |
| `--print-all` | Receiver/reader examples | Print all tags every frame |
| `--video-sink` | C++ readers/receivers | Force a specific video sink |

Run `--help` on any script or executable for the full option list.

---

## PMT Verification

```bash
python3 tools/capture_ts_from_srt.py \
  --host 127.0.0.1 --port 5000 \
  --output examples/ts/recordings/capture.ts --duration 5

python3 tools/verify_ts_klv.py examples/ts/recordings/capture.ts --list-all
```

Expected with the current repo:

- KLV PID uses `stream_type 0x06`
- `registration_descriptor: KLVA`
- `metadata_descriptor: present`

`verify_ts_klv.py` also accepts legacy `0x15` captures.
