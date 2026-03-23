#!/usr/bin/env python3
"""
@file examples/ts/python/klv_video_reader.py
@brief Python MPEG-TS reader example for MISB ST 0601.8 metadata.
@ingroup gstklv_examples_python

This example reads an MPEG-TS recording, plays the H.264 video branch, and
prints decoded MISB ST 0601.8 metadata recovered from the KLV branch.

Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
"""

import argparse
import base64
import configparser
import json
import os
import re
import threading
import time
from pathlib import Path

import gi

gi.require_version("Gst", "1.0")
from gi.repository import Gst, GLib


REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_TAGS_INI = REPO_ROOT / "data" / "stanag4609_tags.ini"
_meson_dir = REPO_ROOT / "build" / "src"
_cmake_dir = REPO_ROOT / "build"
DEFAULT_PLUGIN_DIR = _meson_dir if (_meson_dir / "gstklvplugin.so").exists() else _cmake_dir


def load_tag_defs(ini_path):
    parser = configparser.ConfigParser(interpolation=None)
    parser.optionxform = str
    raw = Path(ini_path).read_text(encoding="utf-8")
    sanitized = []
    for line in raw.splitlines():
        if line.lstrip().startswith(";"):
            sanitized.append("#" + line.lstrip()[1:])
        else:
            sanitized.append(line)
    parser.read_string("\n".join(sanitized))

    tags = {}
    for section in parser.sections():
        if not section.startswith("tag_"):
            continue
        tag_id = parser.getint(section, "id", fallback=None)
        if tag_id is None:
            continue
        tags[tag_id] = {
            "name": parser.get(section, "name", fallback=f"Tag {tag_id}"),
            "type": parser.get(section, "type", fallback=""),
            "units": parser.get(section, "units", fallback=""),
        }
    return tags


def iter_ts_packets(data, packet_size=188):
    for i in range(0, len(data) - packet_size + 1, packet_size):
        pkt = data[i : i + packet_size]
        if pkt[0] != 0x47:
            continue
        yield pkt


def parse_pat(section):
    if not section or section[0] != 0x00:
        return None
    section_length = ((section[1] & 0x0F) << 8) | section[2]
    end = 3 + section_length - 4
    pos = 8
    while pos + 4 <= end:
        program_number = (section[pos] << 8) | section[pos + 1]
        pid = ((section[pos + 2] & 0x1F) << 8) | section[pos + 3]
        if program_number != 0:
            return pid
        pos += 4
    return None


def parse_descriptors(data):
    descriptors = []
    pos = 0
    while pos + 2 <= len(data):
        tag = data[pos]
        length = data[pos + 1]
        pos += 2
        if pos + length > len(data):
            break
        descriptors.append((tag, data[pos:pos + length]))
        pos += length
    return descriptors


def parse_pmt(section):
    if not section or section[0] != 0x02:
        return []
    section_length = ((section[1] & 0x0F) << 8) | section[2]
    end = 3 + section_length - 4
    if end > len(section):
        end = len(section)
    program_info_length = ((section[10] & 0x0F) << 8) | section[11]
    pos = 12 + program_info_length
    streams = []
    while pos + 5 <= end:
        stream_type = section[pos]
        pid = ((section[pos + 1] & 0x1F) << 8) | section[pos + 2]
        es_info_length = ((section[pos + 3] & 0x0F) << 8) | section[pos + 4]
        desc_start = pos + 5
        desc_end = min(desc_start + es_info_length, end)
        descriptors = parse_descriptors(section[desc_start:desc_end])
        streams.append((pid, stream_type, descriptors))
        pos += 5 + es_info_length
    return streams


def extract_sections(data):
    sections = {}
    buffers = {}

    for pkt in iter_ts_packets(data):
        payload_unit_start = (pkt[1] & 0x40) != 0
        pid = ((pkt[1] & 0x1F) << 8) | pkt[2]
        afc = (pkt[3] >> 4) & 0x03
        offset = 4

        if afc in (2, 3):
            if offset >= 188:
                continue
            afl = pkt[offset]
            offset += 1 + afl

        if afc not in (1, 3) or offset >= 188:
            continue

        payload = pkt[offset:]
        if not payload:
            continue

        if payload_unit_start:
            pointer = payload[0]
            payload = payload[1 + pointer :]
            buffers[pid] = bytearray()

        buf = buffers.setdefault(pid, bytearray())
        buf.extend(payload)

        if len(buf) >= 3:
            section_length = ((buf[1] & 0x0F) << 8) | buf[2]
            total_len = 3 + section_length
            if len(buf) >= total_len:
                sections.setdefault(pid, []).append(bytes(buf[:total_len]))
                buffers[pid] = buf[total_len:]

    return sections


