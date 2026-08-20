/* gpu_opt.h — SoA + grid GPU step. Twin of gpu_naive.h. */

#ifndef GPU_OPT_H
#define GPU_OPT_H

#include "benchmark.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float *d_px[2], *d_py[2], *d_vx[2], *d_vy[2];
  int *d_gi[2];
  int *d_cell_ids;
  int *d_cell_count, *d_cell_start, *d_cell_fill;
  int *d_sorted_agents;
  float *d_obs_x, *d_obs_y, *d_obs_r;
  float *d_gx, *d_gy;
  float *h_px, *h_py, *h_vx, *h_vy;
  int *h_gi;
  int buf;
  int n_agents, n_obstacles, n_goals;
} GpuOptState;

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
