/* constants.h — world, physics, grids, bench, rec. */

#ifndef CONSTANTS_H
#define CONSTANTS_H

/* World */
#define MAX_AGENTS 100000
#define WORLD_SIZE 200.0f
#define WORLD_HALF (WORLD_SIZE * 0.5f)
#define MAX_OBSTACLES 16

/* Physics */
#define PERCEPTION_RADIUS 8.0f
#define SEPARATION_DIST 3.0f
#define AGENT_RADIUS 0.6f
#define MAX_SPEED 4.5f
#define MAX_FORCE 3.0f
#define DT 0.016f
#define CONTACT_SLOP 0.05f
#define CONTACT_PUSH_GAIN 3.0f
#define CONTACT_PUSH_BIAS 0.1f
#define CONTACT_PAIR_SLOP 0.02f

/* Goals */
#define MAX_GOALS 8
#define GOAL_REACH_DIST 4.0f
#define SIM_SEED 42u

/* Boids weights */
#define W_SEPARATION 3.5f
#define W_ALIGNMENT 0.8f
#define W_COHESION 0.6f
#define W_GOAL 2.0f
#define W_OBSTACLE 8.0f
#define OBSTACLE_CLEARANCE (SEPARATION_DIST * 3.5f)

/* Perception grid: cell = radius, +1 so WORLD_HALF maps inside */
#define CELL_SIZE PERCEPTION_RADIUS
#define GRID_DIM 26
#define GRID_CELLS (GRID_DIM * GRID_DIM)

/* Constraint grid: cell = 3 agent radii, +1 for the far edge */
#define CONSTR_CELL_SIZE (AGENT_RADIUS * 3.0f)
#define CONSTR_GRID_DIM 112
#define CONSTR_GRID_CELLS (CONSTR_GRID_DIM * CONSTR_GRID_DIM)
#define N_PASSES 5
#define RESTITUTION 0.15f

/* Benchmark */
#define CPU_NAIVE "cpu_naive"
#define CPU_OPT "cpu_opt"
#define GPU_NAIVE "gpu_naive"
#define GPU_OPT "gpu_opt"
#define DUMP_DEFAULT_AGENTS 200
#define DUMP_DEFAULT_STEPS 20
#define BENCH_DEFAULT_AGENTS 100
#define BENCH_DEFAULT_STEPS 100

/* Replay rec (int16 positions) */
#define REC_MAGIC "CF1"
#define REC_XY_SCALE (32767.0f / WORLD_HALF)

/* GPU */
#define BLOCK_SIZE 256
#define CUDA_MAX_BLOCK_THREADS 1024

#endif
