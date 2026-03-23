#!/usr/bin/env python3
"""
@file tests/smoke/smoke_staged_install_clean_registry.py
@brief Smoke test for staged installation and clean-registry plugin discovery.
@ingroup gstklv_tests

This test performs a staged Meson install outside the main build tree, then
verifies that GStreamer discovers the installed plugin from a fresh registry
file and that the plugin does not appear in the blacklist output.

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
    env = os.environ.copy()
    env["PYTHONUNBUFFERED"] = "1"

    with tempfile.TemporaryDirectory(prefix="gstklv-install-smoke-") as tmpdir:
        tmp_root = Path(tmpdir)
        build_dir = tmp_root / "build"
        stage_dir = tmp_root / "stage"
        registry_path = tmp_root / "gst-registry-clean.bin"

        run_checked(
            ["meson", "setup", str(build_dir), "--prefix", "/usr"],
            env=env,
            timeout=180,
        )
        run_checked(["meson", "compile", "-C", str(build_dir)], env=env, timeout=240)
        run_checked(
            ["meson", "install", "-C", str(build_dir), "--destdir", str(stage_dir)],
            env=env,
            timeout=180,
        )

        plugin_file = next(stage_dir.rglob("gstklvplugin.so"), None)
        ini_path = next(stage_dir.rglob("stanag4609_tags.ini"), None)

        if plugin_file is None:
            raise SystemExit("staged install did not contain gstklvplugin.so")
        if ini_path is None:
            raise SystemExit("staged install did not contain stanag4609_tags.ini")

        inspect_env = env.copy()
        inspect_env["GST_PLUGIN_PATH"] = str(plugin_file.parent)
        inspect_env["KLV_TAGS_INI"] = str(ini_path)
        inspect_env["GST_REGISTRY"] = str(registry_path)

        plugin_out = run_checked(
            ["gst-inspect-1.0", "--plugin", "klvplugin"],
            env=inspect_env,
            timeout=60,
        )

        if (
            "Plugin Details:" not in plugin_out
            or "4 features" not in plugin_out
            or "klvmetaenc" not in plugin_out
            or "MIT" not in plugin_out
        ):
            raise SystemExit("gst-inspect did not report the expected staged plugin details")

        exists_proc = subprocess.run(
            ["gst-inspect-1.0", "--exists", "klvmetaenc"],
            cwd=REPO_ROOT,
            env=inspect_env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=60,
            check=False,
        )
        sys.stdout.write(exists_proc.stdout)
        if exists_proc.returncode != 0:
            raise SystemExit("klvmetaenc was not visible from the staged install")

        blacklist_out = run_checked(["gst-inspect-1.0", "-b"], env=inspect_env, timeout=60)
        plugin_name = plugin_file.name
        if plugin_name in blacklist_out or "klvplugin" in blacklist_out:
            raise SystemExit("staged plugin appeared in blacklist output")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
