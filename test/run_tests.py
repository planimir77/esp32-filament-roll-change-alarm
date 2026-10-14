"""Run firmware logic and dashboard unit tests without an ESP32."""
from pathlib import Path
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / ".pio" / "unit-tests"
BUILD.mkdir(parents=True, exist_ok=True)
zig = ROOT / ".pio" / "test-tools" / "ziglang" / ("zig.exe" if os.name == "nt" else "zig")
compiler = [str(zig), "c++", "-nostdlib++"] if zig.exists() else [shutil.which("g++") or shutil.which("clang++") or ""]
if not compiler[0]:
    sys.exit("Install a C++ compiler or run: python -m pip install --target .pio/test-tools ziglang==0.16.0")
node = shutil.which("node")
if not node:
    sys.exit("Node.js 18+ is required.")
env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(BUILD / "zig-cache"))
binary = BUILD / ("mute_timer.exe" if os.name == "nt" else "mute_timer")
def run(args):
    subprocess.run(args, cwd=ROOT, env=env, check=True)
try:
    run(compiler + ["-std=c++11", "-Wall", "-Wextra", "-Werror", "-I", "include",
                    "test/unit/mute_timer.cpp", "-o", str(binary)])
    run([str(binary)])
    run([node, "--test", "test/unit/dashboard.test.cjs", "test/unit/web_ota.test.cjs"])
except subprocess.CalledProcessError as error:
    sys.exit(error.returncode)
