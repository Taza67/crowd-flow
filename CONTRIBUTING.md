# Contributing

Thank you for your interest in **crowd-flow**. The project is a crowd simulator based on Reynolds' boids, written in C and CUDA, built with `make` (C11 standard).

## Before you start

- Search the [existing issues](https://github.com/Taza67/crowd-flow/issues) to avoid duplicating work.
- For large or structural changes, open an issue first.

## Development environment

### Prerequisites

- `gcc` (see `Makefile`: options `-O3 -march=native -std=c11 -Wall -Wextra -I inc -D_GNU_SOURCE`)
- `make`
- `python3`
- `nvcc` for the GPU binary

### Clone and build

```bash
git clone https://github.com/Taza67/crowd-flow.git
cd crowd-flow
make
```

## Pull requests

1. Fork the repository and create a branch from `main`.
2. Make focused changes; keep the PRs easy to review.
3. Make sure `make` compiles without error or warning, then `make test`.
4. Open a pull request with a clear description and link the related issues.

## Commit messages

Follow [Conventional Commits](https://www.conventionalcommits.org/).

- **Types:** `feat`, `fix`, `refactor`, `docs`, `test`, `chore`
- **Description:** imperative, lowercase, no trailing period
- **Body:** optional; blank line after the description, then `-` bullets — lowercase except proper nouns, imperative, no trailing period

## Code organization

| File | Role |
|------|------|
| `src/main.c` | Entry point, dispatch of bench, dump and rec |
| `src/simulation.c` | World spawn and finish of each step |
| `src/cpu_naive.c` | CPU all-pairs neighbour scan |
| `src/cpu_opt.c` | CPU spatial grid |
| `src/gpu_naive.cu` | GPU all-pairs neighbour scan |
| `src/gpu_opt.cu` | GPU spatial grid |
| `src/constraints.c` | Hard contacts after steering |
| `src/dump.c` | CSV dump and binary rec |
| `src/benchmark.c` | Timed run of one backend |
| `inc/steering.h` | Flock, goal, obstacles, Euler |
| `scripts/test.py` | Repro and similar dump tests |
| `scripts/bench.py` | Bench sweeps |
| `scripts/replay.py` | Browser replay |

## Code of conduct

This project follows the [Contributor Covenant](CODE_OF_CONDUCT.md). By participating, you agree to abide by it.
