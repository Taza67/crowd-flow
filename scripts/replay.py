#!/usr/bin/env python3
"""Replay recordings in the browser (independent clocks, recorded step_ms)."""

from __future__ import annotations

import argparse
import array
import csv
import json
import math
import struct
import tempfile
import webbrowser
from base64 import b64encode
from collections import defaultdict
from pathlib import Path

from cfutil import (
    AGENT_RADIUS,
    DUMP_COLS,
    REC_MAGIC,
    ROOT,
    WORLD_HALF,
    WORLD_SIZE,
    XY_SCALE,
    check_proc,
    die,
    dump_data_text,
    log,
    parse_obstacles,
    run_crowd,
)

TEMPLATE = Path(__file__).with_name("replay.html")
OUT = ROOT / "results" / "replay.html"


class Track:
    def __init__(
        self,
        name: str,
        n_agents: int,
        step_ms: list[float],
        speed_mean: list[float],
        speed_max: list[float],
        xy: bytes,
        goal: bytes,
    ) -> None:
        self.name = name
        self.n_agents = n_agents
        self.step_ms = step_ms
        self.speed_mean = speed_mean
        self.speed_max = speed_max
        self.xy = xy
        self.goal = goal
        timed = step_ms[1:]
        self.n_steps = len(timed)
        self.avg_step_ms = sum(timed) / len(timed) if timed else 0.0
        self.min_step_ms = min(timed) if timed else 0.0
        self.max_step_ms = max(timed) if timed else 0.0
        self.total_ms = sum(timed)
        self.fps = (1000.0 / self.avg_step_ms) if self.avg_step_ms > 0 else 0.0

    def payload(self) -> dict:
        return {
            "name": self.name,
            "n_agents": self.n_agents,
            "n_steps": self.n_steps,
            "total_ms": round(self.total_ms, 4),
            "avg_step_ms": round(self.avg_step_ms, 4),
            "min_step_ms": round(self.min_step_ms, 4),
            "max_step_ms": round(self.max_step_ms, 4),
            "fps": round(self.fps, 2),
            "step_ms": self.step_ms,
            "xy": b64encode(self.xy).decode("ascii"),
            "goal": b64encode(self.goal).decode("ascii"),
            "speed_mean": self.speed_mean,
            "speed_max": self.speed_max,
        }


def rec_q(v: float) -> int:
    s = round(v * XY_SCALE)
    if s > 32767:
        return 32767
    if s < -32767:
        return -32767
    return int(s)


def load_rec(path: Path) -> tuple[Track, list[dict[str, float]]]:
    raw = path.read_bytes()
    if len(raw) < 16 or raw[:4] != REC_MAGIC:
        die("replay", f"{path.name}: not a rec file")
    n_obs, n_agents, n_frames = struct.unpack_from("<III", raw, 4)
    off = 16
    obs = []
    for _ in range(n_obs):
        x, y, r = struct.unpack_from("<fff", raw, off)
        obs.append({"x": x, "y": y, "radius": r})
        off += 12
    step_ms: list[float] = []
    speed_mean: list[float] = []
    speed_max: list[float] = []
    xy = bytearray()
    goal = bytearray()
    for _ in range(n_frames):
        step_ms_v, speed_mean_v, speed_max_v = struct.unpack_from("<fff", raw, off)
        off += 12
        step_ms.append(float(step_ms_v))
        speed_mean.append(round(float(speed_mean_v), 4))
        speed_max.append(round(float(speed_max_v), 4))
        nbytes = n_agents * 4
        xy += raw[off : off + nbytes]
        off += nbytes
        goal += raw[off : off + n_agents]
        off += n_agents
    if off != len(raw):
        die("replay", f"{path.name}: rec size mismatch")
    return Track(path.stem, n_agents, step_ms, speed_mean, speed_max, bytes(xy), bytes(goal)), obs


