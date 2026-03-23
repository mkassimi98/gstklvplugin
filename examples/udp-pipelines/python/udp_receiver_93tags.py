#!/usr/bin/env python3
"""
@file examples/udp-pipelines/python/udp_receiver_93tags.py
@brief Python UDP receiver example for MISB ST 0601.8 KLV metadata.
@ingroup gstklv_examples_python

This example receives an MPEG-TS stream over UDP, displays the decoded video,
and prints MISB ST 0601.8 metadata after decoding the KLV branch.

Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
"""

import argparse
import json
import os
import sys
from pathlib import Path
import configparser
import base64
import re

import gi

gi.require_version('Gst', '1.0')
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


def build_pipeline(host, port, headless):
    pipeline = Gst.Pipeline.new("udp-receiver")

    udpsrc = Gst.ElementFactory.make("udpsrc", "src")
    demux = Gst.ElementFactory.make("tsdemux", "demux")
    queue_video = Gst.ElementFactory.make("queue", "queue_video")
    h264parse = None
    decoder = None
    videoconvert = None
    if not headless:
        h264parse = Gst.ElementFactory.make("h264parse", "h264parse")
        decoder = Gst.ElementFactory.make("avdec_h264", "decoder")
        videoconvert = Gst.ElementFactory.make("videoconvert", "convert")
    if headless:
        videosink = Gst.ElementFactory.make("fakesink", "videosink")
    else:
        # Prefer xvimagesink (has async property) → ximagesink → autovideosink
        videosink = (
            Gst.ElementFactory.make("xvimagesink", "videosink")
            or Gst.ElementFactory.make("ximagesink", "videosink")
            or Gst.ElementFactory.make("autovideosink", "videosink")
        )
    queue_klv = Gst.ElementFactory.make("queue", "queue_klv")
    klvmetadec = Gst.ElementFactory.make("klvmetadec", "klvmetadec")
    klvsink = Gst.ElementFactory.make("fakesink", "klvsink")

    elements = [udpsrc, demux, queue_video, videosink, queue_klv, klvmetadec, klvsink]
    if not headless:
        elements.extend([h264parse, decoder, videoconvert])
    if not all(elements):
        missing = [name for name, elem in [
            ("udpsrc", udpsrc),
            ("tsdemux", demux),
            ("queue", queue_video),
            ("videosink", videosink),
            ("queue", queue_klv),
            ("klvmetadec", klvmetadec),
            ("fakesink", klvsink),
            ("h264parse", h264parse) if not headless else ("h264parse", True),
            ("avdec_h264", decoder) if not headless else ("avdec_h264", True),
            ("videoconvert", videoconvert) if not headless else ("videoconvert", True),
        ] if elem is None]
        print(f"ERROR Missing GStreamer elements: {', '.join(missing)}")
        return None, None, None

    udpsrc.set_property("address", host)
    udpsrc.set_property("port", port)
    udpsrc.set_property(
        "caps",
        Gst.Caps.from_string("video/mpegts, systemstream=(boolean)true, packetsize=(int)188"),
    )
    # Keep video and KLV on the same presentation timeline. Dropping only video
    # buffers makes terminal prints drift away from the displayed frame.
    if not headless and videosink.find_property("async") is not None:
        videosink.set_property("async", False)
    videosink.set_property("sync", True)

    pipeline.add(udpsrc)
    pipeline.add(demux)
    pipeline.add(queue_video)
    pipeline.add(videosink)
    pipeline.add(queue_klv)
    pipeline.add(klvmetadec)
    pipeline.add(klvsink)
    if not headless:
        pipeline.add(h264parse)
        pipeline.add(decoder)
        pipeline.add(videoconvert)

    udpsrc.link(demux)
    if headless:
        queue_video.link(videosink)
    else:
        h264parse.set_property("config-interval", -1)
        queue_video.link(h264parse)
        h264parse.link(decoder)
        decoder.link(videoconvert)
        videoconvert.link(videosink)
    queue_klv.link(klvmetadec)
    klvmetadec.link(klvsink)

    state = {
        "video_demux_pad": None,
    }

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
                    state["video_demux_pad"] = pad
                    print(f"  Linked video pad: {name}")
                else:
                    print(f"  WARNING Failed to link video pad ({name}): {ret.value_nick}")
        elif name.startswith("meta/"):
            sink_pad = queue_klv.get_static_pad("sink")
            if not sink_pad.is_linked():
                ret = pad.link(sink_pad)
                if ret == Gst.PadLinkReturn.OK:
                    print(f"  Linked KLV pad: {name}")
                else:
                    print(f"  WARNING Failed to link KLV pad ({name}): {ret.value_nick}")

    demux.connect("pad-added", on_demux_pad)

    return pipeline, klvsink, state


