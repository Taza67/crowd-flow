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
PACKET = RESULTS / "packet.json"
CLIP_DIR = RESULTS / "clips"
_DATA_MARK = "const DATA = "

CLIP_N = SWEEP_N
CLIP_STEPS = 10000
CLIP_BLOCK_NS = (500, 2000, 10000)
CLIP_BLOCK_N = 2000
CPU_WORKERS = 4


def bind_clip_files(data: dict) -> dict:
    for clip in data.get("clips") or []:
        for t in clip.get("tracks") or []:
            rel = t.get("file") or ""
            name = Path(rel).name
            src = CLIP_DIR / name
            if src.is_file():
                t["file"] = f"{CLIP_DIR.name}/{name}"
    return data


def load_packet() -> dict:
    if PACKET.is_file():
        data = json.loads(PACKET.read_text(encoding="utf-8"))
    elif not OUT.is_file():
        die("dashboard", f"missing {OUT}  run make dashboard first")
    else:
        html = OUT.read_text(encoding="utf-8")
        i = html.find(_DATA_MARK)
        if i < 0:
            die("dashboard", f"no DATA in {OUT}")
        i += len(_DATA_MARK)
        j = html.find(";\n", i)
        if j < 0:
            j = html.find(";", i)
        if j < 0:
            die("dashboard", f"truncated DATA in {OUT}")
        data = json.loads(html[i:j])
    return bind_clip_files(data)


def write_dashboard(data: dict) -> Path:
    if not TEMPLATE.is_file():
        die("dashboard", f"missing {TEMPLATE}")
    blob = json.dumps(data, separators=(",", ":")).replace("<", "\\u003c")
    html = TEMPLATE.read_text(encoding="utf-8").replace("__DATA__", blob)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(html, encoding="utf-8")
    PACKET.write_text(json.dumps(data, separators=(",", ":")), encoding="utf-8")
    return OUT


def rebuild_html(*, fit: float | None = None) -> Path:
    data = load_packet()
    if fit is not None:
        data["fit"] = fit
    return write_dashboard(data)


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
