/* gpu_opt.cu — SoA + counting-sort grid. Twin of gpu_naive.cu. */

#include "gpu_common.cuh"
#include "gpu_opt.h"
#include "steering.h"
#include <cuda_runtime.h>

static GpuOptState gos;

static void gpu_opt_copy_to_host(Simulation *s);
static void gpu_opt_upload_from_host(const Simulation *s);

__global__ void assign_cells_kernel(const float *__restrict__ px,
                                    const float *__restrict__ py,
                                    int *__restrict__ cell_ids, int N) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= N)
    return;
  cell_ids[i] = steering_grid_cell(px[i], py[i]);
}

__global__ void count_cells_kernel(const int *__restrict__ cell_ids,
                                   int *__restrict__ cell_count, int N) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= N)
    return;
  atomicAdd(&cell_count[cell_ids[i]], 1);
}

__global__ void exclusive_scan_kernel(const int *__restrict__ in,
                                      int *__restrict__ out, int n) {
  extern __shared__ int smem[];
  int tid = threadIdx.x;
  smem[tid] = (tid < n) ? in[tid] : 0;
  __syncthreads();
  for (int d = 1; d < blockDim.x; d <<= 1) {
    int v = (tid >= d) ? smem[tid - d] : 0;
    __syncthreads();
    smem[tid] += v;
    __syncthreads();
  }
  int prev = (tid > 0) ? smem[tid - 1] : 0;
  __syncthreads();
  if (tid < n)
    out[tid] = prev;
}

__global__ void scatter_agents_kernel(const int *__restrict__ cell_ids,
                                      int *__restrict__ cell_fill,
                                      const int *__restrict__ cell_start,
                                      int *__restrict__ sorted_agents, int N) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= N)
    return;
  int c = cell_ids[i];
  int pos = atomicAdd(&cell_fill[c], 1) + cell_start[c];
  sorted_agents[pos] = i;
}

__global__ void boids_opt_kernel(
    const float *__restrict__ px_in, const float *__restrict__ py_in,
    const float *__restrict__ vx_in, const float *__restrict__ vy_in,
    const int *__restrict__ gi_in, float *__restrict__ px_out,
    float *__restrict__ py_out, float *__restrict__ vx_out,
    float *__restrict__ vy_out, int *__restrict__ gi_out,
    const int *__restrict__ cell_start, const int *__restrict__ cell_count,
    const int *__restrict__ sorted_agents, const float *__restrict__ obs_x,
    const float *__restrict__ obs_y, const float *__restrict__ obs_r,
    const float *__restrict__ gx, const float *__restrict__ gy, int N,
    int n_obs, int n_goals) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= N)
    return;

  float ax = px_in[i], ay = py_in[i], avx = vx_in[i], avy = vy_in[i];
  int gi = steering_clamp_goal(gi_in[i], n_goals);
  int cx = steering_grid_coord(ax);
  int cy = steering_grid_coord(ay);
  float sep_x = 0, sep_y = 0, aln_x = 0, aln_y = 0, coh_x = 0, coh_y = 0;
  int n_nbr = 0, n_sep = 0;
  const float R2 = PERCEPTION_RADIUS * PERCEPTION_RADIUS;
  const float S2 = SEPARATION_DIST * SEPARATION_DIST;

  for (int dy = -1; dy <= 1; dy++) {
    int ny = cy + dy;
    if (ny < 0 || ny >= GRID_DIM)
      continue;
    for (int dx = -1; dx <= 1; dx++) {
      int nx = cx + dx;
      if (nx < 0 || nx >= GRID_DIM)
        continue;
      int cell = ny * GRID_DIM + nx;
      int start = cell_start[cell];
      int cnt = cell_count[cell];
      for (int k = 0; k < cnt; k++) {
        int j = sorted_agents[start + k];
        if (j == i)
          continue;
        steering_accum_neighbour(ax, ay, px_in[j], py_in[j], vx_in[j], vy_in[j],
                                 R2, S2, &sep_x, &sep_y, &n_sep, &aln_x, &aln_y,
                                 &coh_x, &coh_y, &n_nbr);
      }
    }
  }

  Vec2 flock = steering_flock(sep_x, sep_y, n_sep, aln_x, aln_y, coh_x, coh_y,
                              n_nbr, ax, ay, avx, avy);
  Obstacle obs_loc[MAX_OBSTACLES];
  for (int o = 0; o < n_obs; o++)
    obs_loc[o] = (Obstacle){obs_x[o], obs_y[o], obs_r[o]};
  Vec2 acc = steering_forces(flock, ax, ay, gx[gi], gy[gi], obs_loc, n_obs);

  float npx, npy, nvx, nvy;
  steering_integrate(ax, ay, avx, avy, acc, &npx, &npy, &nvx, &nvy);
  px_out[i] = npx;
  py_out[i] = npy;
  vx_out[i] = nvx;
  vy_out[i] = nvy;
  gi_out[i] = gi;
}

