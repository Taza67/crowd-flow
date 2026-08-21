/* dump.c — CSV dump and binary rec. */

#include "dump.h"
#include "constants.h"
#include "cpu_naive.h"
#include "cpu_opt.h"
#include "simulation.h"
#include "utils.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WITH_GPU
#include "gpu_naive.h"
#include "gpu_opt.h"
#endif

static int select_backend(const char *mode, const char *topic,
                          void (**step)(Simulation *),
                          void (**gpu_init)(const Simulation *),
                          void (**gpu_free)(void)) {
  *step = NULL;
  *gpu_init = NULL;
  *gpu_free = NULL;
  if (strcmp(mode, CPU_NAIVE) == 0) {
    *step = cpu_naive_run_step;
  } else if (strcmp(mode, CPU_OPT) == 0) {
    *step = cpu_opt_run_step;
#ifdef WITH_GPU
  } else if (strcmp(mode, GPU_NAIVE) == 0) {
    *step = gpu_naive_run_step;
    *gpu_init = gpu_naive_init;
    *gpu_free = gpu_naive_free;
  } else if (strcmp(mode, GPU_OPT) == 0) {
    *step = gpu_opt_run_step;
    *gpu_init = gpu_opt_init;
    *gpu_free = gpu_opt_free;
#endif
  }
  if (!*step && (strcmp(mode, GPU_NAIVE) == 0 || strcmp(mode, GPU_OPT) == 0)) {
    cf_log(stderr, CF_ST_WARN, topic, "GPU not compiled");
    return EXIT_FAILURE;
  }
  if (!*step) {
    cf_log(stderr, CF_ST_FAIL, topic,
           "unknown mode (cpu_naive cpu_opt gpu_naive gpu_opt)");
    exit(EXIT_FAILURE);
  }
  return EXIT_SUCCESS;
}

static Simulation *dump_start(int n_agents, int n_steps, const char *mode,
                              const char *topic, void (**step)(Simulation *),
                              void (**gpu_init)(const Simulation *),
                              void (**gpu_free)(void)) {
  if (n_agents <= 0 || n_agents > MAX_AGENTS || n_steps < 0) {
    cf_log(stderr, CF_ST_FAIL, topic, "n_agents / n_steps out of range");
    exit(EXIT_FAILURE);
  }
  if (!mode)
    mode = CPU_OPT;
  if (select_backend(mode, topic, step, gpu_init, gpu_free) != EXIT_SUCCESS)
    return NULL;
  cf_log(stderr, CF_ST_INFO, topic, "mode=%s  N=%d  steps=%d  seed=%u", mode,
         n_agents, n_steps, SIM_SEED);
  Simulation *s = simulation_create(n_agents, SIM_SEED);
  if (*gpu_init)
    (*gpu_init)(s);
  return s;
}

static void dump_obstacles(const Simulation *s) {
  printf("#obstacle,x,y,radius\n");
  for (int o = 0; o < s->n_obstacles; o++) {
    const Obstacle *ob = &s->obstacles[o];
    printf("#obstacle,%.6f,%.6f,%.6f\n", ob->x, ob->y, ob->radius);
  }
}

static void dump_state(const Simulation *s, int t, double step_ms) {
  for (int i = 0; i < s->n_agents; i++) {
    const Agent *a = &s->agents[i];
    printf("%d,%d,%.6f,%.6f,%.6f,%.6f,%d,%.4f\n", t, i, a->pos.x, a->pos.y,
           a->vel.x, a->vel.y, a->goal_idx, step_ms);
  }
}

static int16_t rec_q(float v) {
  float s = v * REC_XY_SCALE;
  if (s > 32767.f)
    return 32767;
  if (s < -32767.f)
    return -32767;
  return (int16_t)lrintf(s);
}

