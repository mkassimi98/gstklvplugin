#!/usr/bin/env python3
"""
@file tests/smoke/smoke_udp_loopback.py
@brief Smoke test for the UDP sender and receiver example scripts.
@ingroup gstklv_tests

This test starts the Python UDP receiver in headless mode, streams a small
number of frames from the Python UDP sender, and verifies that the receiver
captures decoded KLV output.

Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
"""

from __future__ import annotations

import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time


REPO_ROOT = Path(__file__).resolve().parents[2]
SENDER = REPO_ROOT / "examples" / "udp-pipelines" / "python" / "udp_sender_93tags.py"
RECEIVER = REPO_ROOT / "examples" / "udp-pipelines" / "python" / "udp_receiver_93tags.py"


def pick_free_udp_port() -> int:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


def run_checked(cmd: list[str], env: dict[str, str], timeout: int) -> str:
    proc = subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=timeout,
        check=False,
    )
    sys.stdout.write(proc.stdout)
    if proc.returncode != 0:
        raise SystemExit(f"command failed with exit code {proc.returncode}: {' '.join(cmd)}")
    return proc.stdout


def main() -> int:
    env = os.environ.copy()
    env.setdefault("GST_PLUGIN_PATH", str(REPO_ROOT / "build" / "src"))
    env.setdefault("KLV_TAGS_INI", str(REPO_ROOT / "data" / "stanag4609_tags.ini"))
    env["PYTHONUNBUFFERED"] = "1"

    port = pick_free_udp_port()

    with tempfile.TemporaryDirectory(prefix="gstklv-udp-smoke-") as tmpdir:
        output_path = Path(tmpdir) / "receiver.jsonl"

        receiver = subprocess.Popen(
            [
                sys.executable,
                "-u",
                str(RECEIVER),
                "--host",
                "127.0.0.1",
                "--port",
                str(port),
                "--headless",
                "--print-summary",
                "--output",
                str(output_path),
            ],
            cwd=REPO_ROOT,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )

        try:
            time.sleep(1.5)

            sender_out = run_checked(
                [
                    sys.executable,
                    str(SENDER),
                    "--host",
                    "127.0.0.1",
                    "--port",
                    str(port),
                    "--count",
                    "5",
                    "--print-summary",
                ],
                env,
                timeout=90,
            )

            if "OK Pipeline stopped (5 frames sent)" not in sender_out:
                raise SystemExit("sender output did not report the expected frame count")

            time.sleep(1.5)
            receiver.send_signal(signal.SIGINT)
            receiver_out, _ = receiver.communicate(timeout=20)
        except BaseException:
            receiver.kill()
            receiver_out, _ = receiver.communicate(timeout=20)
            sys.stdout.write(receiver_out)
            raise

        sys.stdout.write(receiver_out)

        if receiver.returncode not in (0, 130):
            raise SystemExit(f"receiver exited with unexpected status {receiver.returncode}")
        if not output_path.exists() or output_path.stat().st_size == 0:
            raise SystemExit("receiver did not write any decoded JSON output")

        lines = [line for line in output_path.read_text(encoding="utf-8").splitlines() if line.strip()]
        if len(lines) < 2:
            raise SystemExit("receiver captured too few JSON frames")

        sample = json.loads(lines[0])
        tags = sample["tags"] if isinstance(sample, dict) and "tags" in sample else sample
        for required in ("2", "13", "14", "15"):
            if required not in tags:
                raise SystemExit(f"receiver JSON output is missing required tag {required}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
