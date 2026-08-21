/* gpu_opt.h — SoA + grid GPU step. Twin of gpu_naive.h. */

#ifndef GPU_OPT_H
#define GPU_OPT_H

#include "benchmark.h"

#ifdef __cplusplus
extern "C" {
#endif

void gpu_opt_init(const Simulation *s);
void gpu_opt_free(void);
void gpu_opt_run_step(Simulation *s);
void gpu_opt_launch_kernel_only(void);
BenchResult gpu_opt_bench_kernel(int n_agents, int n_steps);
BenchResult gpu_opt_bench_frame(int n_agents, int n_steps);

#ifdef __cplusplus
}
#endif

#endif