def has_klva_descriptor(descriptors):
    for tag, payload in descriptors:
        if tag == 0x05 and len(payload) >= 4:
            if payload[:4] == b"KLVA":
                return True
    return False


def find_klv_pid(ts_path):
    data = Path(ts_path).read_bytes()
    sections = extract_sections(data)
    pmt_pid = None
    if 0 in sections:
        for sec in sections[0]:
            pmt_pid = parse_pat(sec)
            if pmt_pid is not None:
                break
    if pmt_pid is None:
        return None
    pmt_sections = sections.get(pmt_pid, [])
    fallback_pid = None
    for sec in pmt_sections:
        for pid, stream_type, descriptors in parse_pmt(sec):
            if stream_type == 0x15:
                return pid
            if stream_type == 0x06:
                if has_klva_descriptor(descriptors):
                    return pid
                if fallback_pid is None:
                    fallback_pid = pid
    if fallback_pid is not None:
        return fallback_pid
    return None


def parse_pts(pts_bytes):
    if len(pts_bytes) != 5:
        return None
    pts = ((pts_bytes[0] >> 1) & 0x07) << 30
    pts |= (((pts_bytes[1] << 8) | pts_bytes[2]) >> 1) << 15
    pts |= (((pts_bytes[3] << 8) | pts_bytes[4]) >> 1)
    return pts


def iter_klv_pes_payloads(ts_path, pid):
    data = Path(ts_path).read_bytes()
    current = bytearray()
    for pkt in iter_ts_packets(data):
        payload_unit_start = (pkt[1] & 0x40) != 0
        pkt_pid = ((pkt[1] & 0x1F) << 8) | pkt[2]
        if pkt_pid != pid:
            continue
        afc = (pkt[3] >> 4) & 0x03
        offset = 4
        if afc in (2, 3):
            if offset >= 188:
                continue
            afl = pkt[offset]
            offset += 1 + afl
        if afc not in (1, 3) or offset >= 188:
            continue
        payload = pkt[offset:]
        if payload_unit_start:
            if current:
                yield bytes(current)
                current.clear()
        current.extend(payload)
    if current:
        yield bytes(current)


def extract_pes_payload(pes_bytes):
    if len(pes_bytes) < 9:
        return None, None
    if not (pes_bytes[0] == 0x00 and pes_bytes[1] == 0x00 and pes_bytes[2] == 0x01):
        return None, None
    flags = pes_bytes[7]
    header_len = pes_bytes[8]
    pts = None
    if flags & 0x80 and len(pes_bytes) >= 14:
        pts = parse_pts(pes_bytes[9:14])
    payload_start = 9 + header_len
    if payload_start > len(pes_bytes):
        return None, None
    return pts, pes_bytes[payload_start:]


def pts90k_to_ns(value):
    return int(value * (1_000_000_000 / 90000))


def wait_for_pipeline_clock(pipeline, timeout_sec=5.0):
    deadline = time.monotonic() + timeout_sec
    last_clock = None
    last_base_time = Gst.CLOCK_TIME_NONE
    while time.monotonic() < deadline:
        clock = pipeline.get_clock()
        base_time = pipeline.get_base_time()
        if clock is not None and base_time not in (0, Gst.CLOCK_TIME_NONE):
            return clock, base_time
        last_clock = clock
        last_base_time = base_time
        time.sleep(0.01)
    if last_clock is not None and last_base_time not in (0, Gst.CLOCK_TIME_NONE):
        return last_clock, last_base_time
    return None, Gst.CLOCK_TIME_NONE


