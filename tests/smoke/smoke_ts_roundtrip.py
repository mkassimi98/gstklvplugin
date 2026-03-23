#!/usr/bin/env python3
"""
@file tests/smoke/smoke_ts_roundtrip.py
@brief Smoke test for MPEG-TS record and playback example scripts.
@ingroup gstklv_tests

This test records a short transport stream with the Python recorder example and
then replays it through the Python reader example in headless mode.

Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
"""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys
import tempfile


REPO_ROOT = Path(__file__).resolve().parents[2]
RECORDER = REPO_ROOT / "examples" / "ts" / "python" / "klv_recorder.py"
READER = REPO_ROOT / "examples" / "ts" / "python" / "klv_video_reader.py"


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

    with tempfile.TemporaryDirectory(prefix="gstklv-ts-smoke-") as tmpdir:
        ts_path = Path(tmpdir) / "capture.ts"

        recorder_out = run_checked(
            [
                sys.executable,
                str(RECORDER),
                "--output",
                str(ts_path),
                "--fps",
                "5",
                "--count",
                "4",
                "--print-summary",
            ],
            env,
            timeout=90,
        )

        if not ts_path.exists() or ts_path.stat().st_size == 0:
            raise SystemExit("recorder did not produce a non-empty TS file")
        if "OK Recording complete (4 frames)" not in recorder_out:
            raise SystemExit("recorder output did not report the expected frame count")

        reader_out = run_checked(
            [
                sys.executable,
                str(READER),
                str(ts_path),
                "--headless",
                "--print-summary",
            ],
            env,
            timeout=90,
        )

        if "[FRAME 0001]" not in reader_out:
            raise SystemExit("reader did not print any decoded frame output")
        if "OK Playback complete (4 frames)" not in reader_out:
            raise SystemExit("reader output did not report the expected frame count")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
