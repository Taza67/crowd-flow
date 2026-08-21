<a id="readme-top"></a>

[![Contributors][contributors-shield]][contributors-url]
[![Issues][issues-shield]][issues-url]
[![License][license-shield]][license-url]
[![C][c-shield]][c-url]
[![CUDA][cuda-shield]][cuda-url]

<div align="center">

<h3 align="center">crowd-flow</h3>

  <p align="center">
    Crowd simulator based on Reynolds' boids, written in C and CUDA. Flocking, goals and contacts on the GPU, with a CPU build for comparison.
    <br />
    <br />
    <a href="https://github.com/Taza67/crowd-flow/issues/new?labels=bug">Report Bug</a>
    &middot;
    <a href="https://github.com/Taza67/crowd-flow/issues/new?labels=enhancement">Request Feature</a>
  </p>
</div>

<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
      <ul>
        <li><a href="#built-with">Built With</a></li>
      </ul>
    </li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#installation">Installation</a></li>
      </ul>
    </li>
    <li><a href="#usage">Usage</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
  </ol>
</details>

## About The Project

`crowd-flow` is a crowd simulator based on Reynolds' boids, written in C and CUDA. It runs neighbourhood steering, goal following and contact resolution on the GPU. A CPU build of the same model is available for comparison.

### Built With

* [C](https://fr.wikipedia.org/wiki/C_(langage)) C11
* [Make](https://fr.wikipedia.org/wiki/Make)
* [CUDA](https://developer.nvidia.com/cuda-toolkit) (`nvcc`, GPU binary only)

<p align="right"><a href="#readme-top" title="Back to top">↑</a></p>

## Getting Started

### Prerequisites

* `gcc`
* `make`
* `python3`
* `nvcc` for `make gpu`

### Installation

```bash
git clone https://github.com/Taza67/crowd-flow.git
cd crowd-flow
make
```

`make` builds `build/crowd-sim-cpu`. It also builds `build/crowd-sim-gpu` when `nvcc` is available; otherwise it prints `WARN` and skips.

```bash
make cpu
make gpu
make help
```

<p align="right"><a href="#readme-top" title="Back to top">↑</a></p>

## Usage

Display the help notice:

```sh
./build/crowd-sim-cpu
./build/crowd-sim-gpu
```

Dump agent trajectories as CSV on stdout. Comment lines `#obstacle,x,y,radius` list the circular obstacles, then `t,id,x,y,vx,vy,goal,step_ms`. `step_ms` is wall time of that step (`0` at `t=0`). Meta goes to stderr. Defaults: 200 agents, 20 steps, `cpu_opt`. Same-mode dumps match on positions, not on `step_ms`.

```sh
./build/crowd-sim-cpu dump > traj.csv
./build/crowd-sim-gpu dump > traj.csv
```

Short dump (50 agents, 5 steps, `cpu_opt`):

```sh
make dump
```

Run the tests (CPU then GPU). GPU suites print `WARN` and skip if `nvcc` is missing. `make test-gpu` uses Slurm like the other GPU Make targets when the controller answers; otherwise it warns and runs locally. Same seed: bit-identical dumps for one mode twice; naive vs opt within `EPS` (default `1e-4`).

```sh
make test
```

CPU suite or GPU suite only:

```sh
make test-cpu
make test-gpu
```

One-mode identity, or naive vs opt within `EPS`:

```sh
python3 scripts/test.py repro cpu_naive
python3 scripts/test.py similar cpu_naive cpu_opt
```

Replay existing dumps in the browser (Canvas). Independent clocks: each pane advances when that backend’s recorded `step_ms` elapses. Writes `results/replay.html`. Default `--fit 8` maps the slowest dump to 8 seconds. `--fit 0` plays at recorded 1:1.

```sh
./build/crowd-sim-cpu dump 500 5000 cpu_naive > cpu_naive.csv
./build/crowd-sim-cpu dump 500 5000 cpu_opt > cpu_opt.csv
./build/crowd-sim-gpu dump 500 5000 gpu_naive > gpu_naive.csv
./build/crowd-sim-gpu dump 500 5000 gpu_opt > gpu_opt.csv
python3 scripts/replay.py cpu_naive.csv cpu_opt.csv gpu_naive.csv gpu_opt.csv
```

Record then replay (CPU then GPU). GPU suites print `WARN` and skip if `nvcc` is missing. Default `--fit 8` maps the slowest dump to 8 seconds. `--fit 0` plays at recorded 1:1.

```sh
make replay
```

CPU suite or GPU suite only:

```sh
make replay-cpu
make replay-gpu
```

Time one backend (CSV on stdout). GPU modes emit `KERNEL` then `FRAME`.

```sh
./build/crowd-sim-cpu bench 100 100 cpu_opt
./build/crowd-sim-gpu bench 100 100 gpu_opt
```

Quick table (CPU then GPU, N=100, steps=100). GPU suites print `WARN` and skip if `nvcc` is missing. Naive modes skip when N > 20000. Sweeps write `results/bench_results.csv`.

```sh
make bench
```

CPU suite or GPU suite only:

```sh
make bench-cpu
make bench-gpu
```

Full sweep (CPU then GPU):

```sh
make run-bench
```

CPU suite or GPU suite only:

```sh
make run-bench-cpu
make run-bench-gpu
```

<p align="right"><a href="#readme-top" title="Back to top">↑</a></p>

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Please read [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) before participating.

<p align="right"><a href="#readme-top" title="Back to top">↑</a></p>

## License

Distributed under the MIT License. See [LICENSE](LICENSE) for more information.

<p align="right"><a href="#readme-top" title="Back to top">↑</a></p>

## Contact

Taza67 - [tazaakil67@gmail.com](mailto:tazaakil67@gmail.com)

Project link : [https://github.com/Taza67/crowd-flow](https://github.com/Taza67/crowd-flow)

<p align="right"><a href="#readme-top" title="Back to top">↑</a></p>

<!-- MARKDOWN LINKS & IMAGES -->
[contributors-shield]: https://img.shields.io/github/contributors/Taza67/crowd-flow.svg
[contributors-url]: https://github.com/Taza67/crowd-flow/graphs/contributors
[issues-shield]: https://img.shields.io/github/issues/Taza67/crowd-flow.svg
[issues-url]: https://github.com/Taza67/crowd-flow/issues
[license-shield]: https://img.shields.io/badge/License-MIT-blue.svg
[license-url]: https://github.com/Taza67/crowd-flow/blob/main/LICENSE
[c-shield]: https://img.shields.io/badge/C11-00599C.svg?logo=c&logoColor=white
[c-url]: https://fr.wikipedia.org/wiki/C_(langage)
[cuda-shield]: https://img.shields.io/badge/CUDA-76B900.svg?logo=nvidia&logoColor=white
[cuda-url]: https://developer.nvidia.com/cuda-toolkit