def build_pipeline(ts_path, headless, video_sink, video_decoder, video_pipeline):
    if video_pipeline == "playbin":
        pipeline = Gst.ElementFactory.make("playbin", "playbin")
        if not pipeline:
            print("ERROR Missing GStreamer element: playbin")
            return None, None, None
        pipeline.set_property("uri", Path(ts_path).resolve().as_uri())

        videosink = Gst.ElementFactory.make("fakesink" if headless else video_sink, "videosink")
        if videosink:
            if headless:
                videosink.set_property("sync", False)
            else:
                if videosink.find_property("async") is not None:
                    videosink.set_property("async", False)
                videosink.set_property("sync", True)
            pipeline.set_property("video-sink", videosink)
        audiosink = Gst.ElementFactory.make("fakesink", "audiosink")
        if audiosink:
            pipeline.set_property("audio-sink", audiosink)
        textsink = Gst.ElementFactory.make("fakesink", "textsink")
        if textsink:
            pipeline.set_property("text-sink", textsink)

        print("  Video pipeline: playbin")
        return pipeline, videosink, "playbin"

    pipeline = Gst.Pipeline.new("klv-video-reader")

    filesrc = Gst.ElementFactory.make("filesrc", "src")
    demux = Gst.ElementFactory.make("tsdemux", "demux")
    queue_video = Gst.ElementFactory.make("queue", "queue_video")
    h264parse = Gst.ElementFactory.make("h264parse", "h264parse")
    decoder = None
    decoder_name = None
    if video_decoder and video_decoder != "auto":
        decoder = Gst.ElementFactory.make(video_decoder, "decoder")
        decoder_name = video_decoder
    else:
        decoder = Gst.ElementFactory.make("avdec_h264", "decoder")
        decoder_name = "avdec_h264" if decoder else None
        if decoder is None:
            decoder = Gst.ElementFactory.make("openh264dec", "decoder")
            decoder_name = "openh264dec" if decoder else None
        if decoder is None:
            decoder = Gst.ElementFactory.make("decodebin", "decoder")
            decoder_name = "decodebin" if decoder else None
    videoconvert = Gst.ElementFactory.make("videoconvert", "convert")
    videosink = Gst.ElementFactory.make("fakesink" if headless else video_sink, "videosink")
    queue_klv = Gst.ElementFactory.make("queue", "queue_klv")
    klvraw = Gst.ElementFactory.make("fakesink", "klvraw")

    elements = [
        filesrc, demux, queue_video, h264parse, decoder,
        videoconvert, videosink, queue_klv, klvraw
    ]
    if not all(elements):
        missing = [name for name, elem in [
            ("filesrc", filesrc),
            ("tsdemux", demux),
            ("queue", queue_video),
            ("h264parse", h264parse),
            (decoder_name or "decoder", decoder),
            ("videoconvert", videoconvert),
            ("videosink", videosink),
            ("queue", queue_klv),
            ("fakesink", klvraw),
        ] if elem is None]
        print(f"ERROR Missing GStreamer elements: {', '.join(missing)}")
        return None, None, None

    filesrc.set_property("location", str(ts_path))
    if headless:
        videosink.set_property("sync", False)
    else:
        if videosink.find_property("async") is not None:
            videosink.set_property("async", False)
        videosink.set_property("sync", True)

    pipeline.add(filesrc)
    pipeline.add(demux)
    pipeline.add(queue_video)
    pipeline.add(h264parse)
    pipeline.add(decoder)
    pipeline.add(videoconvert)
    pipeline.add(videosink)
    pipeline.add(queue_klv)
    pipeline.add(klvraw)

    filesrc.link(demux)
    queue_video.link(h264parse)
    if decoder_name == "decodebin":
        h264parse.link(decoder)
    else:
        h264parse.link(decoder)
        decoder.link(videoconvert)
        videoconvert.link(videosink)
    queue_klv.link(klvraw)

    if decoder_name == "decodebin":
        def on_decoder_pad(decoder, pad):
            caps = pad.get_current_caps() or pad.query_caps(None)
            cap_name = "unknown"
            if caps and caps.get_size() > 0:
                cap_name = caps.get_structure(0).get_name()
            sink_pad = videoconvert.get_static_pad("sink")
            if not sink_pad.is_linked():
                ret = pad.link(sink_pad)
                if ret == Gst.PadLinkReturn.OK:
                    print(f"  Linked decoder pad: {cap_name}")
                    videoconvert.link(videosink)
                else:
                    print(f"  ERROR Failed to link decoder pad ({cap_name}): {ret}")

        decoder.connect("pad-added", on_decoder_pad)

    def on_demux_pad(demux, pad):
        caps = pad.get_current_caps() or pad.query_caps(None)
        if not caps:
            return
        structure = caps.get_structure(0)
        name = structure.get_name()

        if name.startswith("video/"):
            sink_pad = queue_video.get_static_pad("sink")
            if not sink_pad.is_linked():
                ret = pad.link(sink_pad)
                if ret == Gst.PadLinkReturn.OK:
                    print(f"  Linked video pad: {name}")
                else:
                    print(f"  ERROR Failed to link video pad: {ret}")
        elif name.startswith("meta/"):
            sink_pad = queue_klv.get_static_pad("sink")
            if not sink_pad.is_linked():
                ret = pad.link(sink_pad)
                if ret == Gst.PadLinkReturn.OK:
                    print(f"  Linked KLV pad: {name}")
                else:
                    print(f"  ERROR Failed to link KLV pad: {ret}")

    demux.connect("pad-added", on_demux_pad)

    klvraw.set_property("sync", False)

    return pipeline, videosink, decoder_name


