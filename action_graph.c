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

static void graph_def_free (graph_def_t *gd) /* {{{ */
{
  size_t i;

  if (gd == NULL)
    return;

  for (i = 0; i < gd->data_sources_num; i++)
  {
    free (gd->data_sources[i].file);
    free (gd->data_sources[i].name);
    free (gd->data_sources[i].legend);
  }
  free (gd->data_sources);
  free (gd);
} /* }}} void graph_def_free */

static int hsv_to_rgb (double *hsv, double *rgb) /* {{{ */
{
  double c = hsv[2] * hsv[1];
  double h = hsv[0] / 60.0;
  double x = c * (1.0 - fabs (fmod (h, 2.0) - 1));
  double m = hsv[2] - c;

  rgb[0] = 0.0;
  rgb[1] = 0.0;
  rgb[2] = 0.0;

       if ((0.0 <= h) && (h < 1.0)) { rgb[0] = 1.0; rgb[1] = x; rgb[2] = 0.0; }
  else if ((1.0 <= h) && (h < 2.0)) { rgb[0] = x; rgb[1] = 1.0; rgb[2] = 0.0; }
  else if ((2.0 <= h) && (h < 3.0)) { rgb[0] = 0.0; rgb[1] = 1.0; rgb[2] = x; }
  else if ((3.0 <= h) && (h < 4.0)) { rgb[0] = 0.0; rgb[1] = x; rgb[2] = 1.0; }
  else if ((4.0 <= h) && (h < 5.0)) { rgb[0] = x; rgb[1] = 0.0; rgb[2] = 1.0; }
  else if ((5.0 <= h) && (h < 6.0)) { rgb[0] = 1.0; rgb[1] = 0.0; rgb[2] = x; }

  rgb[0] += m;
  rgb[1] += m;
  rgb[2] += m;

  return (0);
} /* }}} int hsv_to_rgb */

static uint32_t rgb_to_uint32 (double *rgb) /* {{{ */
{
  uint8_t r;
  uint8_t g;
  uint8_t b;

  r = (uint8_t) (255.0 * rgb[0]);
  g = (uint8_t) (255.0 * rgb[1]);
  b = (uint8_t) (255.0 * rgb[2]);

  return ((((uint32_t) r) << 16)
      | (((uint32_t) g) << 8)
      | ((uint32_t) b));
} /* }}} uint32_t rgb_to_uint32 */

static uint32_t get_random_color (void) /* {{{ */
{
  double hsv[3] = { 0.0, 1.0, 1.0 };
  double rgb[3] = { 0.0, 0.0, 0.0 };

  hsv[0] = 360.0 * ((double) rand ()) / (((double) RAND_MAX) + 1.0);

  hsv_to_rgb (hsv, rgb);

  return (rgb_to_uint32 (rgb));
} /* }}} uint32_t get_random_color */

static int graph_def_add_ds (graph_def_t *gd, /* {{{ */
    const char *file, const char *ds_name)
{
  data_source_t *ds;

  ds = realloc (gd->data_sources, sizeof (*ds) * (gd->data_sources_num + 1));
  if (ds == NULL)
    return (ENOMEM);
  gd->data_sources = ds;

  ds = gd->data_sources + gd->data_sources_num;
  memset (ds, 0, sizeof (*ds));

  ds->file = strdup (file);
  if (ds->file == NULL)
    return (ENOMEM);

  ds->name = strdup (ds_name);
  if (ds->name == NULL)
  {
    free (ds->file);
    return (ENOMEM);
  }

  ds->legend = NULL;
  ds->color = get_random_color ();

  gd->data_sources_num++;

  return (0);
} /* }}} int graph_def_add_ds */

static graph_def_t *graph_def_from_rrd_file (char *file) /* {{{ */
{
  graph_def_t *gd;
  char **dses = NULL;
  size_t dses_num = 0;
  int status;
  size_t i;

  gd = malloc (sizeof (*gd));
  if (gd == NULL)
    return (NULL);
  memset (gd, 0, sizeof (*gd));
  gd->data_sources = NULL;

  status = ds_list_from_rrd_file (file, &dses_num, &dses);
  if (status != 0)
  {
    free (gd);
    return (NULL);
  }

  for (i = 0; i < dses_num; i++)
  {
    graph_def_add_ds (gd, file, dses[i]);
    free (dses[i]);
  }

  free (dses);

  return (gd);
} /* }}} graph_def_t *graph_def_from_rrd_file */

