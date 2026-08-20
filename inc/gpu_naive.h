/* gpu_naive.h — O(N²) GPU step. Twin of gpu_opt.h. */

#ifndef GPU_NAIVE_H
#define GPU_NAIVE_H

#include "benchmark.h"
#include "obstacle.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float px, py;
  float vx, vy;
  int goal_idx;
} DevAgent;

typedef Obstacle DevObstacle;

typedef struct {
  float x, y;
} DevGoal;

typedef struct {
  DevAgent *d[2];
  DevObstacle *d_obs;
  DevGoal *d_goals;
  DevAgent *h_buf;
  int buf;
  int n_agents, n_obstacles, n_goals;
} GpuNaiveState;

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