static void rec_header(const Simulation *s, int n_frames) {
  uint32_t n_obs = (uint32_t)s->n_obstacles;
  uint32_t n_agents = (uint32_t)s->n_agents;
  uint32_t frames = (uint32_t)n_frames;
  fwrite(REC_MAGIC, 4, 1, stdout);
  fwrite(&n_obs, 4, 1, stdout);
  fwrite(&n_agents, 4, 1, stdout);
  fwrite(&frames, 4, 1, stdout);
  for (int o = 0; o < s->n_obstacles; o++) {
    const Obstacle *ob = &s->obstacles[o];
    float trip[3] = {ob->x, ob->y, ob->radius};
    fwrite(trip, 4, 3, stdout);
  }
}

static void rec_frame(const Simulation *s, double step_ms) {
  int n = s->n_agents;
  float vsum = 0.f, vmax = 0.f;
  for (int i = 0; i < n; i++) {
    float sp = hypotf(s->agents[i].vel.x, s->agents[i].vel.y);
    vsum += sp;
    if (sp > vmax)
      vmax = sp;
  }
  float stats[3] = {(float)step_ms, n ? vsum / (float)n : 0.f, vmax};
  fwrite(stats, 4, 3, stdout);
  for (int i = 0; i < n; i++) {
    int16_t xy[2] = {rec_q(s->agents[i].pos.x), rec_q(s->agents[i].pos.y)};
    fwrite(xy, 2, 2, stdout);
  }
  for (int i = 0; i < n; i++) {
    unsigned char g = (unsigned char)s->agents[i].goal_idx;
    fwrite(&g, 1, 1, stdout);
  }
}

static int capture_end(Simulation *s, void (*gpu_free)(void)) {
  if (gpu_free)
    gpu_free();
  free(s);
  return EXIT_SUCCESS;
}

int dump_run(int n_agents, int n_steps, const char *mode) {
  void (*step)(Simulation *) = NULL;
  void (*gpu_init)(const Simulation *) = NULL;
  void (*gpu_free)(void) = NULL;
  Simulation *s =
      dump_start(n_agents, n_steps, mode, "dump", &step, &gpu_init, &gpu_free);
  if (!s)
    return EXIT_FAILURE;
  long rows = (long)n_agents * ((long)n_steps + 1);
  if (rows > 200000)
    cf_log(stderr, CF_ST_WARN, "dump", "rows=%ld  redirect stdout to a file",
           rows);
  dump_obstacles(s);
  printf("t,id,x,y,vx,vy,goal,step_ms\n");
  dump_state(s, 0, 0.0);
  for (int t = 1; t <= n_steps; t++) {
    double t0 = get_time_ms();
    step(s);
    dump_state(s, t, get_time_ms() - t0);
  }
  cf_log(stderr, CF_ST_OK, "dump", "mode=%s  N=%d  steps=%d", mode, n_agents,
         n_steps);
  return capture_end(s, gpu_free);
}

int rec_run(int n_agents, int n_steps, const char *mode) {
  void (*step)(Simulation *) = NULL;
  void (*gpu_init)(const Simulation *) = NULL;
  void (*gpu_free)(void) = NULL;
  Simulation *s =
      dump_start(n_agents, n_steps, mode, "rec", &step, &gpu_init, &gpu_free);
  if (!s)
    return EXIT_FAILURE;
  static char rec_buf[1 << 20];
  if (setvbuf(stdout, rec_buf, _IOFBF, sizeof(rec_buf)) != 0) {
    cf_log(stderr, CF_ST_FAIL, "rec", "setvbuf");
    exit(EXIT_FAILURE);
  }
  rec_header(s, n_steps + 1);
  rec_frame(s, 0.0);
  for (int t = 1; t <= n_steps; t++) {
    double t0 = get_time_ms();
    step(s);
    rec_frame(s, get_time_ms() - t0);
  }
  cf_log(stderr, CF_ST_OK, "rec", "mode=%s  N=%d  steps=%d", mode, n_agents,
         n_steps);
  return capture_end(s, gpu_free);
}
