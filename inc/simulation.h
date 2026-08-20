/* simulation.h — world state. */

#ifndef SIMULATION_H
#define SIMULATION_H

#include "constants.h"
#include "obstacle.h"
#include "vec2.h"

typedef struct {
  float x, y;
} Goal;

typedef struct {
  Vec2 pos;
  Vec2 vel;
  int goal_idx;
} Agent;

typedef struct {
  int n_agents;
  int n_obstacles;
  int n_goals;
  Agent agents[MAX_AGENTS];
  Obstacle obstacles[MAX_OBSTACLES];
  Goal goals[MAX_GOALS];
} Simulation;

#ifdef __cplusplus
extern "C" {
#endif

void simulation_init(Simulation *s, int n_agents, unsigned seed);
Simulation *simulation_create(int n_agents, unsigned seed);
void simulation_finish_step(Simulation *s);

#ifdef __cplusplus
}
#endif

#endif
