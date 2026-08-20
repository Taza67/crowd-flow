/* benchmark.c — time one mode. GPU emits KERNEL then FRAME. */

#include "benchmark.h"
#include "cpu_naive.h"
#include "cpu_opt.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WITH_GPU
#include "gpu_naive.h"
#include "gpu_opt.h"
#endif

BenchResult run_bench_generic(int n_agents, int n_steps, const char *version,
                              void (*step_func)(Simulation *)) {
  Simulation *s = simulation_create(n_agents, SIM_SEED);

  BenchResult r;
  benchresult_init(&r, version, n_agents, n_steps);

  double t_start = get_time_ms();
  for (int step = 0; step < n_steps; step++) {
    double t0 = get_time_ms();
    step_func(s);
    benchresult_note_step(&r, get_time_ms() - t0);
  }
  benchresult_finish(&r, get_time_ms() - t_start);
  free(s);
  return r;
}

static void emit(BenchResult r) { benchresult_print(&r); }

int benchmark_run(int n_agents, int n_steps, const char *mode) {
  if (n_agents <= 0 || n_agents > MAX_AGENTS || n_steps < 0) {
    cf_log(stderr, CF_ST_FAIL, "bench", "n_agents / n_steps out of range");
    exit(EXIT_FAILURE);
  }
  if (!mode)
    mode = CPU_OPT;

  if (strcmp(mode, CPU_NAIVE) == 0) {
    emit(run_bench_generic(n_agents, n_steps, "CPU_NAIVE", cpu_naive_run_step));
  } else if (strcmp(mode, CPU_OPT) == 0) {
    emit(run_bench_generic(n_agents, n_steps, "CPU_OPT", cpu_opt_run_step));
#ifdef WITH_GPU
  } else if (strcmp(mode, GPU_NAIVE) == 0) {
    emit(gpu_naive_bench_kernel(n_agents, n_steps));
    emit(gpu_naive_bench_frame(n_agents, n_steps));
  } else if (strcmp(mode, GPU_OPT) == 0) {
    emit(gpu_opt_bench_kernel(n_agents, n_steps));
    emit(gpu_opt_bench_frame(n_agents, n_steps));
#endif
  } else if (strcmp(mode, GPU_NAIVE) == 0 || strcmp(mode, GPU_OPT) == 0) {
    cf_log(stderr, CF_ST_WARN, "bench", "GPU not compiled");
    return EXIT_FAILURE;
  } else {
    cf_log(stderr, CF_ST_FAIL, "bench",
           "unknown mode (cpu_naive cpu_opt gpu_naive gpu_opt)");
    exit(EXIT_FAILURE);
  }
  return EXIT_SUCCESS;
}
