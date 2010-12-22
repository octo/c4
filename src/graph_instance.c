/**
 * collection4 - graph_instance.c
 * Copyright (C) 2010  Florian octo Forster
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors:
 *   Florian octo Forster <ff at octo.it>
 **/

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include "graph_instance.h"
#include "graph.h"
#include "graph_def.h"
#include "graph_ident.h"
#include "graph_list.h"
#include "common.h"
#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

struct graph_instance_s /* {{{ */
{
  graph_ident_t *select;

  graph_ident_t **files;
  size_t files_num;
}; /* }}} struct graph_instance_s */

struct def_callback_data_s
{
  graph_instance_t *inst;
  rrd_args_t *args;
};
typedef struct def_callback_data_s def_callback_data_t;

/*
 * Private functions
 */
struct ident_get_default_defs__data_s
{
  char **dses;
  size_t dses_num;
};
typedef struct ident_get_default_defs__data_s ident_get_default_defs__data_t;

static int ident_get_default_defs__callback (__attribute__((unused))
    graph_ident_t *ident,
    const char *ds_name, void *user_data)
{
  ident_get_default_defs__data_t *data = user_data;
  char **tmp;

  tmp = realloc (data->dses, (data->dses_num + 1) * sizeof (data->dses));
  if (tmp == NULL)
    return (ENOMEM);
  data->dses = tmp;

  data->dses[data->dses_num] = strdup (ds_name);
  if (data->dses[data->dses_num] == NULL)
    return (ENOMEM);

  data->dses_num++;
  return (0);
} /* }}} int ident_get_default_defs__callback */

/* Create one DEF for each data source in the file. Called by
 * "inst_get_default_defs" for each file. */
static graph_def_t *ident_get_default_defs (graph_config_t *cfg, /* {{{ */
    graph_ident_t *ident, graph_def_t *def_head)
{
  graph_def_t *defs = NULL;
  char *file;
  ident_get_default_defs__data_t ds_data;
  int status;
  size_t i;

  ds_data.dses = NULL;
  ds_data.dses_num = 0;

  if ((cfg == NULL) || (ident == NULL))
    return (def_head);

  file = ident_to_file (ident);
  if (file == NULL)
  {
    fprintf (stderr, "ident_get_default_defs: ident_to_file failed\n");
    return (def_head);
  }

  status = data_provider_get_ident_ds_names (ident,
      ident_get_default_defs__callback, &ds_data);
  if (status != 0)
  {
    free (file);
    return (def_head);
  }

  for (i = 0; i < ds_data.dses_num; i++)
  {
    graph_def_t *def;

    def = def_search (def_head, ident, ds_data.dses[i]);
    if (def != NULL)
      continue;

    def = def_create (cfg, ident, ds_data.dses[i]);
    if (def == NULL)
      continue;

    if (defs == NULL)
      defs = def;
    else
      def_append (defs, def);

    free (ds_data.dses[i]);
  }

  free (ds_data.dses);
  free (file);

  return (defs);
} /* }}} int ident_get_default_defs */

/* Called with each DEF in turn. Calls "def_get_rrdargs" with every appropriate
 * file / DEF pair. */
static int gl_instance_get_rrdargs_cb (graph_def_t *def, void *user_data) /* {{{ */
{
  def_callback_data_t *data = user_data;
  graph_instance_t *inst = data->inst;
  rrd_args_t *args = data->args;

  size_t i;
  int status;

  for (i = 0; i < inst->files_num; i++)
  {
    if (!def_matches (def, inst->files[i]))
      continue;

    status = def_get_rrdargs (def, inst->files[i], args);
    if (status != 0)
    {
      fprintf (stderr, "gl_instance_get_rrdargs_cb: def_get_rrdargs failed with status %i\n",
          status);
      fflush (stderr);
    }
  }

  return (0);
} /* }}} int gl_instance_get_rrdargs_cb */

static const char *get_part_from_param (const char *prim_key, /* {{{ */
    const char *sec_key)
{
  const char *val;

  val = param (prim_key);
  if (val != NULL)
    return (val);
  
  return (param (sec_key));
} /* }}} const char *get_part_from_param */

