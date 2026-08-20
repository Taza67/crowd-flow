#!/usr/bin/env python3
"""Dump tests: same-mode identity, cross-mode epsilon."""

from __future__ import annotations

import argparse
import csv
import math

from cfutil import (
    GPU_BIN,
    PHYS_COLS,
    STATE_COLS,
    WORLD_HALF,
    check_proc,
    die,
    dump_data_text,
    ensure_bin,
    log,
    run_crowd,
)


def dump_csv(n: int, steps: int, mode: str) -> str:
    proc = run_crowd(mode, "dump", n, steps, capture_output=True)
    check_proc(proc, "dump", f"mode={mode}  N={n}  steps={steps}")
    return proc.stdout


def rows(text: str) -> list[dict[str, str]]:
    out = []
    for row in csv.DictReader(dump_data_text(text).splitlines(), skipinitialspace=True):
        out.append({k.strip(): (v or "").strip() for k, v in row.items() if k})
    return out


def repro(mode: str, n: int, steps: int) -> None:
    a, b = rows(dump_csv(n, steps, mode)), rows(dump_csv(n, steps, mode))
    if not a or len(a) != len(b):
        die("repro", f"mode={mode}  N={n}  steps={steps}  row count mismatch")
    for x, y in zip(a, b):
        for k in PHYS_COLS:
            if x.get(k) != y.get(k):
                die("repro", f"mode={mode}  N={n}  steps={steps}  {k} mismatch")
    log("OK", "repro", f"mode={mode}  N={n}  steps={steps}")


def similar(
    mode_a: str,
    mode_b: str,
    n: int,
    steps: int,
    eps: float,
    world_half: float,
) -> None:
    a, b = rows(dump_csv(n, steps, mode_a)), rows(dump_csv(n, steps, mode_b))
    if not a or len(a) != len(b):
        die("similar", "row count mismatch or empty dump")
    max_d, goal_m = 0.0, 0
    for x, y in zip(a, b):
        if x["t"] != y["t"] or x["id"] != y["id"]:
            die("similar", "t/id mismatch")
        for k in STATE_COLS:
            u, v = float(x[k]), float(y[k])
            if not math.isfinite(u) or not math.isfinite(v):
                die("similar", "non-finite value")
            max_d = max(max_d, abs(u - v))
        if abs(float(x["x"])) > world_half or abs(float(x["y"])) > world_half:
            die("similar", "position outside world")
        if x["goal"] != y["goal"]:
            goal_m += 1
    extra = (
        f"{mode_a} vs {mode_b}  N={n}  steps={steps}  "
        f"max_delta={max_d:.6g}  EPS={eps:g}"
    )
    if max_d > eps:
        die("similar", extra)
    if goal_m:
        die("similar", f"{extra}  goal_mismatch={goal_m}")
    log("OK", "similar", extra)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cmd", nargs="?", choices=("cpu", "gpu", "repro", "similar"))
    p.add_argument("modes", nargs="*")
    p.add_argument("-n", type=int, default=200, help="agent count")
    p.add_argument("-s", "--steps", type=int, default=20)
    p.add_argument("--eps", type=float, default=1e-4)
    p.add_argument("--world-half", type=float, default=WORLD_HALF)
    return p.parse_args()


def suite_cpu(n: int, steps: int, eps: float, world_half: float) -> None:
    repro("cpu_naive", n, steps)
    repro("cpu_opt", n, steps)
    similar("cpu_naive", "cpu_opt", n, steps, eps, world_half)


def suite_gpu(n: int, steps: int, eps: float, world_half: float) -> None:
    if not ensure_bin(GPU_BIN):
        log("WARN", "gpu", "nvcc not found  skip", err=True)
        return
    repro("gpu_naive", n, steps)
    repro("gpu_opt", n, steps)
    similar("gpu_naive", "gpu_opt", n, steps, eps, world_half)


def main() -> None:
    args = parse_args()
    if args.cmd == "repro":
        mode = args.modes[0] if args.modes else "cpu_opt"
        repro(mode, args.n, args.steps)
    elif args.cmd == "similar":
        a = args.modes[0] if args.modes else "cpu_naive"
        b = args.modes[1] if len(args.modes) > 1 else "cpu_opt"
        similar(a, b, args.n, args.steps, args.eps, args.world_half)
    elif args.cmd == "cpu":
        suite_cpu(args.n, args.steps, args.eps, args.world_half)
    elif args.cmd == "gpu":
        suite_gpu(args.n, args.steps, args.eps, args.world_half)
    else:
        suite_cpu(args.n, args.steps, args.eps, args.world_half)
        suite_gpu(args.n, args.steps, args.eps, args.world_half)


if __name__ == "__main__":
    main()
