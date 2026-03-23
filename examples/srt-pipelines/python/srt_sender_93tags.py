#!/usr/bin/env python3
"""
@file examples/srt-pipelines/python/srt_sender_93tags.py
@brief Python SRT sender example for MISB ST 0601.8 KLV metadata.
@ingroup gstklv_examples_python

This example generates H.264 video plus per-frame MISB ST 0601.8 metadata
and streams the multiplexed MPEG-TS output over SRT.

Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
"""

import argparse
import json
import math
import os
import random
import sys
import time
from pathlib import Path
import configparser
import base64
import re

import gi

gi.require_version('Gst', '1.0')
from gi.repository import Gst, GLib

FPS = 2
FRAME_INTERVAL = 1.0 / FPS


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

    def parse_range_value(val):
        val = val.strip()
        if val.startswith("(") and val.endswith(")"):
            val = val[1:-1].strip()
        if "2^" in val:
            parts = val.split("2^", 1)[1]
            num = ""
            for ch in parts:
                if ch.isdigit():
                    num += ch
                else:
                    break
            if not num:
                return None
            exp = int(num)
            base = 2 ** exp
            rest = parts[len(num):].strip()
            if rest.startswith("+") or rest.startswith("-"):
                try:
                    base += float(rest)
                except ValueError:
                    return None
            return float(base)
        try:
            return float(val)
        except ValueError:
            return None

    def parse_range(range_str):
        if not range_str:
            return None
        s = range_str.strip()
        if "+/-" in s:
            _, rhs = s.split("+/-", 1)
            v = parse_range_value(rhs)
            if v is None:
                return None
            return (-v, v)
        if ".." in s:
            left, right = s.split("..", 1)
            vmin = parse_range_value(left)
            vmax = parse_range_value(right)
            if vmin is None or vmax is None:
                return None
            return (vmin, vmax)
        return None

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
            "range": parse_range(parser.get(section, "range", fallback="")),
        }
    return tags