static graph_def_t *graph_def_from_gl (const graph_list_t *gl) /* {{{ */
{
  char rrd_file[PATH_MAX];

  if ((gl->plugin_instance == NULL) && (gl->type_instance == NULL))
    snprintf (rrd_file, sizeof (rrd_file), "%s/%s/%s/%s.rrd",
        DATA_DIR, gl->host, gl->plugin, gl->type);
  else if (gl->type_instance == NULL)
    snprintf (rrd_file, sizeof (rrd_file), "%s/%s/%s-%s/%s.rrd",
        DATA_DIR, gl->host, gl->plugin, gl->plugin_instance, gl->type);
  else if (gl->plugin_instance == NULL)
    snprintf (rrd_file, sizeof (rrd_file), "%s/%s/%s/%s-%s.rrd",
        DATA_DIR, gl->host, gl->plugin, gl->type, gl->type_instance);
  else
    snprintf (rrd_file, sizeof (rrd_file), "%s/%s/%s-%s/%s-%s.rrd",
        DATA_DIR, gl->host, gl->plugin, gl->plugin_instance,
        gl->type, gl->type_instance);
  rrd_file[sizeof (rrd_file) - 1] = 0;

  return (graph_def_from_rrd_file (rrd_file));
} /* }}} graph_def_t *graph_def_from_gl */

static int draw_graph_ds (graph_def_t *gd, /* {{{ */
    size_t index, str_array_t *args)
{
  data_source_t *ds;

  assert (index < gd->data_sources_num);

  ds = gd->data_sources + index;

  /* CDEFs */
  array_append_format (args, "DEF:def_%04zu_min=%s:%s:MIN",
      index, ds->file, ds->name);
  array_append_format (args, "DEF:def_%04zu_avg=%s:%s:AVERAGE",
      index, ds->file, ds->name);
  array_append_format (args, "DEF:def_%04zu_max=%s:%s:MAX",
      index, ds->file, ds->name);
  /* VDEFs */
  array_append_format (args, "VDEF:vdef_%04zu_min=def_%04zu_min,MINIMUM",
      index, index);
  array_append_format (args, "VDEF:vdef_%04zu_avg=def_%04zu_avg,AVERAGE",
      index, index);
  array_append_format (args, "VDEF:vdef_%04zu_max=def_%04zu_max,MAXIMUM",
      index, index);
  array_append_format (args, "VDEF:vdef_%04zu_lst=def_%04zu_avg,LAST",
      index, index);

  /* Graph part */
  array_append_format (args, "LINE1:def_%04zu_avg#%06x:%s", index, ds->color,
      (ds->legend != NULL) ? ds->legend : ds->name);
  array_append_format (args, "GPRINT:vdef_%04zu_min:%%lg min,", index);
  array_append_format (args, "GPRINT:vdef_%04zu_avg:%%lg avg,", index);
  array_append_format (args, "GPRINT:vdef_%04zu_max:%%lg max,", index);
  array_append_format (args, "GPRINT:vdef_%04zu_lst:%%lg last\\l", index);

  return (0);
} /* }}} int draw_graph_ds */

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

static int init_gl (graph_list_t *gl) /* {{{ */
{
  gl->host = param ("host");
  gl->plugin = param ("plugin");
  gl->plugin_instance = param ("plugin_instance");
  gl->type = param ("type");
  gl->type_instance = param ("type_instance");

  if ((gl->host == NULL)
      || (gl->plugin == NULL)
      || (gl->type == NULL))
    return (EINVAL);

  if ((gl->host[0] == 0) || (gl->host[0] == '.')
      || (gl->plugin[0] == 0) || (gl->plugin[0] == '.')
      || (gl->type[0] == 0) || (gl->type[0] == '.'))
    return (EINVAL);

  if ((strchr (gl->plugin, '-') != NULL)
      || (strchr (gl->type, '-') != NULL))
    return (EINVAL);

  if ((gl->plugin_instance != NULL)
      && (gl->plugin_instance[0] == 0))
    gl->plugin_instance = NULL;

  if ((gl->type_instance != NULL)
      && (gl->type_instance[0] == 0))
    gl->type_instance = NULL;

  return (0);
} /* }}} int init_gl */

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
