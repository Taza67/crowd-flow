#!/usr/bin/env python3
"""Collect clips + sweeps to results/. Serve with --serve after copying results/."""

from __future__ import annotations

import argparse
import csv
import io
import os
import shutil
import threading
import webbrowser
from concurrent.futures import ProcessPoolExecutor, as_completed
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from cfutil import (
    BENCH_FIELDS,
    BLOCKS,
    CPU_BIN,
    DEFAULT_BLOCK,
    MODES,
    ROOT,
    SWEEP_N,
    SWEEP_STEPS,
    check_proc,
    die,
    ensure_bin,
    have_gpu,
    log,
    modes_for,
    run_crowd,
    skip_clip,
    skip_sweep,
)
from dashpack import (
    CLIP_BLOCK_NS,
    CLIP_DIR,
    CLIP_N,
    CLIP_STEPS,
    CPU_WORKERS,
    OUT,
    RESULTS,
    packet,
    write_dashboard,
)
from replay import rec_to_pack

_KIND = {
    "CPU_NAIVE": ("cpu_naive", "frame"),
    "CPU_OPT": ("cpu_opt", "frame"),
    "GPU_NAIVE_KERNEL": ("gpu_naive", "kernel"),
    "GPU_NAIVE_FRAME": ("gpu_naive", "frame"),
    "GPU_OPT_KERNEL": ("gpu_opt", "kernel"),
    "GPU_OPT_FRAME": ("gpu_opt", "frame"),
}


def _parse_rows(text: str, n: int, steps: int, block: int) -> list[dict]:
    raw = list(csv.DictReader(io.StringIO(",".join(BENCH_FIELDS) + "\n" + text)))
    out = []
    for r in raw:
        ver = (r.get("version") or "").strip()
        if ver not in _KIND:
            continue
        mode, kind = _KIND[ver]
        out.append(
            {
                "n": n,
                "steps": steps,
                "block": block,
                "mode": mode,
                "kind": kind,
                "avg_step_ms": float(r["avg_step_ms"]),
                "fps": float(r["fps"]),
                "total_ms": float(r["total_ms"]),
            }
        )
    return out


def _bench_one(mode: str, n: int, steps: int, block: int) -> list[dict]:
    if skip_sweep(n, mode, block):
        return []
    proc = run_crowd(
        mode, "bench", n, steps, capture_output=True, block=block
    )
    check_proc(proc, "bench", f"mode={mode}  N={n}  block={block}")
    return _parse_rows(proc.stdout, n, steps, block)


def _rec_one(mode: str, n: int, steps: int, block: int, path: str) -> str:
    rec_to_pack(Path(path), n, steps, mode, block)
    return path


def _cpu_count() -> int:
    n = os.cpu_count() or 1
    return max(1, min(CPU_WORKERS, n))


def _job(phase: str, i: int, n: int, extra: str) -> str:
    return f"{phase}  {i}/{n}  {extra}"


class _Prog:
    def __init__(self, total: int) -> None:
        self.total = total
        self.done = 0

    def start(self, phase: str, i: int, n: int, extra: str) -> None:
        log("INFO", "dashboard", f"{self.done + 1}/{self.total}  {_job(phase, i, n, extra)}")

    def finish(self, phase: str, i: int, n: int, extra: str) -> None:
        self.done += 1
        log("OK", "dashboard", f"{self.done}/{self.total}  {_job(phase, i, n, extra)}")


def _sweep_jobs(
    modes: tuple[str, ...],
) -> tuple[list[tuple[str, int, int, int]], list[tuple[str, int, int, int]]]:
    cpu: list[tuple[str, int, int, int]] = []
    gpu: list[tuple[str, int, int, int]] = []
    for n in SWEEP_N:
        for mode in modes:
            for block in BLOCKS:
                if skip_sweep(n, mode, block):
                    continue
                job = (mode, n, SWEEP_STEPS, block)
                (gpu if mode.startswith("gpu_") else cpu).append(job)
    return cpu, gpu


def _clip_jobs(
    modes: tuple[str, ...], dest_dir: Path
) -> tuple[
    list[tuple[str, int, int, int, str]],
    list[tuple[str, int, int, int, str]],
]:
    jobs: list[tuple[str, int, int, int, str]] = []
    for n in CLIP_N:
        for mode in modes:
            if skip_clip(n, mode):
                continue
            dest = dest_dir / f"{mode}_n{n}_b{DEFAULT_BLOCK}.rec.gz"
            jobs.append((mode, n, CLIP_STEPS, DEFAULT_BLOCK, str(dest)))
    if "gpu_opt" in modes:
        have = {(m, nn, b) for m, nn, _s, b, _p in jobs}
        for n in CLIP_BLOCK_NS:
            if skip_clip(n, "gpu_opt"):
                continue
            for block in BLOCKS:
                if ("gpu_opt", n, block) in have:
                    continue
                dest = dest_dir / f"gpu_opt_n{n}_b{block}.rec.gz"
                jobs.append(("gpu_opt", n, CLIP_STEPS, block, str(dest)))
    cpu = [j for j in jobs if not j[0].startswith("gpu_")]
    gpu = [j for j in jobs if j[0].startswith("gpu_")]
    return cpu, gpu