static graph_ident_t *inst_get_selector_from_params (void) /* {{{ */
{
  const char *host = get_part_from_param ("inst_host", "host");
  const char *plugin = get_part_from_param ("inst_plugin", "plugin");
  const char *plugin_instance = get_part_from_param ("inst_plugin_instance",
      "plugin_instance");
  const char *type = get_part_from_param ("inst_type", "type");
  const char *type_instance = get_part_from_param ("inst_type_instance",
      "type_instance");

  graph_ident_t *ident;

  if ((host == NULL)
      || (plugin == NULL) || (plugin_instance == NULL)
      || (type == NULL) || (type_instance == NULL))
  {
    fprintf (stderr, "inst_get_selected: A parameter is NULL\n");
    return (NULL);
  }

  ident = ident_create (host, plugin, plugin_instance, type, type_instance);
  if (ident == NULL)
  {
    fprintf (stderr, "inst_get_selected: ident_create failed\n");
    return (NULL);
  }

  return (ident);
} /* }}} graph_ident_t *inst_get_selector_from_params */

/*
 * Public functions
 */
graph_instance_t *inst_create (graph_config_t *cfg, /* {{{ */
    const graph_ident_t *ident)
{
  graph_instance_t *i;
  graph_ident_t *selector;

  if ((cfg == NULL) || (ident == NULL))
    return (NULL);

  i = malloc (sizeof (*i));
  if (i == NULL)
    return (NULL);
  memset (i, 0, sizeof (*i));

  selector = graph_get_selector (cfg);
  if (selector == NULL)
  {
    fprintf (stderr, "inst_create: graph_get_selector failed\n");
    free (i);
    return (NULL);
  }

  i->select = ident_copy_with_selector (selector, ident,
      IDENT_FLAG_REPLACE_ANY);
  if (i->select == NULL)
  {
    fprintf (stderr, "inst_create: ident_copy_with_selector failed\n");
    ident_destroy (selector);
    free (i);
    return (NULL);
  }

  ident_destroy (selector);

  i->files = NULL;
  i->files_num = 0;

  return (i);
} /* }}} graph_instance_t *inst_create */

void inst_destroy (graph_instance_t *inst) /* {{{ */
{
  size_t i;

  if (inst == NULL)
    return;

  ident_destroy (inst->select);

  for (i = 0; i < inst->files_num; i++)
    ident_destroy (inst->files[i]);
  free (inst->files);

  free (inst);
} /* }}} void inst_destroy */

int inst_add_file (graph_instance_t *inst, /* {{{ */
    const graph_ident_t *file)
{
  graph_ident_t **tmp;

  tmp = realloc (inst->files, sizeof (*inst->files) * (inst->files_num + 1));
  if (tmp == NULL)
    return (ENOMEM);
  inst->files = tmp;

  inst->files[inst->files_num] = ident_clone (file);
  if (inst->files[inst->files_num] == NULL)
    return (ENOMEM);

  inst->files_num++;

  return (0);
} /* }}} int inst_add_file */

graph_instance_t *inst_get_selected (graph_config_t *cfg) /* {{{ */
{
  graph_ident_t *ident;
  graph_instance_t *inst;

  if (cfg == NULL)
    cfg = gl_graph_get_selected ();

  if (cfg == NULL)
  {
    DEBUG ("inst_get_selected: cfg == NULL;\n");
    return (NULL);
  }

  ident = inst_get_selector_from_params ();
  if (ident == NULL)
  {
    fprintf (stderr, "inst_get_selected: ident_create failed\n");
    return (NULL);
  }

  inst = graph_inst_find_exact (cfg, ident);

  ident_destroy (ident);
  return (inst);
} /* }}} graph_instance_t *inst_get_selected */

int inst_get_all_selected (graph_config_t *cfg, /* {{{ */
    graph_inst_callback_t callback, void *user_data)
{
  graph_ident_t *ident;
  int status;

  if ((cfg == NULL) || (callback == NULL))
    return (EINVAL);

  ident = inst_get_selector_from_params ();
  if (ident == NULL)
  {
    fprintf (stderr, "inst_get_all_selected: "
        "inst_get_selector_from_params failed\n");
    return (EINVAL);
  }

  status = graph_inst_find_all_matching (cfg, ident, callback, user_data);

  ident_destroy (ident);
  return (status);
} /* }}} int inst_get_all_selected */

int inst_get_rrdargs (graph_config_t *cfg, /* {{{ */
    graph_instance_t *inst,
    rrd_args_t *args)
{
  def_callback_data_t data = { inst, args };
  graph_def_t *defs;
  int status;

  if ((cfg == NULL) || (inst == NULL) || (args == NULL))
    return (EINVAL);

  status = graph_get_rrdargs (cfg, inst, args);
  if (status != 0)
    return (status);

  defs = graph_get_defs (cfg);
  if (defs == NULL)
  {
    defs = inst_get_default_defs (cfg, inst);

    if (defs == NULL)
      return (-1);

    status = def_foreach (defs, gl_instance_get_rrdargs_cb, &data);

    def_destroy (defs);
  }
  else
  {
    status = def_foreach (defs, gl_instance_get_rrdargs_cb, &data);
  }

  return (status);
} /* }}} int inst_get_rrdargs */

