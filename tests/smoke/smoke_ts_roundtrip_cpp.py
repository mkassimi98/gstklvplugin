#!/usr/bin/env python3
"""
@file tests/smoke/smoke_ts_roundtrip_cpp.py
@brief Smoke test for the C++ TS recorder and reader examples.
@ingroup gstklv_tests

This test configures a temporary CMake build for the C++ examples, records a
short MPEG-TS file with the C++ recorder, and replays it through the C++
reader in headless mode.

Author: Mouhsine Kassimi Farhaoui
Mail: mouhsine98@gmail.com
"""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


REPO_ROOT = Path(__file__).resolve().parents[2]
TAGS_INI = REPO_ROOT / "data" / "stanag4609_tags.ini"


def run_checked(cmd: list[str], env: dict[str, str] | None = None, timeout: int = 180) -> str:
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
    cmake = shutil.which("cmake")
    if cmake is None:
        raise SystemExit("cmake is required for the C++ smoke test")

    env = os.environ.copy()
    env["PYTHONUNBUFFERED"] = "1"
    env.pop("GST_PLUGIN_PATH", None)
    env.pop("KLV_TAGS_INI", None)

    with tempfile.TemporaryDirectory(prefix="gstklv-cpp-ts-smoke-") as tmpdir:
        tmp_root = Path(tmpdir)
        build_dir = tmp_root / "build"
        ts_path = tmp_root / "capture.ts"

        run_checked(
            [
                cmake,
                "-S",
                str(REPO_ROOT),
                "-B",
                str(build_dir),
                "-DGSTKLVPLUGIN_BUILD_EXAMPLES=ON",
            ],
            env=env,
            timeout=180,
        )

        run_checked(
            [
                cmake,
                "--build",
                str(build_dir),
                "--target",
                "gstklvplugin",
                "gstklv_ts_recorder",
                "gstklv_ts_reader",
            ],
            env=env,
            timeout=240,
        )

        recorder = build_dir / "gstklv_ts_recorder"
        reader = build_dir / "gstklv_ts_reader"

        recorder_out = run_checked(
            [
                str(recorder),
                "--output",
                str(ts_path),
                "--count",
                "4",
                "--print-summary",
                "--plugin-path",
                str(build_dir),
                "--tags-ini",
                str(TAGS_INI),
            ],
            env=env,
            timeout=120,
        )

        if not ts_path.exists() or ts_path.stat().st_size == 0:
            raise SystemExit("C++ recorder did not produce a non-empty TS file")
        if "OK Recording complete (4 frames)" not in recorder_out:
            raise SystemExit("C++ recorder output did not report the expected frame count")

        reader_out = run_checked(
            [
                str(reader),
                "--input",
                str(ts_path),
                "--headless",
                "--print-summary",
                "--plugin-path",
                str(build_dir),
                "--tags-ini",
                str(TAGS_INI),
            ],
            env=env,
            timeout=120,
        )

        if "[FRAME 0001]" not in reader_out:
            raise SystemExit("C++ reader did not print any decoded frame output")
        if "OK Playback complete (4 frames)" not in reader_out:
            raise SystemExit("C++ reader output did not report the expected frame count")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