class TagValueGenerator:
    """Generate realistic randomized tag values."""

    def __init__(self, seed=None):
        if seed is not None:
            random.seed(seed)

        # Initial values
        self.latitude = 40.7128 + random.uniform(-0.1, 0.1)
        self.longitude = -74.0060 + random.uniform(-0.1, 0.1)
        self.altitude = 500 + random.uniform(-50, 50)
        self.heading = random.uniform(0, 360)
        self.pitch = random.uniform(-30, 30)
        self.roll = random.uniform(-45, 45)
        self.airspeed = random.uniform(100, 300)

        # Velocities
        self.north_vel = random.uniform(-50, 50)
        self.east_vel = random.uniform(-50, 50)
        self.up_vel = random.uniform(-20, 20)

        # Event start time should stay constant for an event
        self.event_start_time_us = int(time.time() * 1_000_000)

    def update(self):
        """Update values for next frame."""
        self.latitude += random.uniform(-0.0001, 0.0001)
        self.longitude += random.uniform(-0.0001, 0.0001)
        self.altitude += random.uniform(-2, 2)
        self.heading = (self.heading + random.uniform(-5, 5)) % 360
        self.pitch = max(-30, min(30, self.pitch + random.uniform(-2, 2)))
        self.roll = max(-45, min(45, self.roll + random.uniform(-2, 2)))
        self.airspeed = max(50, min(400, self.airspeed + random.uniform(-10, 10)))

        self.north_vel += random.uniform(-5, 5)
        self.east_vel += random.uniform(-5, 5)
        self.up_vel += random.uniform(-2, 2)

    def get_tags(self):
        """Return base tags with current values (local sets excluded)."""
        tags = {
            "2": int(time.time() * 1_000_000),  # Unix timestamp (microseconds)
            "3": f"AIRCRAFT-{int(random.random()*1000)}",  # Mission ID
            "4": "N12345",  # Platform Tail Number
            "5": self.heading,  # Platform Heading Angle
            "6": self.pitch,  # Platform Pitch Angle
            "7": self.roll,  # Platform Roll Angle
            "8": self.airspeed,  # Platform True Airspeed
            "9": self.airspeed * 0.95,  # Platform Indicated Airspeed
            "10": "DEMO-SENSOR",  # Platform Designation
            "11": "FLIR-TYPE-A",  # Image Source Sensor
            "12": random.randint(0, 255),  # Image Coordinate System
            "13": self.latitude,  # Sensor Latitude
            "14": self.longitude,  # Sensor Longitude
            "15": self.altitude,  # Sensor True Altitude
            "16": random.uniform(10, 90),  # Sensor Horizontal FOV
            "17": random.uniform(5, 60),  # Sensor Vertical FOV
            "18": random.uniform(0, 360),  # Sensor Relative Azimuth
            "19": random.uniform(-90, 90),  # Sensor Relative Elevation
            "20": random.uniform(-180, 180),  # Sensor Relative Roll
            "21": random.uniform(1000, 50000),  # Slant Range
            "22": random.uniform(10, 500),  # Target Width
            "23": self.latitude + random.uniform(-0.01, 0.01),  # Frame Center Lat
            "24": self.longitude + random.uniform(-0.01, 0.01),  # Frame Center Lon
            "25": self.altitude + random.uniform(-100, 100),  # Frame Center Elevation
            "26": random.uniform(-100, 100),  # Offset Corner Lat 1
            "27": random.uniform(-100, 100),  # Offset Corner Lon 1
            "28": random.uniform(-100, 100),  # Offset Corner Lat 2
            "29": random.uniform(-100, 100),  # Offset Corner Lon 2
            "30": random.uniform(-100, 100),  # Offset Corner Lat 3
            "31": random.uniform(-100, 100),  # Offset Corner Lon 3
            "32": random.uniform(-100, 100),  # Offset Corner Lat 4
            "33": random.uniform(-100, 100),  # Offset Corner Lon 4
            "34": random.randint(0, 1),  # Icing Detected
            "35": random.uniform(0, 360),  # Wind Direction
            "36": random.uniform(0, 50),  # Wind Speed
            "37": random.uniform(900, 1050),  # Static Pressure
            "38": random.uniform(0, 100),  # Density Altitude
            "39": random.uniform(-50, 50),  # Outside Air Temperature
            "40": self.latitude + random.uniform(-0.01, 0.01),  # Target Location Lat
            "41": self.longitude + random.uniform(-0.01, 0.01),  # Target Location Lon
            "42": self.altitude + random.uniform(-50, 50),  # Target Location Elevation
            "43": random.uniform(10, 200),  # Target Track Gate Width
            "44": random.uniform(10, 200),  # Target Track Gate Height
            "45": random.uniform(1, 100),  # Target Error CE90
            "46": random.uniform(1, 100),  # Target Error LE90
            "47": random.randint(0, 1),  # Generic Flag Data 01
            "49": random.uniform(900, 1050),  # Differential Pressure
            "50": random.uniform(-45, 45),  # Platform Angle of Attack
            "51": self.up_vel,  # Platform Vertical Speed
            "52": random.uniform(-30, 30),  # Platform Sideslip Angle
            "53": random.uniform(900, 1050),  # Airfield Barometric Pressure
            "54": random.uniform(0, 5000),  # Airfield Elevation
            "55": random.uniform(10, 95),  # Relative Humidity
            "56": math.sqrt(self.north_vel**2 + self.east_vel**2),  # Platform Ground Speed
            "57": random.uniform(1000, 100000),  # Ground Range
            "58": random.uniform(1000, 10000),  # Platform Fuel Remaining
            "59": "CALLSIGN-1",  # Platform Call Sign
            "60": random.randint(0, 10000),  # Weapon Load
            "61": random.randint(0, 1),  # Weapon Fired
            "62": random.randint(1000, 10000),  # Laser PRF Code
            "63": "WIDE-AREA-FLIR",  # Sensor FOV Name
            "64": self.heading,  # Platform Magnetic Heading
            "65": 1,  # UAS Datalink LS Version
            "67": self.latitude + random.uniform(-0.1, 0.1),  # Alternate Platform Lat
            "68": self.longitude + random.uniform(-0.1, 0.1),  # Alternate Platform Lon
            "69": self.altitude + random.uniform(-200, 200),  # Alternate Platform Altitude
            "70": "ALT-PLATFORM-A",  # Alternate Platform Name
            "71": self.heading + random.uniform(-10, 10),  # Alternate Platform Heading
            "72": self.event_start_time_us,  # Event Start Time (microseconds)
            "75": self.altitude,  # Sensor Ellipsoid Height
            "76": self.altitude,  # Alternate Platform Ellipsoid Height
            "77": random.randint(0, 255),  # Operational Mode
            "78": self.altitude,  # Frame Center Height Above Ellipsoid
            "79": self.north_vel,  # Sensor North Velocity
            "80": self.east_vel,  # Sensor East Velocity
            "81": self.up_vel,  # Sensor Up Velocity
            "82": self.north_vel * 1.05,  # Platform North Velocity
            "83": self.east_vel * 1.05,  # Platform East Velocity
            "84": self.up_vel * 1.05,  # Platform Up Velocity
            "85": self.north_vel * 0.9,  # Alternate Platform North Velocity
            "86": self.east_vel * 0.9,  # Alternate Platform East Velocity
            "87": self.up_vel * 0.9,  # Alternate Platform Up Velocity
            "88": self.pitch,  # Platform Pitch Angle Full
            "89": self.roll,  # Platform Roll Angle Full
            "90": random.uniform(-45, 45),  # Platform Angle of Attack Full
            "91": random.uniform(-30, 30),  # Platform Sideslip Angle Full
        }
        return tags


REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_TAGS_INI = REPO_ROOT / "data" / "stanag4609_tags.ini"
# Meson puts the .so in build/src/; CMake puts it directly in build/
_meson_dir = REPO_ROOT / "build" / "src"
_cmake_dir = REPO_ROOT / "build"
DEFAULT_PLUGIN_DIR = _meson_dir if (_meson_dir / "gstklvplugin.so").exists() else _cmake_dir


def mpegtsmux_supports_enable_custom_mappings():
    probe = Gst.ElementFactory.make("mpegtsmux", None)
    if not probe:
        return False
    return probe.find_property("enable-custom-mappings") is not None


def run_sender(
    host="localhost",
    port=5000,
    bitrate=2000,
    count=100,
    print_all=True,
    local_set_args=None,
    local_set_demo=False,
    metadata_app_format=0xFFFF,
    metadata_app_identifier="MISB",
    metadata_format=0xFF,
    metadata_format_identifier="KLVA",
    metadata_service_id=0x01,
    metadata_flags=0x00,
):
    """Run SRT sender with randomized KLV tags."""

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
    supports_custom_mappings = mpegtsmux_supports_enable_custom_mappings()
    mux_custom_mapping_opt = " enable-custom-mappings=true" if supports_custom_mappings else ""

    gop = max(2, FPS * 2)
    pipeline_str = (
        "videotestsrc is-live=true ! "
        "videoconvert ! "
        f"video/x-raw,format=I420,width=640,height=480,framerate={FPS}/1 ! "
        f"x264enc name=enc bitrate={bitrate} speed-preset=ultrafast tune=zerolatency ! "
        "h264parse name=vparse ! "
        "video/x-h264,stream-format=byte-stream,alignment=au ! "
        "klvframeinject name=inject "
        # MPEG-TS over SRT is more reliable when buffers are aligned to 7 TS
        # packets (7 * 188 = 1316 bytes), which is the common live-streaming size.
        f"inject.video_src ! queue ! mpegtsmux name=mux{mux_custom_mapping_opt} ! tspmtrewrite name=pmtrw ! "
        f"srtsink mode=listener localaddress={host} localport={port} latency=125 wait-for-connection=false sync=false async=false "
        "inject.klv_src ! queue ! meta/x-klv,parsed=true,stream-format=klv,stream-type=(int)21 ! mux."
    )

    print("\n" + "=" * 100)
    print("SRT SENDER - MISB ST 0601.8 KLV TAGS")
    print("=" * 100 + "\n")
    print("> Configuration:")
    print(f"  Host (bind): {host}")
    print(f"  Port: {port}")
    print(f"  Framerate: {FPS} fps")
    print(f"  Bitrate: {bitrate} kbps")
    print(f"  Plugin: {plugin_path}/gstklvplugin.so")
    print(f"  Tags: {ini_path}")
    print(f"  mpegtsmux custom mappings: {'enabled' if supports_custom_mappings else 'disabled (not supported by this GST)'}")
    print(f"  SRT: listener on {host}:{port}\n")
    print("> PMT metadata descriptor:")
    print(f"  metadata_application_format: 0x{metadata_app_format:04X}")
    print(f"  metadata_application_format_identifier: {metadata_app_identifier}")
    print(f"  metadata_format: 0x{metadata_format:02X}")
    print(f"  metadata_format_identifier: {metadata_format_identifier}")
    print(f"  metadata_service_id: 0x{metadata_service_id:02X}")
    print(f"  decoder_config_flags: 0x{metadata_flags:02X}\n")

    print("> Starting GStreamer pipeline...")
    print("  Press Ctrl+C to stop\n")

    pipeline = Gst.parse_launch(pipeline_str)
    if not pipeline:
        print("ERROR Failed to create pipeline")
        return 1

    mux = pipeline.get_by_name("mux")
    if mux and mux.find_property("alignment") is not None:
        mux.set_property("alignment", 7)

    enc = pipeline.get_by_name("enc")
    if enc:
        enc.set_property("key-int-max", gop)
        if enc.find_property("bframes") is not None:
            enc.set_property("bframes", 0)
        if enc.find_property("option-string") is not None:
            enc.set_property(
                "option-string",
                f"keyint={gop}:min-keyint={gop}:scenecut=0:repeat-headers=1:aud=1",
            )
        if enc.find_property("aud") is not None:
            enc.set_property("aud", True)
    vparse = pipeline.get_by_name("vparse")
    if vparse and vparse.find_property("config-interval") is not None:
        vparse.set_property("config-interval", -1)

    inject = pipeline.get_by_name("inject")
    if not inject:
        print("ERROR Failed to get inject element")
        pipeline.set_state(Gst.State.NULL)
        return 1
    pmtrw = pipeline.get_by_name("pmtrw")
    if not pmtrw:
        print("ERROR Failed to get PMT rewrite element")
        pipeline.set_state(Gst.State.NULL)
        return 1
    pmtrw.set_property("metadata-app-format", metadata_app_format)
    pmtrw.set_property("metadata-app-identifier", metadata_app_identifier)
    pmtrw.set_property("metadata-format", metadata_format)
    pmtrw.set_property("metadata-format-identifier", metadata_format_identifier)
    pmtrw.set_property("metadata-service-id", metadata_service_id)
    pmtrw.set_property("metadata-flags", metadata_flags)

    tag_gen = TagValueGenerator()
    frame_state = {"count": 0}
    eos_sent = {"value": False}
    loop_state = {"loop": None}
    local_set_args = local_set_args or []

    def parse_local_set_arg(arg):
        if "=" not in arg:
            raise ValueError("local-set must be in the form <tag_id>=hex:... | base64:... | @file")
        key, value = arg.split("=", 1)
        tag_id = int(key.strip())
        value = value.strip()

        if value.startswith("@"):
            data = Path(value[1:]).read_bytes()
            b64 = base64.b64encode(data).decode("ascii")
            return tag_id, f"base64:{b64}"
        if value.startswith("hex:") or value.startswith("base64:"):
            return tag_id, value
        raise ValueError("local-set payload must start with hex:, base64:, or @file")

    def parse_local_sets(args_list):
        local_sets = {}
        for item in args_list:
            tag_id, payload = parse_local_set_arg(item)
            local_sets[tag_id] = payload
        return local_sets

    def build_demo_local_sets():
        def tlv(tag_id, value_bytes):
            return bytes([tag_id, len(value_bytes)]) + value_bytes

        payload = tlv(1, b"\x00")
        b64 = base64.b64encode(payload).decode("ascii")
        demo = {}
        for tid in (48, 66, 73, 74, 92, 93):
            demo[tid] = f"base64:{b64}"
        return demo

    def bytes_preview(payload):
        try:
            if payload.startswith("hex:"):
                hex_str = re.sub(r"[^0-9a-fA-F]", "", payload[4:])
                data = bytes.fromhex(hex_str)
            elif payload.startswith("base64:"):
                data = base64.b64decode(payload[7:])
            else:
                return "bytes(?)"
        except Exception:
            return "bytes(?)"
        preview = data[:16].hex()
        return f"bytes[{len(data)}]: {preview}{'...' if len(data) > 16 else ''}"

    try:
        local_sets = parse_local_sets(local_set_args)
    except ValueError as exc:
        print(f"ERROR Invalid --local-set: {exc}")
        pipeline.set_state(Gst.State.NULL)
        return 1

    if local_set_demo:
        demo_sets = build_demo_local_sets()
        for tid, payload in demo_sets.items():
            local_sets.setdefault(tid, payload)

    for tag_id in list(local_sets.keys()):
        meta = tag_defs.get(tag_id)
        if not meta or meta.get("type", "").lower() != "bytes":
            print(f"Warning: ignoring --local-set for tag {tag_id} (not a bytes/local-set tag)")
            del local_sets[tag_id]

    def filter_tags(tags, local_sets):
        filtered = {}
        for key, value in tags.items():
            tag_id = int(key)
            meta = tag_defs.get(tag_id)
            if not meta:
                continue
            if not meta.get("type"):
                continue
            if meta.get("type", "").lower() == "bytes" and tag_id not in local_sets:
                continue
            if tag_id == 1:
                continue
            filtered[key] = value
        for tag_id, payload in local_sets.items():
            filtered[str(tag_id)] = payload
        return filtered

    def clamp_tags(tags):
        clamped = {}
        for key, value in tags.items():
            tag_id = int(key)
            meta = tag_defs.get(tag_id, {})
            ttype = meta.get("type", "")
            rng = meta.get("range")
            if rng and not ttype.lower().startswith("string"):
                try:
                    vmin, vmax = rng
                    if isinstance(value, (int, float)):
                        value = max(vmin, min(vmax, value))
                except Exception:
                    pass
            clamped[key] = value
        return clamped

    # Initialize with first set of tags before starting pipeline
    tag_gen.update()
    init_tags = clamp_tags(filter_tags(tag_gen.get_tags(), local_sets))
    inject.set_property("tags-json", json.dumps(init_tags))

    # Sync metadata to actual video buffers
    sink_pad = inject.get_static_pad("sink")
    if not sink_pad:
        print("ERROR Failed to get inject sink pad")
        pipeline.set_state(Gst.State.NULL)
        return 1

    def on_buffer(pad, info):
        frame_state["count"] += 1
        tag_gen.update()
        tags = clamp_tags(filter_tags(tag_gen.get_tags(), local_sets))
        inject.set_property("tags-json", json.dumps(tags))

        print("\n" + "-" * 80)
        sent_count = len(tags) + 1  # checksum is injected in the plugin
        print(f"[FRAME {frame_state['count']:04d}] Sent {sent_count} tags (incl. checksum)")
        if print_all:
            for key in sorted(tags.keys(), key=lambda k: int(k)):
                tag_id = int(key)
                meta = tag_defs.get(tag_id, {"name": f"Tag {tag_id}", "units": ""})
                unit = f" {meta['units']}" if meta.get("units") else ""
                if meta.get("type", "").lower() == "bytes":
                    print(f"  {tag_id:02d} {meta['name']}: {bytes_preview(tags[key])}")
                else:
                    print(f"  {tag_id:02d} {meta['name']}: {tags[key]}{unit}")
        else:
            print(f"  13 Sensor Latitude:  {tags['13']:.6f} Degrees")
            print(f"  14 Sensor Longitude: {tags['14']:.6f} Degrees")
            print(f"  15 Sensor True Altitude: {tags['15']:.1f} Meters")
        print("-" * 80)

        if count > 0 and frame_state["count"] >= count and not eos_sent["value"]:
            eos_sent["value"] = True
            if loop_state["loop"] is not None:
                GLib.idle_add(loop_state["loop"].quit)
            pipeline.send_event(Gst.Event.new_eos())
            return Gst.PadProbeReturn.REMOVE
        return Gst.PadProbeReturn.OK

    sink_pad.add_probe(Gst.PadProbeType.BUFFER, on_buffer)

    ret = pipeline.set_state(Gst.State.PLAYING)
    if ret == Gst.StateChangeReturn.FAILURE:
        print("ERROR Failed to start pipeline")
        pipeline.set_state(Gst.State.NULL)
        return 1

    print("OK Pipeline started")
    print("OK Streaming frames with randomized tags...\n")

    loop = GLib.MainLoop()
    loop_state["loop"] = loop
    bus = pipeline.get_bus()
    bus.add_signal_watch()

    def on_message(bus, message):
        msg_type = message.type
        if msg_type == Gst.MessageType.ERROR:
            err, debug = message.parse_error()
            print(f"\nERROR Pipeline error: {err.message}")
            if debug:
                print(f"  Debug: {debug}")
            loop.quit()
        elif msg_type == Gst.MessageType.EOS:
            loop.quit()

    bus.connect("message", on_message)

    try:
        loop.run()
    except KeyboardInterrupt:
        pass
    finally:
        pipeline.set_state(Gst.State.NULL)

    print(f"\nOK Pipeline stopped ({frame_state['count']} frames sent)")
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="SRT Sender with MISB ST 0601.8 KLV tags"
    )
    parser.add_argument("--host", default="0.0.0.0", help="Bind host (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=5000, help="SRT port (default: 5000)")
    parser.add_argument("--count", type=int, default=100, help="Frames to send (0 = infinite)")
    parser.add_argument("--bitrate", type=int, default=2000, help="Video bitrate kbps (default: 2000)")
    parser.add_argument("--print-all", action="store_true", help="Print all tags each frame (default)")
    parser.add_argument("--print-summary", action="store_true", help="Only print tags 13/14/15")
    parser.add_argument(
        "--local-set",
        action="append",
        default=[],
        help="Attach local set bytes: <tag_id>=hex:... | base64:... | @file (repeatable)",
    )
    parser.add_argument(
        "--local-set-demo",
        action="store_true",
        help="Attach minimal demo local sets for tags 48/66/73/74/92/93",
    )
    parser.add_argument(
        "--metadata-app-format",
        type=lambda v: int(v, 0),
        default=0xFFFF,
        help="metadata_application_format (default: 0xFFFF)",
    )
    parser.add_argument(
        "--metadata-app-identifier",
        default="MISB",
        help="metadata_application_format_identifier (4 chars, default: MISB)",
    )
    parser.add_argument(
        "--metadata-format",
        type=lambda v: int(v, 0),
        default=0xFF,
        help="metadata_format (default: 0xFF)",
    )
    parser.add_argument(
        "--metadata-format-identifier",
        default="KLVA",
        help="metadata_format_identifier (4 chars, default: KLVA)",
    )
    parser.add_argument(
        "--metadata-service-id",
        type=lambda v: int(v, 0),
        default=0x01,
        help="metadata_service_id (default: 0x01)",
    )
    parser.add_argument(
        "--metadata-flags",
        type=lambda v: int(v, 0),
        default=0x00,
        help="decoder_config_flags (default: 0x00)",
    )

    args = parser.parse_args()

    try:
        return run_sender(
            host=args.host,
            port=args.port,
            bitrate=args.bitrate,
            count=args.count,
            print_all=False if args.print_summary else True,
            local_set_args=args.local_set,
            local_set_demo=args.local_set_demo,
            metadata_app_format=args.metadata_app_format,
            metadata_app_identifier=args.metadata_app_identifier,
            metadata_format=args.metadata_format,
            metadata_format_identifier=args.metadata_format_identifier,
            metadata_service_id=args.metadata_service_id,
            metadata_flags=args.metadata_flags,
        )
    except KeyboardInterrupt:
        print("\nERROR Interrupted by user")
        return 1


if __name__ == "__main__":
    sys.exit(main())
