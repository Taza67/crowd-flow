/* main.c — bench | dump | rec */

#include "benchmark.h"
#include "constants.h"
#include "dump.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(char *prog) {
  fprintf(stderr, "\n");
  fprintf(stderr, "  %s bench  [N] [steps] [mode]\n", prog);
  fprintf(stderr, "  %s dump   [N] [steps] [mode]\n", prog);
  fprintf(stderr, "  %s rec    [N] [steps] [mode]\n", prog);
  fprintf(stderr, "\n");
  fprintf(stderr, "  %-8s  %-12s  N=%-4d  steps=%-4d  %s  seed=%u\n", "bench",
          "timed run", BENCH_DEFAULT_AGENTS, BENCH_DEFAULT_STEPS, CPU_OPT,
          SIM_SEED);
  fprintf(stderr, "  %-8s  %-12s  N=%-4d  steps=%-4d  %s  seed=%u\n", "dump",
          "CSV dump", DUMP_DEFAULT_AGENTS, DUMP_DEFAULT_STEPS, CPU_OPT,
          SIM_SEED);
  fprintf(stderr, "  %-8s  %-12s  N=%-4d  steps=%-4d  %s  seed=%u\n", "rec",
          "binary rec", DUMP_DEFAULT_AGENTS, DUMP_DEFAULT_STEPS, CPU_OPT,
          SIM_SEED);
  fprintf(stderr, "  %-8s  %s\n", "mode",
          "cpu_naive  cpu_opt  gpu_naive  gpu_opt");
  fprintf(stderr, "\n");
}

static int arg_int(int argc, char **argv, int i, int def) {
  return (argc > i) ? atoi(argv[i]) : def;
}

static const char *arg_mode(int argc, char **argv, int i) {
  return (argc > i) ? argv[i] : CPU_OPT;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  if (strcmp(argv[1], "bench") == 0)
    return benchmark_run(arg_int(argc, argv, 2, BENCH_DEFAULT_AGENTS),
                         arg_int(argc, argv, 3, BENCH_DEFAULT_STEPS),
                         arg_mode(argc, argv, 4));

  if (strcmp(argv[1], "dump") == 0)
    return dump_run(arg_int(argc, argv, 2, DUMP_DEFAULT_AGENTS),
                    arg_int(argc, argv, 3, DUMP_DEFAULT_STEPS),
                    arg_mode(argc, argv, 4));

  if (strcmp(argv[1], "rec") == 0)
    return rec_run(arg_int(argc, argv, 2, DUMP_DEFAULT_AGENTS),
                   arg_int(argc, argv, 3, DUMP_DEFAULT_STEPS),
                   arg_mode(argc, argv, 4));

  usage(argv[0]);
  return EXIT_FAILURE;
}
