/* simulation.c — spawn world, then finish each step (goals + contacts). */

#include "simulation.h"
#include "constraints.h"
#include "steering.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

_Static_assert(GRID_DIM == (int)(WORLD_SIZE / CELL_SIZE) + 1, "GRID_DIM");
_Static_assert(CONSTR_GRID_DIM == (int)(WORLD_SIZE / CONSTR_CELL_SIZE) + 1,
               "CONSTR_GRID_DIM");
_Static_assert(GRID_CELLS <= CUDA_MAX_BLOCK_THREADS, "gpu_opt one-block scan");

void simulation_init(Simulation *s, int n_agents, unsigned seed) {
  if (n_agents < 0 || n_agents > MAX_AGENTS) {
    cf_log(stderr, CF_ST_FAIL, "sim", "n_agents=%d  range=[0, %d]", n_agents,
           MAX_AGENTS);
    exit(EXIT_FAILURE);
  }

  srand(seed);
  s->n_agents = n_agents;

  const float ox[] = {0.f, 40.f, -40.f, 60.f, -60.f};
  const float oy[] = {0.f, 30.f, -30.f, -50.f, 50.f};
  const float or_[] = {10.f, 7.f, 7.f, 5.f, 5.f};
  s->n_obstacles = (int)(sizeof(ox) / sizeof(ox[0]));
  if (s->n_obstacles > MAX_OBSTACLES) {
    cf_log(stderr, CF_ST_FAIL, "sim", "n_obstacles=%d  max=%d", s->n_obstacles,
           MAX_OBSTACLES);
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < s->n_obstacles; i++)
    s->obstacles[i] = (Obstacle){ox[i], oy[i], or_[i]};

  const Goal defs[] = {
      {-75.f, 75.f}, {0.f, 85.f},  {75.f, 75.f},   {85.f, 0.f},
      {75.f, -75.f}, {0.f, -85.f}, {-75.f, -75.f}, {-85.f, 0.f},
  };
  s->n_goals = (int)(sizeof(defs) / sizeof(defs[0]));
  if (s->n_goals > MAX_GOALS) {
    cf_log(stderr, CF_ST_FAIL, "sim", "n_goals=%d  max=%d", s->n_goals,
           MAX_GOALS);
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < s->n_goals; i++)
    s->goals[i] = defs[i];

  for (int i = 0; i < n_agents; i++) {
    float rx, ry;
    int tries = 0;
    do {
      rx = ((float)rand() / RAND_MAX - 0.5f) * WORLD_SIZE * 0.88f;
      ry = ((float)rand() / RAND_MAX - 0.5f) * WORLD_SIZE * 0.88f;
      tries++;
    } while (tries < 40 &&
             inside_obstacle(rx, ry, s->obstacles, s->n_obstacles));

    float vx = ((float)rand() / RAND_MAX - 0.5f) * 1.5f;
    float vy = ((float)rand() / RAND_MAX - 0.5f) * 1.5f;
    s->agents[i] = (Agent){{rx, ry}, {vx, vy}, i % s->n_goals};
  }
}

Simulation *simulation_create(int n_agents, unsigned seed) {
  Simulation *s = (Simulation *)malloc(sizeof(Simulation));
  if (!s)
    cf_die("simulation_create: out of memory");
  simulation_init(s, n_agents, seed);
  return s;
}

void simulation_finish_step(Simulation *s) {
  for (int i = 0; i < s->n_agents; i++) {
    Agent *a = &s->agents[i];
    const Goal *g = &s->goals[a->goal_idx];
    if (steering_goal_reached(a->pos.x, a->pos.y, g->x, g->y))
      a->goal_idx = (a->goal_idx + 1) % s->n_goals;
  }
  resolve_constraints(s);
}
