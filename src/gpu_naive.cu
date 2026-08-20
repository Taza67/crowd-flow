/* gpu_naive.cu — O(N²) neighbour scan. Twin of gpu_opt.cu. */

#include "gpu_common.cuh"
#include "gpu_naive.h"
#include "steering.h"
#include <cuda_runtime.h>

static GpuNaiveState gns;

static void gpu_naive_copy_to_host(Simulation *s);
static void gpu_naive_upload_from_host(const Simulation *s);

__global__ void boids_naive_kernel(const DevAgent *__restrict__ in,
                                   DevAgent *__restrict__ out,
                                   const DevObstacle *__restrict__ obs,
                                   const DevGoal *__restrict__ goals, int N,
                                   int n_obs, int n_goals) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= N)
    return;

  float ax = in[i].px, ay = in[i].py, avx = in[i].vx, avy = in[i].vy;
  int gi = steering_clamp_goal(in[i].goal_idx, n_goals);
  float sep_x = 0, sep_y = 0, aln_x = 0, aln_y = 0, coh_x = 0, coh_y = 0;
  int n_nbr = 0, n_sep = 0;
  const float R2 = PERCEPTION_RADIUS * PERCEPTION_RADIUS;
  const float S2 = SEPARATION_DIST * SEPARATION_DIST;

  for (int j = 0; j < N; j++) {
    if (j == i)
      continue;
    steering_accum_neighbour(ax, ay, in[j].px, in[j].py, in[j].vx, in[j].vy, R2,
                             S2, &sep_x, &sep_y, &n_sep, &aln_x, &aln_y, &coh_x,
                             &coh_y, &n_nbr);
  }

  Vec2 flock = steering_flock(sep_x, sep_y, n_sep, aln_x, aln_y, coh_x, coh_y,
                              n_nbr, ax, ay, avx, avy);
  Vec2 acc =
      steering_forces(flock, ax, ay, goals[gi].x, goals[gi].y, obs, n_obs);

  float npx, npy, nvx, nvy;
  steering_integrate(ax, ay, avx, avy, acc, &npx, &npy, &nvx, &nvy);
  out[i] = (DevAgent){npx, npy, nvx, nvy, gi};
}

void gpu_naive_free(void);

void gpu_naive_init(const Simulation *s) {
  gpu_require_device();
  gpu_naive_free();

  gns.n_agents = s->n_agents;
  gns.n_obstacles = s->n_obstacles;
  gns.n_goals = s->n_goals;
  gns.buf = 0;

  size_t agents_sz = (size_t)gns.n_agents * sizeof(DevAgent);
  CUDA_CHECK(cudaMalloc(&gns.d[0], agents_sz));
  CUDA_CHECK(cudaMalloc(&gns.d[1], agents_sz));
  CUDA_CHECK(
      cudaMalloc(&gns.d_obs, (size_t)gns.n_obstacles * sizeof(DevObstacle)));
  CUDA_CHECK(cudaMalloc(&gns.d_goals, (size_t)gns.n_goals * sizeof(DevGoal)));
  CUDA_CHECK(cudaMallocHost(&gns.h_buf, agents_sz));

  DevObstacle *h_obs =
      (DevObstacle *)malloc((size_t)gns.n_obstacles * sizeof(DevObstacle));
  DevGoal *h_goals = (DevGoal *)malloc((size_t)gns.n_goals * sizeof(DevGoal));
  if (!h_obs || !h_goals)
    cf_die("gpu_naive_init: out of memory");
  for (int o = 0; o < gns.n_obstacles; o++)
    h_obs[o] = (DevObstacle){s->obstacles[o].x, s->obstacles[o].y,
                             s->obstacles[o].radius};
  for (int g = 0; g < gns.n_goals; g++)
    h_goals[g] = (DevGoal){s->goals[g].x, s->goals[g].y};
  CUDA_CHECK(cudaMemcpy(gns.d_obs, h_obs,
                        (size_t)gns.n_obstacles * sizeof(DevObstacle),
                        cudaMemcpyHostToDevice));
  CUDA_CHECK(cudaMemcpy(gns.d_goals, h_goals,
                        (size_t)gns.n_goals * sizeof(DevGoal),
                        cudaMemcpyHostToDevice));
  free(h_obs);
  free(h_goals);

  gpu_naive_upload_from_host(s);
}

void gpu_naive_free(void) {
  cudaFree(gns.d[0]);
  cudaFree(gns.d[1]);
  cudaFree(gns.d_obs);
  cudaFree(gns.d_goals);
  cudaFreeHost(gns.h_buf);
  memset(&gns, 0, sizeof(gns));
}

void gpu_naive_run_step(Simulation *s) {
  gpu_naive_launch_kernel_only();
  CUDA_CHECK(cudaDeviceSynchronize());
  gpu_naive_copy_to_host(s);
  simulation_finish_step(s);
  gpu_naive_upload_from_host(s);
}

void gpu_naive_launch_kernel_only(void) {
  int N = gns.n_agents;
  int r = gns.buf;
  int w = 1 - r;
  int nblocks = (N + BLOCK_SIZE - 1) / BLOCK_SIZE;
  boids_naive_kernel<<<nblocks, BLOCK_SIZE>>>(gns.d[r], gns.d[w], gns.d_obs,
                                              gns.d_goals, N, gns.n_obstacles,
                                              gns.n_goals);
  CUDA_POST_KERNEL_CHECK();
  gns.buf = w;
}

static void gpu_naive_copy_to_host(Simulation *s) {
  int N = gns.n_agents;
  int b = gns.buf;
  CUDA_CHECK(cudaMemcpy(gns.h_buf, gns.d[b], (size_t)N * sizeof(DevAgent),
                        cudaMemcpyDeviceToHost));
  for (int i = 0; i < N; i++) {
    s->agents[i].pos = (Vec2){gns.h_buf[i].px, gns.h_buf[i].py};
    s->agents[i].vel = (Vec2){gns.h_buf[i].vx, gns.h_buf[i].vy};
    s->agents[i].goal_idx = gns.h_buf[i].goal_idx;
  }
}

static void gpu_naive_upload_from_host(const Simulation *s) {
  int N = gns.n_agents;
  int b = gns.buf;
  for (int i = 0; i < N; i++)
    gns.h_buf[i] =
        (DevAgent){s->agents[i].pos.x, s->agents[i].pos.y, s->agents[i].vel.x,
                   s->agents[i].vel.y, s->agents[i].goal_idx};
  CUDA_CHECK(cudaMemcpy(gns.d[b], gns.h_buf, (size_t)N * sizeof(DevAgent),
                        cudaMemcpyHostToDevice));
}

BenchResult gpu_naive_bench_kernel(int n_agents, int n_steps) {
  return gpu_bench_kernel(n_agents, n_steps, "GPU_NAIVE_KERNEL", gpu_naive_init,
                          gpu_naive_launch_kernel_only, gpu_naive_free);
}

BenchResult gpu_naive_bench_frame(int n_agents, int n_steps) {
  return gpu_bench_frame(n_agents, n_steps, "GPU_NAIVE_FRAME", gpu_naive_init,
                         gpu_naive_run_step, gpu_naive_free);
}
