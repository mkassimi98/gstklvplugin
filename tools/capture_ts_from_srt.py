#!/usr/bin/env python3
"""
@file tools/capture_ts_from_srt.py
@brief Capture an MPEG-TS stream from SRT to a local file.
@ingroup gstklv_tools

This utility records a live MPEG-TS stream received over SRT and stores it as
an on-disk transport stream for later inspection or replay.

Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
"""

import argparse
from pathlib import Path
import sys

import gi

gi.require_version("Gst", "1.0")
from gi.repository import Gst, GLib


def main():
    parser = argparse.ArgumentParser(description="Capture TS from SRT to a file")
    parser.add_argument("--host", default="127.0.0.1", help="SRT host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=5000, help="SRT port (default: 5000)")
    parser.add_argument("--output", default="capture.ts", help="Output TS file path")
    parser.add_argument("--duration", type=float, default=5.0, help="Capture duration in seconds")
    args = parser.parse_args()

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    Gst.init([])

    pipeline_str = (
        f"srtsrc uri=srt://{args.host}:{args.port}?mode=caller ! "
        f"filesink location={output_path}"
    )

    pipeline = Gst.parse_launch(pipeline_str)
    if not pipeline:
        print("ERROR Failed to create pipeline")
        return 1

    print(f"> Capturing TS from srt://{args.host}:{args.port} to {output_path}")
    print(f"> Duration: {args.duration} seconds")

    loop = GLib.MainLoop()
    bus = pipeline.get_bus()
    bus.add_signal_watch()

    def on_message(bus, message):
        msg_type = message.type
        if msg_type == Gst.MessageType.ERROR:
            err, debug = message.parse_error()
            print(f"ERROR Pipeline error: {err.message}")
            if debug:
                print(f"  Debug: {debug}")
            loop.quit()
        elif msg_type == Gst.MessageType.EOS:
            loop.quit()

    bus.connect("message", on_message)

    def on_timeout():
        pipeline.send_event(Gst.Event.new_eos())
        return False

    GLib.timeout_add(int(args.duration * 1000), on_timeout)

    ret = pipeline.set_state(Gst.State.PLAYING)
    if ret == Gst.StateChangeReturn.FAILURE:
        print("ERROR Failed to start pipeline")
        pipeline.set_state(Gst.State.NULL)
        return 1

    try:
        loop.run()
    finally:
        pipeline.set_state(Gst.State.NULL)

    print("OK Capture complete")
    return 0


if __name__ == "__main__":
    sys.exit(main())
