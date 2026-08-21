# Makefile — CPU / GPU / bench / dump / rec / replay / test

CC        := gcc
NVCC      := nvcc
CFLAGS    := -O3 -march=native -std=c11 -Wall -Wextra -I inc -D_GNU_SOURCE
NVCCFLAGS := -O3 -I inc --compiler-options "-O3 -Wall"
LDFLAGS   := -lm
SRUN      := srun --gres=shard:1 --cpus-per-task=4 --mem=2GB
export CROWD_FLOW_SRUN := $(SRUN)
HAS_NVCC  := $(shell command -v $(NVCC) >/dev/null 2>&1 && echo 1)
SRUN_BIN  := $(shell command -v $(firstword $(SRUN)) 2>/dev/null)
HAS_SLURM := $(shell scontrol ping 2>/dev/null | grep -qi up && echo 1)
ifeq ($(HAS_SLURM),1)
GPU_LAUNCH := $(SRUN)
else
GPU_LAUNCH :=
endif
WARN_SRUN := $(if $(HAS_SLURM),,$(if $(SRUN_BIN),1))
BLANK     := printf '\n'

.SILENT:

BUILD   := build
SRC     := src
INC     := $(wildcard inc/*.h inc/*.cuh)

CPU_SRCS := $(SRC)/main.c $(SRC)/simulation.c $(SRC)/benchmark.c \
            $(SRC)/cpu_naive.c $(SRC)/cpu_opt.c $(SRC)/constraints.c \
            $(SRC)/dump.c
GPU_C_OBJS := $(patsubst $(SRC)/%.c,$(BUILD)/%_gpu.o,$(CPU_SRCS))
GPU_CU_OBJS := $(BUILD)/gpu_naive.o $(BUILD)/gpu_opt.o

CPU_BIN := $(BUILD)/crowd-sim-cpu
GPU_BIN := $(BUILD)/crowd-sim-gpu
GPU_DEPS := $(if $(HAS_NVCC),$(GPU_BIN),skip-gpu)

.PHONY: all help clean cpu gpu bench bench-cpu bench-gpu run-bench \
        run-bench-cpu run-bench-gpu dump replay replay-cpu replay-gpu \
        test test-cpu test-gpu skip-gpu gpu-launch-warn

all: cpu gpu

help:
	$(BLANK)
	printf '  %-16s  %s\n' make 'CPU + GPU (WARN if no nvcc)'
	printf '  %-16s  %s\n' cpu 'CPU binary'
	printf '  %-16s  %s\n' gpu 'GPU binary'
	printf '  %-16s  %s\n' bench 'CPU then GPU  N=100  steps=100'
	printf '  %-16s  %s\n' bench-cpu 'CPU  N=100  steps=100'
	printf '  %-16s  %s\n' bench-gpu 'GPU  N=100  steps=100'
	printf '  %-16s  %s\n' run-bench 'CPU then GPU sweep'
	printf '  %-16s  %s\n' run-bench-cpu 'CPU sweep'
	printf '  %-16s  %s\n' run-bench-gpu 'GPU sweep'
	printf '  %-16s  %s\n' dump 'CSV stdout  N=50  steps=5  cpu_opt'
	printf '  %-16s  %s\n' replay 'CPU then GPU'
	printf '  %-16s  %s\n' replay-cpu 'CPU'
	printf '  %-16s  %s\n' replay-gpu 'GPU'
	printf '  %-16s  %s\n' test 'CPU then GPU'
	printf '  %-16s  %s\n' test-cpu 'CPU'
	printf '  %-16s  %s\n' test-gpu 'GPU'
	printf '  %-16s  %s\n' clean 'remove build/'
	$(BLANK)

$(BUILD):
	mkdir -p $(BUILD)

$(CPU_BIN): $(CPU_SRCS) $(INC) | $(BUILD)
	printf '%-4s  %-12s  %s\n' INFO build 'CPU...'
	$(CC) $(CFLAGS) $(CPU_SRCS) -o $@ $(LDFLAGS)
	printf '%-4s  %-12s  %s\n' OK build '$(CPU_BIN)'

$(BUILD)/%_gpu.o: $(SRC)/%.c $(INC) | $(BUILD)
	$(CC) $(CFLAGS) -DWITH_GPU -c $< -o $@

$(BUILD)/gpu_%.o: $(SRC)/gpu_%.cu $(INC) | $(BUILD)
	$(NVCC) $(NVCCFLAGS) -DWITH_GPU -c $< -o $@

$(GPU_BIN): $(GPU_C_OBJS) $(GPU_CU_OBJS)
	printf '%-4s  %-12s  %s\n' INFO build 'GPU (CUDA)...'
	$(NVCC) $(NVCCFLAGS) $(GPU_C_OBJS) $(GPU_CU_OBJS) -o $@ $(LDFLAGS)
	printf '%-4s  %-12s  %s\n' OK build '$(GPU_BIN)'

cpu: $(CPU_BIN)

skip-gpu:
	printf '%-4s  %-12s  %s\n' WARN gpu 'nvcc not found  skip'

gpu: $(GPU_DEPS)

gpu-launch-warn:
ifeq ($(WARN_SRUN),1)
	printf '%-4s  %-12s  %s\n' WARN gpu 'srun unavailable  running locally'
endif

bench-cpu: $(CPU_BIN)
	printf '%-4s  %-12s  %s\n' INFO bench 'CPU  N=100  steps=100'
	python3 scripts/bench.py cpu -n 100 -s 100

bench-gpu: gpu-launch-warn
	printf '%-4s  %-12s  %s\n' INFO bench 'GPU  N=100  steps=100'
	$(GPU_LAUNCH) python3 scripts/bench.py gpu -n 100 -s 100

bench: bench-cpu bench-gpu

run-bench-cpu: $(CPU_BIN)
	printf '%-4s  %-12s  %s\n' INFO bench 'CPU sweep'
	python3 scripts/bench.py cpu

run-bench-gpu: gpu-launch-warn
	printf '%-4s  %-12s  %s\n' INFO bench 'GPU sweep'
	$(GPU_LAUNCH) python3 scripts/bench.py gpu

run-bench: run-bench-cpu run-bench-gpu

dump: $(CPU_BIN)
	printf '%-4s  %-12s  %s\n' INFO dump 'N=50  steps=5  mode=cpu_opt'
	$(BLANK)
	./$(CPU_BIN) dump 50 5 cpu_opt
	$(BLANK)

replay-cpu: $(CPU_BIN)
	printf '%-4s  %-12s  %s\n' INFO replay 'CPU'
	python3 scripts/replay.py cpu

replay-gpu: gpu-launch-warn
	printf '%-4s  %-12s  %s\n' INFO replay 'GPU'
	$(GPU_LAUNCH) python3 scripts/replay.py gpu

replay: $(CPU_BIN) gpu-launch-warn
	printf '%-4s  %-12s  %s\n' INFO replay 'CPU then GPU'
	$(GPU_LAUNCH) python3 scripts/replay.py

test-cpu: $(CPU_BIN)
	printf '%-4s  %-12s  %s\n' INFO test 'CPU'
	python3 scripts/test.py cpu

test-gpu: gpu-launch-warn
	printf '%-4s  %-12s  %s\n' INFO test 'GPU'
	$(GPU_LAUNCH) python3 scripts/test.py gpu

test: test-cpu test-gpu

clean:
	rm -rf $(BUILD)
	printf '%-4s  %-12s  %s\n' OK clean '$(BUILD)'