def _clips_from(packed: list[tuple[str, int, int, str]]) -> list[dict]:
    grouped: dict[tuple[int, int], list[tuple[str, str]]] = {}
    for mode, n, block, path in packed:
        rel = f"{CLIP_DIR.name}/{Path(path).name}"
        grouped.setdefault((n, block), []).append((mode, rel))
    clips = []
    for (n, block), rows in sorted(grouped.items()):
        rows.sort(key=lambda r: list(MODES).index(r[0]) if r[0] in MODES else 99)
        clips.append(
            {
                "n": n,
                "block": block,
                "tracks": [
                    {"name": mode, "file": rel, "xy_delta": True}
                    for mode, rel in rows
                ],
            }
        )
    return clips


def collect(modes: tuple[str, ...]) -> tuple[list[dict], list[dict]]:
    cpu_s, gpu_s = _sweep_jobs(modes)
    if CLIP_DIR.exists():
        shutil.rmtree(CLIP_DIR)
    CLIP_DIR.mkdir(parents=True)
    cpu_c, gpu_c = _clip_jobs(modes, CLIP_DIR)
    if not have_gpu():
        gpu_s, gpu_c = [], []
    if cpu_s or cpu_c:
        ensure_bin(CPU_BIN)
    log("INFO", "dashboard", f"sweep CPU  jobs={len(cpu_s)}")
    log("INFO", "dashboard", f"sweep GPU  jobs={len(gpu_s)}  serial device")
    log("INFO", "dashboard", f"clip CPU  jobs={len(cpu_c)}")
    log("INFO", "dashboard", f"clip GPU  jobs={len(gpu_c)}  serial device")
    prog = _Prog(len(cpu_s) + len(gpu_s) + len(cpu_c) + len(gpu_c))
    lock = threading.Lock()
    rows: list[dict] = []
    packed: list[tuple[str, int, int, str]] = []

    def gpu_phase() -> None:
        ns, nc = len(gpu_s), len(gpu_c)
        for i, (mode, n, steps, block) in enumerate(gpu_s, 1):
            extra = f"{mode}  N={n}  block={block}"
            with lock:
                prog.start("sweep GPU", i, ns, extra)
            part = _bench_one(mode, n, steps, block)
            with lock:
                rows.extend(part)
                prog.finish("sweep GPU", i, ns, extra)
        for i, (mode, n, steps, block, dest) in enumerate(gpu_c, 1):
            extra = f"{mode}  N={n}  block={block}"
            with lock:
                prog.start("clip GPU", i, nc, extra)
            rec = _rec_one(mode, n, steps, block, dest)
            with lock:
                packed.append((mode, n, block, rec))
                prog.finish("clip GPU", i, nc, extra)

    if cpu_s or cpu_c:
        with ProcessPoolExecutor(max_workers=_cpu_count()) as pool:
            futs_s = {
                pool.submit(_bench_one, mode, n, steps, block): (mode, n, block)
                for mode, n, steps, block in cpu_s
            }
            futs_c = {
                pool.submit(_rec_one, mode, n, steps, block, path): (mode, n, block)
                for mode, n, steps, block, path in cpu_c
            }
            ns, nc = len(cpu_s), len(cpu_c)
            done_s = [0]
            done_c = [0]

            def watch_sweep() -> None:
                for fut in as_completed(futs_s):
                    mode, n, block = futs_s[fut]
                    part = fut.result()
                    with lock:
                        rows.extend(part)
                        done_s[0] += 1
                        prog.finish(
                            "sweep CPU",
                            done_s[0],
                            ns,
                            f"{mode}  N={n}  block={block}",
                        )

            def watch_clip() -> None:
                for fut in as_completed(futs_c):
                    mode, n, block = futs_c[fut]
                    path = fut.result()
                    with lock:
                        packed.append((mode, n, block, path))
                        done_c[0] += 1
                        prog.finish(
                            "clip CPU",
                            done_c[0],
                            nc,
                            f"{mode}  N={n}  block={block}",
                        )

            watchers = [
                threading.Thread(target=watch_sweep),
                threading.Thread(target=watch_clip),
            ]
            for t in watchers:
                t.start()
            gpu_phase()
            for t in watchers:
                t.join()
    else:
        gpu_phase()
    return rows, _clips_from(packed)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cmd", nargs="?", choices=("cpu", "gpu"))
    p.add_argument("--fit", type=float, default=8.0)
    p.add_argument("--serve", action="store_true", help="serve results/ (local browser)")
    p.add_argument("--no-open", action="store_true")
    return p.parse_args()


def serve(open_browser: bool) -> None:
    if not OUT.is_file():
        die("dashboard", f"missing {OUT}  run make dashboard first")
    handler = partial(SimpleHTTPRequestHandler, directory=str(RESULTS))
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    url = f"http://127.0.0.1:{httpd.server_address[1]}/{OUT.name}"
    log("INFO", "dashboard", url)
    log("INFO", "dashboard", "Ctrl+C to stop")
    if open_browser:
        webbrowser.open(url)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print()


def main() -> None:
    args = parse_args()
    if args.serve:
        serve(not args.no_open)
        return
    modes = modes_for(args.cmd)
    sweeps, clips = collect(modes)
    if not clips and not sweeps:
        die("dashboard", "no clips or sweeps")
    out = write_dashboard(packet(clips, sweeps, fit=args.fit))
    log("OK", "dashboard", str(out.relative_to(ROOT)))


if __name__ == "__main__":
    main()
