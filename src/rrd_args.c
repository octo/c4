#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "rrd_args.h"

rrd_args_t *ra_create (void) /* {{{ */
{
  rrd_args_t *ra;

  ra = malloc (sizeof (*ra));
  if (ra == NULL)
    return (NULL);
  memset (ra, 0, sizeof (*ra));

  ra->options = array_create ();
  ra->data    = array_create ();
  ra->calc    = array_create ();
  ra->areas   = array_create ();
  ra->lines   = array_create ();

  if ((ra->options == NULL)
      || (ra->data == NULL)
      || (ra->calc == NULL)
      || (ra->areas == NULL)
      || (ra->lines == NULL))
  {
    ra_destroy (ra);
    return (NULL);
  }

  return (ra);
} /* }}} rrd_args_t *ra_create */

void ra_destroy (rrd_args_t *ra) /* {{{ */
{
  if (ra == NULL)
    return;

  array_destroy (ra->options);
  array_destroy (ra->data);
  array_destroy (ra->calc);
  array_destroy (ra->areas);
  array_destroy (ra->lines);

  free (ra);
} /* }}} void ra_destroy */

int ra_argc (rrd_args_t *ra)
{
  if (ra == NULL)
    return (-EINVAL);

  return (array_argc (ra->options)
      + array_argc (ra->data)
      + array_argc (ra->calc)
      + array_argc (ra->areas)
      + array_argc (ra->lines));
} /* }}} int ra_argc */

char **ra_argv (rrd_args_t *ra) /* {{{ */
{
  size_t argc;
  char **argv;

  size_t pos;
  int tmp;

  if (ra == NULL)
    return (NULL);

  tmp = ra_argc (ra);
  if (tmp < 0)
    return (NULL);
  argc = (size_t) tmp;

  argv = calloc (argc + 1, sizeof (*argv));
  if (argv == NULL)
    return (NULL);

  pos = 0;
  argv[0] = NULL;

#define APPEND_FIELD(field) do                                               \
{                                                                            \
  size_t ary_argc;                                                           \
  char **ary_argv;                                                           \
                                                                             \
  ary_argc = (size_t) array_argc (ra->field);                                \
  ary_argv = array_argv (ra->field);                                         \
  if ((ary_argc > 0) && (ary_argv != NULL))                                  \
  {                                                                          \
    memcpy (argv + pos, ary_argv, ary_argc * sizeof (*ary_argv));            \
    pos += ary_argc;                                                         \
    argv[pos] = NULL;                                                        \
  }                                                                          \
} while (0)
 
  APPEND_FIELD (options);
  APPEND_FIELD (data);
  APPEND_FIELD (calc);
  APPEND_FIELD (areas);
  APPEND_FIELD (lines);

#undef APPEND_FIELD

  return (argv);
} /* }}} char **ra_argv */

void ra_argv_free (char **argv) /* {{{ */
{
  /* The pointers contained in the "argv" come from "array_argv". We don't need
   * to free them. We only need to free what we actually alloced directly in
   * "ra_argv". */
  free (argv);
} /* }}} void ra_argv_free */

/* vim: set sw=2 sts=2 et fdm=marker : */