static void gpu_opt_rebuild_grid(int N) {
  int b = gos.buf;
  int nblocks = (N + BLOCK_SIZE - 1) / BLOCK_SIZE;
  CUDA_CHECK(cudaMemset(gos.d_cell_count, 0, GRID_CELLS * sizeof(int)));
  CUDA_CHECK(cudaMemset(gos.d_cell_fill, 0, GRID_CELLS * sizeof(int)));
  assign_cells_kernel<<<nblocks, BLOCK_SIZE>>>(gos.d_px[b], gos.d_py[b],
                                               gos.d_cell_ids, N);
  CUDA_POST_KERNEL_CHECK();
  count_cells_kernel<<<nblocks, BLOCK_SIZE>>>(gos.d_cell_ids, gos.d_cell_count,
                                              N);
  CUDA_POST_KERNEL_CHECK();

  int scan_blk = 1;
  while (scan_blk < GRID_CELLS)
    scan_blk <<= 1;
  exclusive_scan_kernel<<<1, scan_blk, scan_blk * sizeof(int)>>>(
      gos.d_cell_count, gos.d_cell_start, GRID_CELLS);
  CUDA_POST_KERNEL_CHECK();
  scatter_agents_kernel<<<nblocks, BLOCK_SIZE>>>(
      gos.d_cell_ids, gos.d_cell_fill, gos.d_cell_start, gos.d_sorted_agents,
      N);
  CUDA_POST_KERNEL_CHECK();
}

void gpu_opt_free(void);