def run_reader(
    ts_path,
    output="-",
    headless=False,
    print_all=True,
    video_sink="autovideosink",
    video_decoder="auto",
    video_pipeline="playbin",
):
    plugin_path = DEFAULT_PLUGIN_DIR
    ini_path = DEFAULT_TAGS_INI

    if not plugin_path.exists():
        print(f"ERROR Plugin not found: {plugin_path}")
        return 1
    if not ini_path.exists():
        print(f"ERROR Tags INI not found: {ini_path}")
        return 1
    if not Path(ts_path).exists():
        print(f"ERROR TS file not found: {ts_path}")
        return 1

    os.environ["GST_PLUGIN_PATH"] = str(plugin_path)
    os.environ["KLV_TAGS_INI"] = str(ini_path)

    Gst.init([])
    tag_defs = load_tag_defs(ini_path)

    print("\n" + "=" * 100)
    print("KLV VIDEO READER - MISB ST 0601.8 KLV TAGS")
    print("=" * 100 + "\n")
    print("> Configuration:")
    print(f"  Input: {ts_path}")
    print(f"  Plugin: {plugin_path}/gstklvplugin.so")
    print(f"  Tags: {ini_path}")
    print(f"  Output: {output}")
    if headless:
        print("  Video: headless (fakesink)")
    else:
        print(f"  Video sink: {video_sink}")
        print(f"  Video decoder: {video_decoder}")
        print(f"  Video pipeline: {video_pipeline}")
    print("\nNote: klvmetadec emits all tags as JSON; 2/5/13/14/15 are scaled to physical units.\n")

    pipeline, videosink, decoder_name = build_pipeline(
        ts_path, headless, video_sink, video_decoder, video_pipeline
    )
    if not pipeline:
        return 1

    output_fp = None
    if output and output != "-":
        output_fp = open(output, "w", encoding="utf-8")

    klv_ul = bytes([0x06, 0x0E, 0x2B, 0x34, 0x02, 0x0B, 0x01, 0x01,
                    0x0E, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00])
    klv_ul_suffix = klv_ul[5:]
    json_count = [0]

    def bytes_preview(value):
        if not isinstance(value, str):
            return "bytes(?)"
        try:
            if value.startswith("hex:"):
                hex_str = re.sub(r"[^0-9a-fA-F]", "", value[4:])
                data = bytes.fromhex(hex_str)
            elif value.startswith("base64:"):
                data = base64.b64decode(value[7:])
            else:
                return "bytes(?)"
        except Exception:
            return "bytes(?)"
        preview = data[:16].hex()
        return f"bytes[{len(data)}]: {preview}{'...' if len(data) > 16 else ''}"

    def build_klv_buffer(raw_bytes, pts, dts):
        data = raw_bytes
        if data.startswith(klv_ul):
            fixed = data
        elif data.startswith(klv_ul_suffix):
            fixed = klv_ul[:5] + data
        else:
            fixed = data

        outbuf = Gst.Buffer.new_allocate(None, len(fixed), None)
        outbuf.fill(0, fixed)
        if pts != Gst.CLOCK_TIME_NONE:
            outbuf.pts = pts
        if dts != Gst.CLOCK_TIME_NONE:
            outbuf.dts = dts
        return outbuf

    def on_json_sample(sink):
        sample = sink.emit("pull-sample")
        if not sample:
            return Gst.FlowReturn.OK
        buffer = sample.get_buffer()
        success, mapinfo = buffer.map(Gst.MapFlags.READ)
        if not success:
            return Gst.FlowReturn.OK
        try:
            text = mapinfo.data.decode("utf-8", errors="replace").strip()
            if output_fp:
                output_fp.write(text + "\n")

            data = json.loads(text)
            json_count[0] += 1
            lines = [
                "",
                "=" * 80,
                f"[FRAME {json_count[0]:04d}] Decoded tags: {len(data)}",
                "-" * 80,
            ]

            if print_all:
                for key in sorted(data.keys(), key=lambda k: int(k)):
                    tag_id = int(key)
                    meta = tag_defs.get(tag_id, {"name": f"Tag {tag_id}", "units": ""})
                    unit = f" {meta['units']}" if meta.get("units") else ""
                    if meta.get("type", "").lower() == "bytes":
                        lines.append(f"  {tag_id:02d} {meta['name']}: {bytes_preview(data[key])}")
                    else:
                        lines.append(f"  {tag_id:02d} {meta['name']}: {data[key]}{unit}")
            else:
                if "13" in data and "14" in data and "15" in data:
                    lines.append(f"  13 Sensor Latitude:  {float(data['13']):.6f} Degrees")
                    lines.append(f"  14 Sensor Longitude: {float(data['14']):.6f} Degrees")
                    lines.append(f"  15 Sensor True Altitude: {float(data['15']):.1f} Meters")
            lines.append("=" * 80)
            print("\n".join(lines), flush=True)
        except Exception as exc:
            print(f"ERROR Failed to parse JSON: {exc}")
        finally:
            buffer.unmap(mapinfo)
        return Gst.FlowReturn.OK


    klv_pipeline = Gst.Pipeline.new("klv-decode-pipeline")
    appsrc = Gst.ElementFactory.make("appsrc", "klvsrc")
    klvmetadec = Gst.ElementFactory.make("klvmetadec", "klvmetadec")
    jsonsink = Gst.ElementFactory.make("appsink", "jsonsink")
    if not all([appsrc, klvmetadec, jsonsink]):
        print("ERROR Failed to create KLV decode pipeline elements")
        pipeline.set_state(Gst.State.NULL)
        return 1

    appsrc.set_property("caps", Gst.Caps.from_string("meta/x-klv"))
    appsrc.set_property("is-live", False)
    appsrc.set_property("format", Gst.Format.TIME)
    jsonsink.set_property("emit-signals", True)
    jsonsink.set_property("sync", False)

    klv_pipeline.add(appsrc)
    klv_pipeline.add(klvmetadec)
    klv_pipeline.add(jsonsink)
    appsrc.link(klvmetadec)
    klvmetadec.link(jsonsink)

    jsonsink.connect("new-sample", on_json_sample)

    klv_pid = find_klv_pid(ts_path)
    if klv_pid is None:
        print("WARNING No KLV PID found in PMT; KLV output will be empty")

    klv_bus = klv_pipeline.get_bus()
    klv_bus.add_signal_watch()

    loop = GLib.MainLoop()
    eos_state = {
        "video": False,
        "klv": False,
        "error": False,
    }

    def maybe_quit():
        if eos_state["error"]:
            loop.quit()
            return
        if eos_state["video"] and eos_state["klv"]:
            loop.quit()

    def on_klv_message(bus, message):
        msg_type = message.type
        if msg_type == Gst.MessageType.ERROR:
            err, debug = message.parse_error()
            print(f"ERROR KLV pipeline error: {err.message}")
            if debug:
                print(f"  Debug: {debug}")
            eos_state["error"] = True
            maybe_quit()
        elif msg_type == Gst.MessageType.EOS:
            eos_state["klv"] = True
            maybe_quit()
        return True

    klv_bus.connect("message", on_klv_message)

    ret = pipeline.set_state(Gst.State.PLAYING)
    if ret == Gst.StateChangeReturn.FAILURE:
        print("ERROR Failed to start pipeline")
        pipeline.set_state(Gst.State.NULL)
        klv_pipeline.set_state(Gst.State.NULL)
        return 1
    klv_pipeline.set_state(Gst.State.PLAYING)

    video_frames = [0]
    if not headless and videosink is not None:
        sink_pad = videosink.get_static_pad("sink")
        if sink_pad:
            def on_video_buffer(pad, info):
                video_frames[0] += 1
                return Gst.PadProbeReturn.OK
            sink_pad.add_probe(Gst.PadProbeType.BUFFER, on_video_buffer)

    def warn_if_no_video_frames():
        if not headless and video_frames[0] == 0:
            print("WARNING No video frames received yet.")
            print("  If you are on Wayland, try: --video-sink waylandsink or glimagesink")
            print("  If this is an older system, try: --video-sink ximagesink or xvimagesink")
            print("  You can also verify playback with: gst-play-1.0 capture.ts")
        return False

    GLib.timeout_add_seconds(2, warn_if_no_video_frames)

    def warn_if_no_video_output():
        if headless:
            return
        if videosink is None:
            print("WARNING Video sink is not available")
            return
        factory = videosink.get_factory()
        if factory and factory.get_name() == "fakesink":
            print("WARNING Video sink is fakesink (no window). Use --video-sink to pick a real sink.")
            return
        if factory and factory.get_name() == "autovideosink":
            iterator = videosink.iterate_elements()
            while True:
                res, elem = iterator.next()
                if res != Gst.IteratorResult.OK:
                    break
                elem_factory = elem.get_factory()
                if elem_factory and elem_factory.get_name() == "fakesink":
                    print("WARNING autovideosink selected fakesink (no window).")
                    print("  Try: --video-sink glimagesink | xvimagesink | ximagesink")
                    break

    warn_if_no_video_output()

    def feed_klv():
        if klv_pid is None:
            return
        first_pts = None
        clock, base_time = wait_for_pipeline_clock(pipeline)
        fallback_start = time.monotonic()
        if clock is None:
            print("WARNING Video pipeline clock unavailable; falling back to local pacing.")
        for pes in iter_klv_pes_payloads(ts_path, klv_pid):
            pts, payload = extract_pes_payload(pes)
            if not payload:
                continue
            pts_ns = Gst.CLOCK_TIME_NONE
            if pts is not None:
                if first_pts is None:
                    first_pts = pts
                rel_ns = pts90k_to_ns(pts - first_pts)
                pts_ns = rel_ns
                if clock is not None and base_time not in (0, Gst.CLOCK_TIME_NONE):
                    target = int(base_time + rel_ns)
                    while True:
                        now = int(clock.get_time())
                        remaining = target - now
                        if remaining <= 0:
                            break
                        time.sleep(min(remaining / 1_000_000_000.0, 0.05))
                else:
                    target = fallback_start + (rel_ns / 1_000_000_000.0)
                    delay = target - time.monotonic()
                    if delay > 0:
                        time.sleep(delay)
            outbuf = build_klv_buffer(payload, pts_ns, pts_ns)
            appsrc.emit("push-buffer", outbuf)
        appsrc.emit("end-of-stream")

    feeder = threading.Thread(target=feed_klv, daemon=True)
    feeder.start()

    bus = pipeline.get_bus()
    bus.add_signal_watch()

    def on_message(bus, message):
        msg_type = message.type
        if msg_type == Gst.MessageType.ERROR:
            err, debug = message.parse_error()
            print(f"\nERROR Pipeline error: {err.message}")
            if debug:
                print(f"  Debug: {debug}")
            eos_state["error"] = True
            maybe_quit()
        elif msg_type == Gst.MessageType.EOS:
            eos_state["video"] = True
            maybe_quit()

    bus.connect("message", on_message)

    try:
        loop.run()
    except KeyboardInterrupt:
        pass
    finally:
        pipeline.set_state(Gst.State.NULL)
        appsrc.emit("end-of-stream")
        klv_pipeline.set_state(Gst.State.NULL)
        if output_fp:
            output_fp.close()

    print(f"\nOK Playback complete ({json_count[0]} frames)")
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="Read an MPEG-TS file and print MISB ST 0601.8 KLV tags"
    )
    parser.add_argument("input", help="Input .ts file")
    parser.add_argument("--output", default="-", help="Write JSON output to file")
    parser.add_argument("--headless", action="store_true", help="Disable video output")
    parser.add_argument("--video-sink", default="autovideosink", help="Video sink element")
    parser.add_argument(
        "--video-decoder",
        default="auto",
        help="Video decoder element (auto uses avdec_h264/openh264dec/decodebin)",
    )
    parser.add_argument(
        "--video-pipeline",
        default="playbin",
        choices=["playbin", "manual"],
        help="Video pipeline backend (default: playbin)",
    )
    parser.add_argument("--print-all", action="store_true", help="Print all tags each frame (default)")
    parser.add_argument("--print-summary", action="store_true", help="Only print tags 13/14/15")

    args = parser.parse_args()
    print_all = True
    if args.print_summary:
        print_all = False
    elif args.print_all:
        print_all = True

    return run_reader(
        ts_path=args.input,
        output=args.output,
        headless=args.headless,
        print_all=print_all,
        video_sink=args.video_sink,
        video_decoder=args.video_decoder,
        video_pipeline=args.video_pipeline,
    )


if __name__ == "__main__":
    raise SystemExit(main())
