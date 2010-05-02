#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <dirent.h> /* for PATH_MAX */
#include <assert.h>

#include <fcgiapp.h>
#include <fcgi_stdio.h>

#include <rrd.h>

#include "common.h"
#include "action_graph.h"
#include "graph_list.h"
#include "utils_params.h"

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
};
typedef struct graph_def_s graph_def_t;

static int graph_def_add_ds (graph_def_t *gd, /* {{{ */
    const char *file,
    const char *in_ds_name, size_t ds_name_len)
{
  char ds_name[ds_name_len + 1];
  data_source_t *ds;

  strncpy (ds_name, in_ds_name, sizeof (ds_name));
  ds_name[sizeof (ds_name) - 1] = 0;

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
  ds->color = 0xff0000;

  gd->data_sources_num++;

  return (0);
} /* }}} int graph_def_add_ds */

static graph_def_t *graph_def_from_rrd_file (char *file) /* {{{ */
{
  char *rrd_argv[] = { "info", file, NULL };
  int rrd_argc = (sizeof (rrd_argv) / sizeof (rrd_argv[0])) - 1;
  rrd_info_t *info;
  rrd_info_t *ptr;
  graph_def_t *gd;

  gd = malloc (sizeof (*gd));
  if (gd == NULL)
    return (NULL);
  memset (gd, 0, sizeof (*gd));

  gd->data_sources = NULL;

  info = rrd_info (rrd_argc, rrd_argv);
  if (info == NULL)
  {
    printf ("%s: rrd_info (%s) failed.\n", __func__, file);
    free (gd);
    return (NULL);
  }

  for (ptr = info; ptr != NULL; ptr = ptr->next)
  {
    size_t keylen;
    size_t dslen;

    if (strncmp ("ds[", ptr->key, strlen ("ds[")) != 0)
      continue;

    keylen = strlen (ptr->key);
    if (keylen < strlen ("ds[?].index"))
      continue;

    dslen = keylen - strlen ("ds[].index");
    assert (dslen >= 1);

    if (strcmp ("].index", ptr->key + (strlen ("ds[") + dslen)) != 0)
      continue;

    graph_def_add_ds (gd, file, ptr->key + strlen ("ds["), dslen);
  }

  rrd_info_free (info);

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

  printf ("rrd_file = %s;\n", rrd_file);

  return (graph_def_from_rrd_file (rrd_file));
} /* }}} graph_def_t *graph_def_from_gl */

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
  graph_list_t gl;
  graph_def_t *gd;
  int status;
  size_t i;

  memset (&gl, 0, sizeof (gl));
  status = init_gl (&gl);
  if (status != 0)
  {
    printf ("Content-Type: text/plain\n\n"
        "init_gl failed with status %i.\n", status);
    return (status);
  }

  printf ("Content-Type: text/plain\n\n"
      "Hello, this is %s\n", __func__);
  gd = graph_def_from_gl (&gl);
  if (gd == NULL)
  {
    printf ("graph_def_from_gl failed.\n");
    return (0);
  }

  for (i = 0; i < gd->data_sources_num; i++)
  {
    printf ("data source %lu: %s @ %s\n",
        (unsigned long) i, gd->data_sources[i].name, gd->data_sources[i].file);
  }

  /* FIXME: Free gd */

  return (0);
} /* }}} int action_graph */

/* vim: set sw=2 sts=2 et fdm=marker : */