void gpu_opt_init(const Simulation *s) {
  gpu_require_device();
  gpu_opt_free();
  if (GRID_CELLS > CUDA_MAX_BLOCK_THREADS)
    cf_die("gpu_opt_init: GRID_CELLS exceeds one-block exclusive scan");

  int N = s->n_agents;
  gos.n_agents = N;
  gos.n_obstacles = s->n_obstacles;
  gos.n_goals = s->n_goals;
  gos.buf = 0;

  for (int b = 0; b < 2; b++) {
    CUDA_CHECK(cudaMalloc(&gos.d_px[b], (size_t)N * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&gos.d_py[b], (size_t)N * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&gos.d_vx[b], (size_t)N * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&gos.d_vy[b], (size_t)N * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&gos.d_gi[b], (size_t)N * sizeof(int)));
  }
  CUDA_CHECK(cudaMalloc(&gos.d_cell_ids, (size_t)N * sizeof(int)));
  CUDA_CHECK(cudaMalloc(&gos.d_cell_count, GRID_CELLS * sizeof(int)));
  CUDA_CHECK(cudaMalloc(&gos.d_cell_start, GRID_CELLS * sizeof(int)));
  CUDA_CHECK(cudaMalloc(&gos.d_cell_fill, GRID_CELLS * sizeof(int)));
  CUDA_CHECK(cudaMalloc(&gos.d_sorted_agents, (size_t)N * sizeof(int)));
  CUDA_CHECK(cudaMalloc(&gos.d_obs_x, (size_t)s->n_obstacles * sizeof(float)));
  CUDA_CHECK(cudaMalloc(&gos.d_obs_y, (size_t)s->n_obstacles * sizeof(float)));
  CUDA_CHECK(cudaMalloc(&gos.d_obs_r, (size_t)s->n_obstacles * sizeof(float)));
  CUDA_CHECK(cudaMalloc(&gos.d_gx, (size_t)s->n_goals * sizeof(float)));
  CUDA_CHECK(cudaMalloc(&gos.d_gy, (size_t)s->n_goals * sizeof(float)));
  CUDA_CHECK(cudaMallocHost(&gos.h_px, (size_t)N * sizeof(float)));
  CUDA_CHECK(cudaMallocHost(&gos.h_py, (size_t)N * sizeof(float)));
  CUDA_CHECK(cudaMallocHost(&gos.h_vx, (size_t)N * sizeof(float)));
  CUDA_CHECK(cudaMallocHost(&gos.h_vy, (size_t)N * sizeof(float)));
  CUDA_CHECK(cudaMallocHost(&gos.h_gi, (size_t)N * sizeof(int)));

  float *ox = (float *)malloc((size_t)s->n_obstacles * sizeof(float));
  float *oy = (float *)malloc((size_t)s->n_obstacles * sizeof(float));
  float *or_ = (float *)malloc((size_t)s->n_obstacles * sizeof(float));
  if (!ox || !oy || !or_)
    cf_die("gpu_opt_init: out of memory");
  for (int o = 0; o < s->n_obstacles; o++) {
    ox[o] = s->obstacles[o].x;
    oy[o] = s->obstacles[o].y;
    or_[o] = s->obstacles[o].radius;
  }
  CUDA_CHECK(cudaMemcpy(gos.d_obs_x, ox, (size_t)s->n_obstacles * sizeof(float),
                        cudaMemcpyHostToDevice));
  CUDA_CHECK(cudaMemcpy(gos.d_obs_y, oy, (size_t)s->n_obstacles * sizeof(float),
                        cudaMemcpyHostToDevice));
  CUDA_CHECK(cudaMemcpy(gos.d_obs_r, or_,
                        (size_t)s->n_obstacles * sizeof(float),
                        cudaMemcpyHostToDevice));
  free(ox);
  free(oy);
  free(or_);

  float *hx = (float *)malloc((size_t)s->n_goals * sizeof(float));
  float *hy = (float *)malloc((size_t)s->n_goals * sizeof(float));
  if (!hx || !hy)
    cf_die("gpu_opt_init: out of memory");
  for (int g = 0; g < s->n_goals; g++) {
    hx[g] = s->goals[g].x;
    hy[g] = s->goals[g].y;
  }
  CUDA_CHECK(cudaMemcpy(gos.d_gx, hx, (size_t)s->n_goals * sizeof(float),
                        cudaMemcpyHostToDevice));
  CUDA_CHECK(cudaMemcpy(gos.d_gy, hy, (size_t)s->n_goals * sizeof(float),
                        cudaMemcpyHostToDevice));
  free(hx);
  free(hy);

  gpu_opt_upload_from_host(s);
}

void gpu_opt_free(void) {
  for (int b = 0; b < 2; b++) {
    cudaFree(gos.d_px[b]);
    cudaFree(gos.d_py[b]);
    cudaFree(gos.d_vx[b]);
    cudaFree(gos.d_vy[b]);
    cudaFree(gos.d_gi[b]);
  }
  cudaFree(gos.d_cell_ids);
  cudaFree(gos.d_cell_count);
  cudaFree(gos.d_cell_start);
  cudaFree(gos.d_cell_fill);
  cudaFree(gos.d_sorted_agents);
  cudaFree(gos.d_obs_x);
  cudaFree(gos.d_obs_y);
  cudaFree(gos.d_obs_r);
  cudaFree(gos.d_gx);
  cudaFree(gos.d_gy);
  cudaFreeHost(gos.h_px);
  cudaFreeHost(gos.h_py);
  cudaFreeHost(gos.h_vx);
  cudaFreeHost(gos.h_vy);
  cudaFreeHost(gos.h_gi);
  memset(&gos, 0, sizeof(gos));
}

void gpu_opt_run_step(Simulation *s) {
  gpu_opt_launch_kernel_only();
  CUDA_CHECK(cudaDeviceSynchronize());
  gpu_opt_copy_to_host(s);
  simulation_finish_step(s);
  gpu_opt_upload_from_host(s);
}

