#ifndef RRD_ARGS_H
#define RRD_ARGS_H

#include "utils_array.h"

struct rrd_args_s
{
  str_array_t *options;
  str_array_t *data;
  str_array_t *calc;
  str_array_t *areas;
  str_array_t *lines;

  int index;
  char last_stack_cdef[64];
};
typedef struct rrd_args_s rrd_args_t;

rrd_args_t *ra_create (void);
void ra_destroy (rrd_args_t *ra);

int ra_argc (rrd_args_t *ra);
char **ra_argv (rrd_args_t *ra);
void ra_argv_free (char **argv);

#endif /* RRD_ARGS_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