/* Create one or more DEFs for each file in the graph instance. The number
 * depends on the number of data sources in each of the files. Called from
 * "inst_get_rrdargs" if no DEFs are available from the configuration.
 * */
graph_def_t *inst_get_default_defs (graph_config_t *cfg, /* {{{ */
    graph_instance_t *inst)
{
  graph_def_t *defs = NULL;
  size_t i;

  if ((cfg == NULL) || (inst == NULL))
    return (NULL);

  for (i = 0; i < inst->files_num; i++)
  {
    graph_def_t *def;

    def = ident_get_default_defs (cfg, inst->files[i], defs);
    if (def == NULL)
      continue;

    if (defs == NULL)
      defs = def;
    else
      def_append (defs, def);
  }

  return (defs);
} /* }}} graph_def_t *inst_get_default_defs */

graph_ident_t *inst_get_selector (graph_instance_t *inst) /* {{{ */
{
  if (inst == NULL)
    return (NULL);

  return (ident_clone (inst->select));
} /* }}} graph_ident_t *inst_get_selector */

int inst_get_params (graph_config_t *cfg, graph_instance_t *inst, /* {{{ */
    char *buffer, size_t buffer_size)
{
  graph_ident_t *cfg_select;

  if ((cfg == NULL) || (inst == NULL)
      || (buffer == NULL) || (buffer_size < 1))
    return (EINVAL);

  cfg_select = graph_get_selector (cfg);
  if (cfg_select == NULL)
  {
    fprintf (stderr, "inst_get_params: graph_get_selector failed");
    return (-1);
  }

  buffer[0] = 0;

#define COPY_ESCAPE(str) do {                                   \
  char tmp[1024];                                               \
  uri_escape_copy (tmp, (str), sizeof (tmp));                   \
  strlcat (buffer, tmp, buffer_size);                           \
} while (0)                                                     \

