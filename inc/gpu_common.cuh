/* gpu_common.cuh — CUDA checks and GPU benches. */

#ifndef GPU_COMMON_CUH
#define GPU_COMMON_CUH

#include "benchmark.h"
#include "utils.h"
#include <cuda_runtime.h>

#define CUDA_CHECK(err)                                                        \
  do {                                                                         \
    if ((err) != cudaSuccess) {                                                \
      cf_log(stderr, CF_ST_FAIL, "cuda", "%s  line=%d",                        \
             cudaGetErrorString(err), __LINE__);                               \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

static inline void gpu_require_device(void) {
  int n = 0;
  CUDA_CHECK(cudaGetDeviceCount(&n));
  if (n < 1) {
    cf_log(stderr, CF_ST_FAIL, "cuda", "no CUDA device");
    exit(EXIT_FAILURE);
  }
}

static inline int cf_cuda_block(void) {
  const char *e = getenv("CROWD_FLOW_BLOCK");
  int b = BLOCK_SIZE;
  if (e && e[0]) {
    char *end = NULL;
    long v = strtol(e, &end, 10);
    if (end == e || *end != '\0' || v < 32 || v > CUDA_MAX_BLOCK_THREADS ||
        (v & 31) != 0) {
      cf_log(stderr, CF_ST_FAIL, "cuda",
             "CROWD_FLOW_BLOCK must be 32..%d and a multiple of 32 (got '%s')",
             CUDA_MAX_BLOCK_THREADS, e);
      exit(EXIT_FAILURE);
    }
    b = (int)v;
  }
  return b;
}

#define CUDA_POST_KERNEL_CHECK()                                               \
  do {                                                                         \
    cudaError_t kerr = cudaGetLastError();                                     \
    if (kerr != cudaSuccess) {                                                 \
      cf_log(stderr, CF_ST_FAIL, "cuda", "%s  line=%d",                        \
             cudaGetErrorString(kerr), __LINE__);                              \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

static inline BenchResult gpu_bench_kernel(int n_agents, int n_steps,
                                           const char *version,
                                           void (*init_fn)(const Simulation *),
                                           void (*launch_fn)(void),
                                           void (*free_fn)(void)) {
  Simulation *s = simulation_create(n_agents, SIM_SEED);
  init_fn(s);

  BenchResult r;
  benchresult_init(&r, version, n_agents, n_steps);

  cudaEvent_t start, stop;
  cudaEventCreate(&start);
  cudaEventCreate(&stop);
  launch_fn();
  CUDA_CHECK(cudaDeviceSynchronize());

  double t0 = get_time_ms();
  for (int step = 0; step < n_steps; step++) {
    cudaEventRecord(start, 0);
    launch_fn();
    cudaEventRecord(stop, 0);
    cudaEventSynchronize(stop);
    float dt_ms;
    cudaEventElapsedTime(&dt_ms, start, stop);
    benchresult_note_step(&r, dt_ms);
  }
  benchresult_finish(&r, get_time_ms() - t0);

  cudaEventDestroy(start);
  cudaEventDestroy(stop);
  free_fn();
  free(s);
  return r;
}

static inline BenchResult gpu_bench_frame(int n_agents, int n_steps,
                                          const char *version,
                                          void (*init_fn)(const Simulation *),
                                          void (*step_fn)(Simulation *),
                                          void (*free_fn)(void)) {
  Simulation *s = simulation_create(n_agents, SIM_SEED);
  init_fn(s);

  BenchResult r;
  benchresult_init(&r, version, n_agents, n_steps);

  double t_start = get_time_ms();
  for (int step = 0; step < n_steps; step++) {
    double t0 = get_time_ms();
    step_fn(s);
    benchresult_note_step(&r, get_time_ms() - t0);
  }
  benchresult_finish(&r, get_time_ms() - t_start);

  free_fn();
  free(s);
  return r;
}

#endif