void gpu_opt_launch_kernel_only(void) {
  int N = gos.n_agents;
  int r = gos.buf;
  int w = 1 - r;
  gpu_opt_rebuild_grid(N);
  int nblocks = (N + BLOCK_SIZE - 1) / BLOCK_SIZE;
  boids_opt_kernel<<<nblocks, BLOCK_SIZE>>>(
      gos.d_px[r], gos.d_py[r], gos.d_vx[r], gos.d_vy[r], gos.d_gi[r],
      gos.d_px[w], gos.d_py[w], gos.d_vx[w], gos.d_vy[w], gos.d_gi[w],
      gos.d_cell_start, gos.d_cell_count, gos.d_sorted_agents, gos.d_obs_x,
      gos.d_obs_y, gos.d_obs_r, gos.d_gx, gos.d_gy, N, gos.n_obstacles,
      gos.n_goals);
  CUDA_POST_KERNEL_CHECK();
  gos.buf = w;
}

static void gpu_opt_copy_to_host(Simulation *s) {
  int N = gos.n_agents;
  int b = gos.buf;
  CUDA_CHECK(cudaMemcpy(gos.h_px, gos.d_px[b], (size_t)N * sizeof(float),
                        cudaMemcpyDeviceToHost));
  CUDA_CHECK(cudaMemcpy(gos.h_py, gos.d_py[b], (size_t)N * sizeof(float),
                        cudaMemcpyDeviceToHost));
  CUDA_CHECK(cudaMemcpy(gos.h_vx, gos.d_vx[b], (size_t)N * sizeof(float),
                        cudaMemcpyDeviceToHost));
  CUDA_CHECK(cudaMemcpy(gos.h_vy, gos.d_vy[b], (size_t)N * sizeof(float),
                        cudaMemcpyDeviceToHost));
  CUDA_CHECK(cudaMemcpy(gos.h_gi, gos.d_gi[b], (size_t)N * sizeof(int),
                        cudaMemcpyDeviceToHost));
  for (int i = 0; i < N; i++) {
    s->agents[i].pos.x = gos.h_px[i];
    s->agents[i].pos.y = gos.h_py[i];
    s->agents[i].vel.x = gos.h_vx[i];
    s->agents[i].vel.y = gos.h_vy[i];
    s->agents[i].goal_idx = gos.h_gi[i];
  }
}

static void gpu_opt_upload_from_host(const Simulation *s) {
  int N = gos.n_agents;
  int b = gos.buf;
  for (int i = 0; i < N; i++) {
    gos.h_px[i] = s->agents[i].pos.x;
    gos.h_py[i] = s->agents[i].pos.y;
    gos.h_vx[i] = s->agents[i].vel.x;
    gos.h_vy[i] = s->agents[i].vel.y;
    gos.h_gi[i] = s->agents[i].goal_idx;
  }
  CUDA_CHECK(cudaMemcpy(gos.d_px[b], gos.h_px, (size_t)N * sizeof(float),
                        cudaMemcpyHostToDevice));
  CUDA_CHECK(cudaMemcpy(gos.d_py[b], gos.h_py, (size_t)N * sizeof(float),
                        cudaMemcpyHostToDevice));
  CUDA_CHECK(cudaMemcpy(gos.d_vx[b], gos.h_vx, (size_t)N * sizeof(float),
                        cudaMemcpyHostToDevice));
  CUDA_CHECK(cudaMemcpy(gos.d_vy[b], gos.h_vy, (size_t)N * sizeof(float),
                        cudaMemcpyHostToDevice));
  CUDA_CHECK(cudaMemcpy(gos.d_gi[b], gos.h_gi, (size_t)N * sizeof(int),
                        cudaMemcpyHostToDevice));
}

BenchResult gpu_opt_bench_kernel(int n_agents, int n_steps) {
  return gpu_bench_kernel(n_agents, n_steps, "GPU_OPT_KERNEL", gpu_opt_init,
                          gpu_opt_launch_kernel_only, gpu_opt_free);
}

BenchResult gpu_opt_bench_frame(int n_agents, int n_steps) {
  return gpu_bench_frame(n_agents, n_steps, "GPU_OPT_FRAME", gpu_opt_init,
                         gpu_opt_run_step, gpu_opt_free);
}