#define COPY_FIELD(field) do {                                  \
  const char *cfg_f  = ident_get_##field (cfg_select);          \
  const char *inst_f = ident_get_##field (inst->select);        \
  if (strcmp (cfg_f, inst_f) == 0)                              \
  {                                                             \
    strlcat (buffer, #field, buffer_size);                      \
    strlcat (buffer, "=", buffer_size);                         \
    COPY_ESCAPE (cfg_f);                                        \
  }                                                             \
  else                                                          \
  {                                                             \
    strlcat (buffer, "graph_", buffer_size);                    \
    strlcat (buffer, #field, buffer_size);                      \
    strlcat (buffer, "=", buffer_size);                         \
    COPY_ESCAPE (cfg_f);                                        \
    strlcat (buffer, ";", buffer_size);                         \
    strlcat (buffer, "inst_", buffer_size);                     \
    strlcat (buffer, #field, buffer_size);                      \
    strlcat (buffer, "=", buffer_size);                         \
    COPY_ESCAPE (inst_f);                                       \
  }                                                             \
} while (0)

  COPY_FIELD(host);
  strlcat (buffer, ";", buffer_size);
  COPY_FIELD(plugin);
  strlcat (buffer, ";", buffer_size);
  COPY_FIELD(plugin_instance);
  strlcat (buffer, ";", buffer_size);
  COPY_FIELD(type);
  strlcat (buffer, ";", buffer_size);
  COPY_FIELD(type_instance);

#undef COPY_FIELD
#undef COPY_ESCAPE

  ident_destroy (cfg_select);

  return (0);
} /* }}} int inst_get_params */

int inst_compare (const graph_instance_t *i0, /* {{{ */
    const graph_instance_t *i1)
{
  return (ident_compare (i0->select, i1->select));
} /* }}} int inst_compare */

int inst_compare_ident (graph_instance_t *inst, /* {{{ */
    const graph_ident_t *ident)
{
  if ((inst == NULL) || (ident == NULL))
    return (0);

  return (ident_compare (inst->select, ident));
} /* }}} int inst_compare_ident */

_Bool inst_ident_matches (graph_instance_t *inst, /* {{{ */
    const graph_ident_t *ident)
{
#if C4_DEBUG
  if ((inst == NULL) || (ident == NULL))
    return (0);
#endif

  return (ident_matches (inst->select, ident));
} /* }}} _Bool inst_ident_matches */

_Bool inst_matches_ident (graph_instance_t *inst, /* {{{ */
    const graph_ident_t *ident)
{
  if ((inst == NULL) || (ident == NULL))
    return (0);

  return (ident_matches (ident, inst->select));
} /* }}} _Bool inst_matches_ident */

_Bool inst_matches_string (graph_config_t *cfg, /* {{{ */
    graph_instance_t *inst,
    const char *term)
{
  char buffer[1024];
  int status;

  if ((cfg == NULL) || (inst == NULL) || (term == NULL))
    return (0);

  status = inst_describe (cfg, inst, buffer, sizeof (buffer));
  if (status != 0)
  {
    fprintf (stderr, "inst_matches_string: inst_describe failed\n");
    return (status);
  }

  strtolower (buffer);

  /* no match */
  if (strstr (buffer, term) == NULL)
    return (0);

  return (1);
} /* }}} _Bool inst_matches_string */

_Bool inst_matches_field (graph_instance_t *inst, /* {{{ */
    graph_ident_field_t field, const char *field_value)
{
  const char *selector_field;
  size_t i;

  if ((inst == NULL) || (field_value == NULL))
    return (0);

  selector_field = ident_get_field (inst->select, field);
  if (selector_field == NULL)
    return (0);

  assert (!IS_ANY (selector_field));
  if (!IS_ALL (selector_field))
  {
    if (strcasecmp (selector_field, field_value) == 0)
      return (1);
    else
      return (0);
  }

  /* The selector field is an ALL selector
   * => we need to check the files to see if the instance matches. */
  for (i = 0; i < inst->files_num; i++)
  {
    selector_field = ident_get_field (inst->files[i], field);
    if (selector_field == NULL)
      continue;

    assert (!IS_ANY (selector_field));
    assert (!IS_ALL (selector_field));

    if (strcasecmp (selector_field, field_value) == 0)
      return (1);
  } /* for files */

  return (0);
} /* }}} _Bool inst_matches_field */

int inst_to_json (const graph_instance_t *inst, /* {{{ */
    yajl_gen handler)
{
  size_t i;

  if ((inst == NULL) || (handler == NULL))
    return (EINVAL);

  /* TODO: error handling */
  yajl_gen_map_open (handler);
  yajl_gen_string (handler,
      (unsigned char *) "select",
      (unsigned int) strlen ("select"));
  ident_to_json (inst->select, handler);
  yajl_gen_string (handler,
      (unsigned char *) "files",
      (unsigned int) strlen ("files"));
  yajl_gen_array_open (handler);
  for (i = 0; i < inst->files_num; i++)
    ident_to_json (inst->files[i], handler);
  yajl_gen_array_close (handler);
  yajl_gen_map_close (handler);

  return (0);
} /* }}} int inst_to_json */

int inst_data_to_json (const graph_instance_t *inst, /* {{{ */
    dp_time_t begin, dp_time_t end,
    yajl_gen handler)
{
  size_t i;

  yajl_gen_array_open (handler);
  for (i = 0; i < inst->files_num; i++)
    ident_data_to_json (inst->files[i], begin, end, handler);
  yajl_gen_array_close (handler);

  return (0);
} /* }}} int inst_data_to_json */

int inst_describe (graph_config_t *cfg, graph_instance_t *inst, /* {{{ */
    char *buffer, size_t buffer_size)
{
  graph_ident_t *cfg_select;
  int status;

  if ((cfg == NULL) || (inst == NULL)
      || (buffer == NULL) || (buffer_size < 2))
    return (EINVAL);

  cfg_select = graph_get_selector (cfg);
  if (cfg_select == NULL)
  {
    fprintf (stderr, "inst_describe: graph_get_selector failed\n");
    return (-1);
  }

  status = ident_describe (inst->select, cfg_select,
      buffer, buffer_size);

  ident_destroy (cfg_select);

  return (status);
} /* }}} int inst_describe */

time_t inst_get_mtime (graph_instance_t *inst) /* {{{ */
{
  size_t i;
  time_t mtime;

  if (inst == NULL)
    return (0);

  mtime = 0;
  for (i = 0; i < inst->files_num; i++)
  {
    time_t tmp;

    tmp = ident_get_mtime (inst->files[i]);
    if (mtime < tmp)
      mtime = tmp;
  }

  return (mtime);
} /* }}} time_t inst_get_mtime */

/* vim: set sw=2 sts=2 et fdm=marker : */
