"""Dashboard packet: one HTML, clips + sweeps."""

from __future__ import annotations

import json
from pathlib import Path

from cfutil import (
    AGENT_RADIUS,
    BLOCKS,
    CLIP_NAIVE_CAP,
    CPU_CAP,
    DEFAULT_BLOCK,
    NAIVE_CAP,
    ROOT,
    SWEEP_N,
    SWEEP_STEPS,
    WORLD_HALF,
    WORLD_SIZE,
    die,
)

TEMPLATE = Path(__file__).with_name("dashboard.html")
RESULTS = ROOT / "results"
OUT = RESULTS / "dashboard.html"
CLIP_DIR = RESULTS / "clips"

CLIP_N = SWEEP_N
CLIP_STEPS = 10000
CLIP_BLOCK_NS = (500, 2000, 10000)
CLIP_BLOCK_N = 2000
CPU_WORKERS = 4


def write_dashboard(packet: dict) -> Path:
    if not TEMPLATE.is_file():
        die("dashboard", f"missing {TEMPLATE}")
    data = json.dumps(packet, separators=(",", ":")).replace("<", "\\u003c")
    html = TEMPLATE.read_text(encoding="utf-8").replace("__DATA__", data)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(html, encoding="utf-8")
    return OUT


def packet(
    clips: list[dict],
    sweeps: list[dict],
    *,
    fit: float = 8.0,
    blocks: tuple[int, ...] = BLOCKS,
) -> dict:
    if CLIP_BLOCK_N not in CLIP_BLOCK_NS:
        die("dashboard", f"clip_block_n={CLIP_BLOCK_N} not in clip_block_ns")
    return {
        "fit": fit,
        "world_size": WORLD_SIZE,
        "world_half": WORLD_HALF,
        "agent_radius": AGENT_RADIUS,
        "meta": {
            "seed": 42,
            "blocks": list(blocks),
            "naive_cap": NAIVE_CAP,
            "cpu_cap": CPU_CAP,
            "clip_naive_cap": CLIP_NAIVE_CAP,
            "clip_block_n": CLIP_BLOCK_N,
            "clip_block_ns": list(CLIP_BLOCK_NS),
            "default_block": DEFAULT_BLOCK,
            "sweep_steps": SWEEP_STEPS,
            "clip_steps": CLIP_STEPS,
        },
        "clips": clips,
        "sweeps": sweeps,
    }
