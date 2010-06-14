#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <dirent.h> /* for PATH_MAX */
#include <assert.h>
#include <math.h>

#include <rrd.h>

#include "common.h"
#include "action_graph.h"
#include "graph_list.h"
#include "utils_params.h"
#include "utils_array.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

struct data_source_s
{
  char *file;
  char *name;
  char *legend;
  double scale;
  _Bool nan_to_zero;
  _Bool draw_area;
  uint32_t color;
};
typedef struct data_source_s data_source_t;

struct graph_def_s
{
  data_source_t *data_sources;
  size_t data_sources_num;

  _Bool stack;

  int def_num;
};
typedef struct graph_def_s graph_def_t;

static void emulate_graph (int argc, char **argv) /* {{{ */
{
  int i;

  printf ("rrdtool \\\n");
  for (i = 0; i < argc; i++)
  {
    if (i < (argc - 1))
      printf ("  \"%s\" \\\n", argv[i]);
    else
      printf ("  \"%s\"\n", argv[i]);
  }
} /* }}} void emulate_graph */

static int ag_info_print (rrd_info_t *info) /* {{{ */
{
  if (info->type == RD_I_VAL)
    printf ("[info] %s = %g;\n", info->key, info->value.u_val);
  else if (info->type == RD_I_CNT)
    printf ("[info] %s = %lu;\n", info->key, info->value.u_cnt);
  else if (info->type == RD_I_STR)
    printf ("[info] %s = %s;\n", info->key, info->value.u_str);
  else if (info->type == RD_I_INT)
    printf ("[info] %s = %i;\n", info->key, info->value.u_int);
  else if (info->type == RD_I_BLO)
    printf ("[info] %s = [blob, %lu bytes];\n", info->key, info->value.u_blo.size);
  else
    printf ("[info] %s = [unknown type %#x];\n", info->key, info->type);

  return (0);
} /* }}} int ag_info_print */

static int output_graph (rrd_info_t *info) /* {{{ */
{
  rrd_info_t *img;

  for (img = info; img != NULL; img = img->next)
    if ((strcmp ("image", img->key) == 0)
        && (img->type == RD_I_BLO))
      break;

  if (img == NULL)
    return (ENOENT);

  printf ("Content-Type: image/png\n"
      "Content-Length: %lu\n"
      "\n",
      img->value.u_blo.size);
  fwrite (img->value.u_blo.ptr, img->value.u_blo.size,
      /* nmemb = */ 1, stdout);

  return (0);
} /* }}} int output_graph */

#define OUTPUT_ERROR(...) do {             \
  printf ("Content-Type: text/plain\n\n"); \
  printf (__VA_ARGS__);                    \
  return (0);                              \
} while (0)

int action_graph (void) /* {{{ */
{
  str_array_t *args;
  graph_config_t *cfg;
  graph_instance_t *inst;
  rrd_info_t *info;
  int status;

  cfg = graph_get_selected ();
  if (cfg == NULL)
    OUTPUT_ERROR ("graph_get_selected () failed.\n");

  inst = inst_get_selected (cfg);
  if (inst == NULL)
    OUTPUT_ERROR ("inst_get_selected (%p) failed.\n", (void *) cfg);

  args = array_create ();
  if (args == NULL)
    return (ENOMEM);

  array_append (args, "graph");
  array_append (args, "-");
  array_append (args, "--imgformat");
  array_append (args, "PNG");

  status = gl_instance_get_rrdargs (cfg, inst, args);
  if (status != 0)
  {
    array_destroy (args);
    OUTPUT_ERROR ("gl_instance_get_rrdargs failed with status %i.\n", status);
  }

  rrd_clear_error ();
  info = rrd_graph_v (array_argc (args), array_argv (args));
  if ((info == NULL) || rrd_test_error ())
  {
    printf ("Content-Type: text/plain\n\n");
    printf ("rrd_graph_v failed: %s\n", rrd_get_error ());
    emulate_graph (array_argc (args), array_argv (args));
  }
  else
  {
    int status;

    status = output_graph (info);
    if (status != 0)
    {
      rrd_info_t *ptr;

      printf ("Content-Type: text/plain\n\n");
      printf ("output_graph failed. Maybe the \"image\" info was not found?\n\n");

      for (ptr = info; ptr != NULL; ptr = ptr->next)
      {
        ag_info_print (ptr);
      }
    }
  }

  if (info != NULL)
    rrd_info_free (info);

  array_destroy (args);
  args = NULL;

  return (0);
} /* }}} int action_graph */

/* vim: set sw=2 sts=2 et fdm=marker : */
