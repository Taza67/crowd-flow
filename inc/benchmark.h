/* benchmark.h — time one backend. */

#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "constants.h"
#include "simulation.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  double total_time_ms;
  double avg_step_ms;
  double min_step_ms;
  double max_step_ms;
  double fps;
  int n_agents;
  long n_steps;
  char version[32];
} BenchResult;

static inline void benchresult_init(BenchResult *r, const char *version,
                                    int n_agents, int n_steps) {
  memset(r, 0, sizeof(*r));
  strncpy(r->version, version, 31);
  r->version[31] = '\0';
  r->n_agents = n_agents;
  r->n_steps = n_steps;
  r->min_step_ms = 1e300;
}

static inline void benchresult_note_step(BenchResult *r, double dt_ms) {
  r->avg_step_ms += dt_ms;
  if (dt_ms < r->min_step_ms)
    r->min_step_ms = dt_ms;
  if (dt_ms > r->max_step_ms)
    r->max_step_ms = dt_ms;
}

static inline void benchresult_finish(BenchResult *r, double total_ms) {
  r->total_time_ms = total_ms;
  r->avg_step_ms /= r->n_steps;
  r->fps = (r->avg_step_ms > 0) ? 1000.0 / r->avg_step_ms : 0;
}

static inline void benchresult_print(const BenchResult *br) {
  printf("%s,%d,%ld,%.4f,%.4f,%.4f,%.4f,%.2f\n", br->version, br->n_agents,
         br->n_steps, br->total_time_ms, br->avg_step_ms, br->min_step_ms,
         br->max_step_ms, br->fps);
}

BenchResult run_bench_generic(int n_agents, int n_steps, const char *version,
                              void (*step_func)(Simulation *));
int benchmark_run(int n_agents, int n_steps, const char *mode);

#endif