def load_dump(path: Path) -> tuple[Track, list[dict[str, float]]]:
    text = path.read_text()
    obs = parse_obstacles(text)
    body = dump_data_text(text)
    by_t: dict[int, list[tuple[int, float, float, float, float, int, float]]] = (
        defaultdict(list)
    )
    rows = csv.DictReader(body.splitlines(), skipinitialspace=True)
    names = [k.strip() for k in (rows.fieldnames or []) if k]
    missing = [c for c in DUMP_COLS if c not in names]
    if missing:
        die("replay", f"{path.name}: missing {','.join(missing)} (re-dump)")
    for row in rows:
        row = {k.strip(): (v or "").strip() for k, v in row.items() if k}
        t = int(row["t"])
        by_t[t].append(
            (
                int(row["id"]),
                float(row["x"]),
                float(row["y"]),
                float(row["vx"]),
                float(row["vy"]),
                int(row["goal"]),
                float(row["step_ms"]),
            )
        )
    if not by_t:
        die("replay", f"{path.name}: empty")
    times = sorted(by_t)
    xy = array.array("h")
    goal = array.array("B")
    step_ms, speed_mean, speed_max = [], [], []
    n = None
    for t in times:
        agents = sorted(by_t[t], key=lambda a: a[0])
        if n is None:
            n = len(agents)
        elif len(agents) != n:
            die("replay", f"{path.name}: t={t} agent count")
        speeds = [math.hypot(a[3], a[4]) for a in agents]
        for a in agents:
            xy.append(rec_q(a[1]))
            xy.append(rec_q(a[2]))
            goal.append(a[5] & 255)
        step_ms.append(agents[0][6])
        speed_mean.append(round(sum(speeds) / len(speeds), 4) if speeds else 0.0)
        speed_max.append(round(max(speeds), 4) if speeds else 0.0)
    return Track(path.stem, n or 0, step_ms, speed_mean, speed_max, xy.tobytes(), goal.tobytes()), obs


def load_any(path: Path) -> tuple[Track, list[dict[str, float]]]:
    with path.open("rb") as fp:
        magic = fp.read(4)
    if magic == REC_MAGIC:
        return load_rec(path)
    return load_dump(path)


def rec_to(path: Path, n: int, steps: int, mode: str) -> None:
    with path.open("wb") as out:
        proc = run_crowd(mode, "rec", n, steps, stdout=out, text=False)
    check_proc(proc, "rec", f"mode={mode}  N={n}  steps={steps}")


def write_html(
    tracks: list[Track], fit: float, obstacles: list[dict[str, float]]
) -> Path:
    if not TEMPLATE.is_file():
        die("replay", f"missing {TEMPLATE}")
    data = json.dumps(
        {
            "fit": fit,
            "world_size": WORLD_SIZE,
            "world_half": WORLD_HALF,
            "agent_radius": AGENT_RADIUS,
            "obstacles": obstacles,
            "tracks": [t.payload() for t in tracks],
        },
        separators=(",", ":"),
    ).replace("<", "\\u003c")
    html = TEMPLATE.read_text(encoding="utf-8").replace("__DATA__", data)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(html, encoding="utf-8")
    return OUT


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("dumps", nargs="*", type=Path, help="dump CSV or rec files")
    p.add_argument(
        "--run",
        nargs="+",
        metavar="MODE",
        help="record these modes then replay",
    )
    p.add_argument("-n", type=int, default=500)
    p.add_argument("-s", "--steps", type=int, default=5000)
    p.add_argument(
        "--fit",
        type=float,
        default=8.0,
        help="map slowest dump to this many seconds (0 = recorded 1:1)",
    )
    p.add_argument("--no-open", action="store_true", help="write HTML only")
    return p.parse_args()


def main() -> None:
    args = parse_args()
    paths: list[Path] = []
    tmpdir = None
    try:
        if args.run:
            tmpdir = tempfile.TemporaryDirectory(prefix="crowd-flow-replay-")
            root = Path(tmpdir.name)
            for mode in args.run:
                path = root / f"{mode}.rec"
                log("INFO", "replay", f"rec {mode}  N={args.n}  steps={args.steps}")
                rec_to(path, args.n, args.steps, mode)
                paths.append(path)
        for p in args.dumps:
            q = p if p.is_file() else ROOT / p
            if not q.is_file():
                die("replay", f"missing {p}")
            paths.append(q)
        if not paths:
            die("replay", "give CSV/rec paths or --run MODE...")
        tracks = []
        obstacles: list[dict[str, float]] = []
        for p in paths:
            track, obs = load_any(p)
            tracks.append(track)
            if obs and not obstacles:
                obstacles = obs
        out = write_html(tracks, args.fit, obstacles)
        log("OK", "replay", str(out.relative_to(ROOT)))
        if not args.no_open:
            webbrowser.open(out.as_uri())
    finally:
        if tmpdir:
            tmpdir.cleanup()


if __name__ == "__main__":
    main()
