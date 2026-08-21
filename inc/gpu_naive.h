/* gpu_naive.h — O(N²) GPU step. Twin of gpu_opt.h. */

#ifndef GPU_NAIVE_H
#define GPU_NAIVE_H

#include "benchmark.h"

#ifdef __cplusplus
extern "C" {
#endif

void gpu_naive_init(const Simulation *s);
void gpu_naive_free(void);
void gpu_naive_run_step(Simulation *s);
void gpu_naive_launch_kernel_only(void);
BenchResult gpu_naive_bench_kernel(int n_agents, int n_steps);
BenchResult gpu_naive_bench_frame(int n_agents, int n_steps);

#ifdef __cplusplus
}
#endif

#endif
