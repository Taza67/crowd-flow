#!/usr/bin/env python3
"""Sweep bench: call the binary once per (N, mode), table + CSV."""

from __future__ import annotations

import argparse
import csv
import io

from cfutil import CPU_MODES, GPU_MODES, ROOT, check_proc, have_gpu, run_crowd

RESULTS = ROOT / "results" / "bench_results.csv"
SWEEP = (100, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000)
NAIVE_CAP = 20000
FIELDS = (
    "version",
    "n_agents",
    "n_steps",
    "total_ms",
    "avg_step_ms",
    "min_step_ms",
    "max_step_ms",
    "fps",
)
ROW = "%-20s %8s %8s %10s %10s %10s %10s %10s"
RULE = ROW % (("-" * 20,) + ("-" * 8,) * 2 + ("-" * 10,) * 5)


def run_one(n: int, steps: int, mode: str) -> list[dict[str, str]]:
    if "naive" in mode and n > NAIVE_CAP:
        label = "GPU_NAIVE" if mode.startswith("gpu") else "CPU_NAIVE"
        print(ROW % (label, n, steps, "skip", "", "", "", ""), flush=True)
        return []
    proc = run_crowd(mode, "bench", n, steps, capture_output=True)
    check_proc(proc, "bench", f"mode={mode}  N={n}  steps={steps}")
    text = ",".join(FIELDS) + "\n" + proc.stdout
    return list(csv.DictReader(io.StringIO(text)))


def fmt_row(r: dict[str, str]) -> str:
    return ROW % (
        r["version"],
        r["n_agents"],
        r["n_steps"],
        f"{float(r['total_ms']):.2f}",
        f"{float(r['avg_step_ms']):.2f}",
        f"{float(r['min_step_ms']):.2f}",
        f"{float(r['max_step_ms']):.2f}",
        f"{float(r['fps']):.2f}",
    )


def append_csv(rows: list[dict[str, str]]) -> None:
    RESULTS.parent.mkdir(exist_ok=True)
    new = not RESULTS.is_file() or RESULTS.stat().st_size == 0
    with RESULTS.open("a", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        if new:
            w.writeheader()
        w.writerows(rows)


def suite_cpu(ns: tuple[int, ...], steps: int) -> None:
    _suite(CPU_MODES, ns, steps)


def suite_gpu(ns: tuple[int, ...], steps: int) -> None:
    if not have_gpu():
        return
    _suite(GPU_MODES, ns, steps)


def _suite(modes: tuple[str, ...], ns: tuple[int, ...], steps: int) -> None:
    print(flush=True)
    print(ROW % ("version", "N", "steps", "total_ms", "avg_step_ms", "min_step_ms",
                 "max_step_ms", "fps"), flush=True)
    print(RULE, flush=True)
    n_out = 0
    for n in ns:
        for mode in modes:
            rows = run_one(n, steps, mode)
            if not rows:
                continue
            append_csv(rows)
            for r in rows:
                print(fmt_row(r), flush=True)
            n_out += len(rows)
    if n_out:
        print(RULE, flush=True)
        print("wrote  results/bench_results.csv", flush=True)
    print(flush=True)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cmd", nargs="?", choices=("cpu", "gpu"))
    p.add_argument("-n", type=int, default=0, help="agent count; 0 = sweep")
    p.add_argument("-s", "--steps", type=int, default=100)
    return p.parse_args()


def main() -> None:
    args = parse_args()
    ns = SWEEP if args.n <= 0 else (args.n,)
    if args.cmd == "cpu":
        suite_cpu(ns, args.steps)
    elif args.cmd == "gpu":
        suite_gpu(ns, args.steps)
    else:
        suite_cpu(ns, args.steps)
        suite_gpu(ns, args.steps)


if __name__ == "__main__":
    main()
