"""Shared paths, dump CSV columns, and world constants."""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CPU_BIN = ROOT / "build" / "crowd-sim-cpu"
GPU_BIN = ROOT / "build" / "crowd-sim-gpu"
CPU_MODES = ("cpu_naive", "cpu_opt")
GPU_MODES = ("gpu_naive", "gpu_opt")
MODES = CPU_MODES + GPU_MODES
_gpu_ok: bool | None = None

DUMP_COLS = ("t", "id", "x", "y", "vx", "vy", "goal", "step_ms")
PHYS_COLS = ("t", "id", "x", "y", "vx", "vy", "goal")
STATE_COLS = ("x", "y", "vx", "vy")

WORLD_SIZE = 200.0
WORLD_HALF = WORLD_SIZE * 0.5
AGENT_RADIUS = 0.6
XY_SCALE = 32767.0 / WORLD_HALF
REC_MAGIC = b"CF1\0"


def binary(mode: str) -> Path:
    return GPU_BIN if mode.startswith("gpu_") else CPU_BIN


def ensure_bin(path: Path) -> bool:
    if path.is_file() and os.access(path, os.X_OK):
        return True
    target = "gpu" if path == GPU_BIN else "cpu"
    subprocess.check_call(["make", "-C", str(ROOT), target])
    return path.is_file() and os.access(path, os.X_OK)


def log(status: str, topic: str, extra: str = "", *, err: bool = False) -> None:
    line = f"{status:<4}  {topic:<12}"
    if extra:
        line += f"  {extra}"
    print(line, file=sys.stderr if err else sys.stdout)


def have_gpu() -> bool:
    global _gpu_ok
    if _gpu_ok is None:
        _gpu_ok = ensure_bin(GPU_BIN)
        if not _gpu_ok:
            log("WARN", "gpu", "nvcc not found  skip", err=True)
    return _gpu_ok


def modes_for(cmd: str | None) -> tuple[str, ...]:
    if cmd == "cpu":
        return CPU_MODES
    if cmd == "gpu":
        return GPU_MODES
    return MODES


def die(topic: str, extra: str = "") -> None:
    log("FAIL", topic, extra, err=True)
    raise SystemExit(1)


def check_proc(proc: subprocess.CompletedProcess, topic: str, extra: str) -> None:
    err = proc.stderr
    if err:
        text = err.decode() if isinstance(err, bytes) else err
        text = text.rstrip()
        if text:
            print(text, file=sys.stderr)
    if proc.returncode != 0:
        die(topic, extra)


def run_crowd(
    mode: str,
    verb: str,
    n: int,
    steps: int,
    *,
    capture_output: bool = False,
    stdout=None,
    text: bool = True,
) -> subprocess.CompletedProcess:
    path = binary(mode)
    if not ensure_bin(path):
        die("gpu" if mode.startswith("gpu_") else "cpu", "binary missing")
    cmd = [str(path), verb, str(n), str(steps), mode]
    if capture_output:
        return subprocess.run(cmd, capture_output=True, text=True)
    return subprocess.run(
        cmd, stdout=stdout, stderr=subprocess.PIPE, text=text
    )


def dump_data_text(text: str) -> str:
    return "\n".join(
        ln for ln in text.splitlines() if ln.strip() and not ln.startswith("#")
    )


def parse_obstacles(text: str) -> list[dict[str, float]]:
    out = []
    for ln in text.splitlines():
        if not ln.startswith("#obstacle,"):
            continue
        parts = ln.split(",")
        if len(parts) != 4 or parts[1] == "x":
            continue
        out.append(
            {
                "x": float(parts[1]),
                "y": float(parts[2]),
                "radius": float(parts[3]),
            }
        )
    return out
