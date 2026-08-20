/* utils.h — clock, aligned logs, fatal errors. */

#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define CF_ST_OK "OK"
#define CF_ST_FAIL "FAIL"
#define CF_ST_INFO "INFO"
#define CF_ST_WARN "WARN"
#define CF_ST_SKIP "SKIP"

static inline void cf_log(FILE *fp, const char *st, const char *topic,
                          const char *fmt, ...) {
  fprintf(fp, "%-4s  %-12s", st, topic);
  if (fmt && fmt[0]) {
    fputs("  ", fp);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
  }
  fputc('\n', fp);
}

static inline double get_time_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static inline void cf_die(const char *msg) {
  cf_log(stderr, CF_ST_FAIL, "error", "%s", msg);
  exit(EXIT_FAILURE);
}

#endif