def run_receiver(host="127.0.0.1", port=5000, output="-", headless=False, print_all=True):
    plugin_path = DEFAULT_PLUGIN_DIR
    ini_path = DEFAULT_TAGS_INI

    if not plugin_path.exists():
        print(f"ERROR Plugin not found: {plugin_path}")
        return 1
    if not ini_path.exists():
        print(f"ERROR Tags INI not found: {ini_path}")
        return 1

    os.environ["GST_PLUGIN_PATH"] = str(plugin_path)
    os.environ["KLV_TAGS_INI"] = str(ini_path)

    Gst.init([])
    tag_defs = load_tag_defs(ini_path)

    print("\n" + "=" * 100)
    print("UDP RECEIVER - MISB ST 0601.8 KLV TAGS")
    print("=" * 100 + "\n")
    print("> Configuration:")
    print(f"  Host: {host}")
    print(f"  Port: {port}")
    print(f"  Plugin: {plugin_path}/gstklvplugin.so")
    print(f"  Tags: {ini_path}")
    print(f"  Output: {output}")
    if headless:
        print("  Video: headless (fakesink)")
    print("\nNote: klvmetadec now emits all tags as JSON; 2/5/13/14/15 are scaled to physical units.\n")

    pipeline, klvsink, pipeline_state = build_pipeline(host, port, headless)
    if not pipeline:
        return 1

    output_fp = None
    if output and output != "-":
        output_fp = open(output, "w", encoding="utf-8")

    klvsink.set_property("signal-handoffs", True)
    klvsink.set_property("sync", True)
    frame_count = [0]
    video_disabled = {"value": False}

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

    def on_handoff(sink, buffer, pad):
        frame_count[0] += 1
        success, mapinfo = buffer.map(Gst.MapFlags.READ)
        if not success:
            return
        try:
            text = mapinfo.data.decode("utf-8", errors="replace").strip()
            if not text:
                print(f"[FRAME {frame_count[0]:04d}] KLV buffer empty (size={mapinfo.size})")
                return
            data = json.loads(text)

            if isinstance(data, dict) and "tags" in data and isinstance(data["tags"], dict):
                tags = data["tags"]
            elif isinstance(data, dict):
                tags = data
            else:
                tags = {}

            tag_count = len(tags)
            lines = [
                "",
                "=" * 80,
                f"[FRAME {frame_count[0]:04d}] Decoded tags: {tag_count}",
                "-" * 80,
            ]

            if print_all:
                for key in sorted(tags.keys(), key=lambda k: int(k)):
                    tag_id = int(key)
                    meta = tag_defs.get(tag_id, {"name": f"Tag {tag_id}", "units": ""})
                    unit = f" {meta['units']}" if meta.get("units") else ""
                    if meta.get("type", "").lower() == "bytes":
                        lines.append(f"  {tag_id:02d} {meta['name']}: {bytes_preview(tags[key])}")
                    else:
                        lines.append(f"  {tag_id:02d} {meta['name']}: {tags[key]}{unit}")
            else:
                sample = []
                for key in ["2", "5", "13", "14", "15"]:
                    if key in tags:
                        tag_id = int(key)
                        meta = tag_defs.get(tag_id, {"name": f"Tag {tag_id}", "units": ""})
                        unit = f" {meta['units']}" if meta.get("units") else ""
                        sample.append(f"{tag_id:02d} {meta['name']}: {tags[key]}{unit}")
                if sample:
                    for line in sample:
                        lines.append(f"  {line}")

            # Verify missing tags (exclude byte/local-set tags unless present)
            expected_ids = [
                tid for tid, meta in tag_defs.items()
                if meta.get("type") and meta.get("type", "").lower() != "bytes"
            ]
            missing = [str(tid) for tid in sorted(expected_ids) if str(tid) not in tags]
            if missing:
                lines.append(f"  Missing tags: {', '.join(missing)}")
            lines.extend(["=" * 80, ""])
            print("\n".join(lines), flush=True)

            if output_fp:
                output_fp.write(json.dumps(data) + "\n")
                output_fp.flush()
        except Exception as e:
            print(f"Error parsing KLV JSON: {e} (size={mapinfo.size})")
        finally:
            buffer.unmap(mapinfo)

    klvsink.connect("handoff", on_handoff)

    bus = pipeline.get_bus()
    bus.add_signal_watch()
    loop = GLib.MainLoop()

    def disable_video_branch(reason):
        if video_disabled["value"] or headless:
            return
        pad = pipeline_state.get("video_demux_pad") if pipeline_state else None
        if pad is None:
            return

        def drop_video(_pad, _info):
            return Gst.PadProbeReturn.DROP

        pad.add_probe(Gst.PadProbeType.BUFFER, drop_video)
        video_disabled["value"] = True
        # Do NOT set elements to NULL — that causes GST_FLOW_ERROR to propagate
        # back to srtsrc and crash the whole pipeline. The probe above drops all
        # future video buffers; the video elements will simply go idle.
        print(f"WARNING Video branch disabled: {reason}")
        print("  Continuing with KLV metadata only.")

    def on_message(bus, message):
        msg_type = message.type
        if msg_type == Gst.MessageType.ERROR:
            err, debug = message.parse_error()
            src_name = message.src.get_name() if message.src else ""
            video_err_sources = {
                "h264parse",
                "decoder",
                "convert",
                "videosink",
                "typefind",
                "queue_video",
            }
            if video_disabled["value"] and src_name in video_err_sources:
                return
            if src_name in video_err_sources and not headless:
                disable_video_branch(err.message)
                if debug:
                    print(f"  Video debug: {debug}")
                return
            # After video is disabled, udpsrc may post a cascading "streaming
            # stopped" flow error because in-flight buffers hit the now-idle
            # video branch. Suppress it — the KLV branch is still running.
            if video_disabled["value"] and src_name == "src":
                return

            print(f"\nERROR Pipeline error: {err.message}")
            if debug:
                print(f"  Debug: {debug}")
            loop.quit()
        elif msg_type == Gst.MessageType.EOS:
            print("\nOK End of stream")
            loop.quit()

    bus.connect("message", on_message)

    print(f"> Listening on udp://{host}:{port} ...")
    print("  Press Ctrl+C to stop\n")

    ret = pipeline.set_state(Gst.State.PLAYING)
    if ret == Gst.StateChangeReturn.FAILURE:
        print("ERROR Failed to start pipeline")
        pipeline.set_state(Gst.State.NULL)
        if output_fp:
            output_fp.close()
        return 1

    try:
        loop.run()
    except KeyboardInterrupt:
        print("\nOK Stopped")
    finally:
        pipeline.set_state(Gst.State.NULL)
        if output_fp:
            output_fp.close()

    return 0


def main():
    parser = argparse.ArgumentParser(
        description="UDP Receiver with MISB ST 0601.8 KLV tags"
    )
    parser.add_argument("--host", default="0.0.0.0", help="Listen host (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=5000, help="UDP port (default: 5000)")
    parser.add_argument("--output", default="-", help="Output file ('-' = stdout)")
    parser.add_argument("--headless", action="store_true", help="Disable video output")
    parser.add_argument("--print-all", action="store_true", help="Print all tags each frame (default)")
    parser.add_argument("--print-summary", action="store_true", help="Only print tags 2, 5, 13, 14, 15")

    args = parser.parse_args()

    try:
        return run_receiver(
            host=args.host,
            port=args.port,
            output=args.output,
            headless=args.headless,
            print_all=False if args.print_summary else True,
        )
    except KeyboardInterrupt:
        print("\nERROR Interrupted by user")
        return 1


if __name__ == "__main__":
    sys.exit(main())
