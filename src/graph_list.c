/**
 * collection4 - graph_list.c
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
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "graph_list.h"
#include "common.h"
#include "filesystem.h"
#include "graph.h"
#include "graph_config.h"
#include "graph_def.h"
#include "graph_ident.h"
#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

/*
 * Defines
 */
#define UPDATE_INTERVAL 900

/*
 * Global variables
 */
static graph_config_t **gl_active = NULL;
static size_t gl_active_num = 0;

static graph_config_t **gl_staging = NULL;
static size_t gl_staging_num = 0;

/* Graphs created on-the-fly for files which don't match any existing graph
 * definition. */
static graph_config_t **gl_dynamic = NULL;
static size_t gl_dynamic_num = 0;

static char **host_list = NULL;
static size_t host_list_len = 0;

static time_t gl_last_update = 0;

/*
 * Private functions
 */
static int gl_add_graph_internal (graph_config_t *cfg, /* {{{ */
    graph_config_t ***gl_array, size_t *gl_array_num)
{
  graph_config_t **tmp;

#define ARRAY_PTR  (*gl_array)
#define ARRAY_SIZE (*gl_array_num)

  if (cfg == NULL)
    return (EINVAL);

  tmp = realloc (ARRAY_PTR, sizeof (*ARRAY_PTR) * (ARRAY_SIZE + 1));
  if (tmp == NULL)
    return (ENOMEM);
  ARRAY_PTR = tmp;

  ARRAY_PTR[ARRAY_SIZE] = cfg;
  ARRAY_SIZE++;

#undef ARRAY_SIZE
#undef ARRAY_PTR

  return (0);
} /* }}} int gl_add_graph_internal */

static void gl_destroy (graph_config_t ***gl_array, /* {{{ */
    size_t *gl_array_num)
{
  size_t i;

  if ((gl_array == NULL) || (gl_array_num == NULL))
    return;

#define ARRAY_PTR  (*gl_array)
#define ARRAY_SIZE (*gl_array_num)

  for (i = 0; i < ARRAY_SIZE; i++)
  {
    graph_destroy (ARRAY_PTR[i]);
    ARRAY_PTR[i] = NULL;
  }
  free (ARRAY_PTR);
  ARRAY_PTR = NULL;
  ARRAY_SIZE = 0;

#undef ARRAY_SIZE
#undef ARRAY_PTR
} /* }}} void gl_destroy */

static int gl_register_host (const char *host) /* {{{ */
{
  char **tmp;
  size_t i;

  if (host == NULL)
    return (EINVAL);

  for (i = 0; i < host_list_len; i++)
    if (strcmp (host_list[i], host) == 0)
      return (0);

  tmp = realloc (host_list, sizeof (*host_list) * (host_list_len + 1));
  if (tmp == NULL)
    return (ENOMEM);
  host_list = tmp;

  host_list[host_list_len] = strdup (host);
  if (host_list[host_list_len] == NULL)
    return (ENOMEM);

  host_list_len++;
  return (0);
} /* }}} int gl_register_host */

static int gl_clear_hosts (void) /* {{{ */
{
  size_t i;

  for (i = 0; i < host_list_len; i++)
    free (host_list[i]);
  free (host_list);

  host_list = NULL;
  host_list_len = 0;

  return (0);
} /* }}} int gl_clear_hosts */

static int gl_compare_hosts (const void *v0, const void *v1) /* {{{ */
{
  return (strcmp (*(char * const *) v0, *(char * const *) v1));
} /* }}} int gl_compare_hosts */

static int gl_register_file (const graph_ident_t *file, /* {{{ */
    __attribute__((unused)) void *user_data)
{
  graph_config_t *cfg;
  int num_graphs = 0;
  size_t i;

  for (i = 0; i < gl_active_num; i++)
  {
    graph_config_t *cfg = gl_active[i];
    int status;

    if (!graph_matches_ident (cfg, file))
      continue;

    status = graph_add_file (cfg, file);
    if (status != 0)
    {
      /* report error */;
    }
    else
    {
      num_graphs++;
    }
  }

  if (num_graphs == 0)
  {
    cfg = graph_create (file);
    gl_add_graph_internal (cfg, &gl_dynamic, &gl_dynamic_num);
    graph_add_file (cfg, file);
  }

  gl_register_host (ident_get_host (file));

  return (0);
} /* }}} int gl_register_file */

static const char *get_part_from_param (const char *prim_key, /* {{{ */
    const char *sec_key)
{
  const char *val;

  val = param (prim_key);
  if (val != NULL)
    return (val);
  
  return (param (sec_key));
} /* }}} const char *get_part_from_param */

static int gl_clear_instances (void) /* {{{ */
{
  size_t i;

  for (i = 0; i < gl_active_num; i++)
    graph_clear_instances (gl_active[i]);

  return (0);
} /* }}} int gl_clear_instances */

/*
 * Global functions
 */
int gl_add_graph (graph_config_t *cfg) /* {{{ */
{
  return (gl_add_graph_internal (cfg, &gl_staging, &gl_staging_num));
} /* }}} int gl_add_graph */

int gl_config_submit (void) /* {{{ */
{
  graph_config_t **old;
  size_t old_num;

  old = gl_active;
  old_num = gl_active_num;

  gl_active = gl_staging;
  gl_active_num = gl_staging_num;

  gl_staging = NULL;
  gl_staging_num = 0;

  gl_destroy (&old, &old_num);

  return (0);
} /* }}} int graph_config_submit */

int gl_graph_get_all (graph_callback_t callback, /* {{{ */
    void *user_data)
{
  size_t i;

  if (callback == NULL)
    return (EINVAL);

  gl_update ();

  for (i = 0; i < gl_active_num; i++)
  {
    int status;

    status = (*callback) (gl_active[i], user_data);
    if (status != 0)
      return (status);
  }

  for (i = 0; i < gl_dynamic_num; i++)
  {
    int status;

    status = (*callback) (gl_dynamic[i], user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int gl_graph_get_all */

graph_config_t *gl_graph_get_selected (void) /* {{{ */
{
  const char *host = get_part_from_param ("graph_host", "host");
  const char *plugin = get_part_from_param ("graph_plugin", "plugin");
  const char *plugin_instance = get_part_from_param ("graph_plugin_instance", "plugin_instance");
  const char *type = get_part_from_param ("graph_type", "type");
  const char *type_instance = get_part_from_param ("graph_type_instance", "type_instance");
  graph_ident_t *ident;
  size_t i;

  if ((host == NULL)
      || (plugin == NULL) || (plugin_instance == NULL)
      || (type == NULL) || (type_instance == NULL))
    return (NULL);

  ident = ident_create (host, plugin, plugin_instance, type, type_instance);

  gl_update ();

  for (i = 0; i < gl_active_num; i++)
  {
    if (graph_compare (gl_active[i], ident) != 0)
      continue;

    ident_destroy (ident);
    return (gl_active[i]);
  }

  for (i = 0; i < gl_dynamic_num; i++)
  {
    if (graph_compare (gl_dynamic[i], ident) != 0)
      continue;

    ident_destroy (ident);
    return (gl_dynamic[i]);
  }

  ident_destroy (ident);
  return (NULL);
} /* }}} graph_config_t *gl_graph_get_selected */

/* gl_instance_get_all, gl_graph_instance_get_all {{{ */
struct gl_inst_callback_data /* {{{ */
{
  graph_config_t *cfg;
  graph_inst_callback_t callback;
  void *user_data;
}; /* }}} struct gl_inst_callback_data */

static int gl_inst_callback_handler (graph_instance_t *inst, /* {{{ */
    void *user_data)
{
  struct gl_inst_callback_data *data = user_data;

  return ((*data->callback) (data->cfg, inst, data->user_data));
} /* }}} int gl_inst_callback_handler */

int gl_graph_instance_get_all (graph_config_t *cfg, /* {{{ */
    graph_inst_callback_t callback, void *user_data)
{
  struct gl_inst_callback_data data =
  {
    cfg,
    callback,
    user_data
  };

  if ((cfg == NULL) || (callback == NULL))
    return (EINVAL);

  return (graph_inst_foreach (cfg, gl_inst_callback_handler, &data));
} /* }}} int gl_graph_instance_get_all */

int gl_instance_get_all (graph_inst_callback_t callback, /* {{{ */
    void *user_data)
{
  size_t i;

  gl_update ();

  for (i = 0; i < gl_active_num; i++)
  {
    int status;

    status = gl_graph_instance_get_all (gl_active[i], callback, user_data);
    if (status != 0)
      return (status);
  }

  for (i = 0; i < gl_dynamic_num; i++)
  {
    int status;

    status = gl_graph_instance_get_all (gl_dynamic[i], callback, user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int gl_instance_get_all */
/* }}} gl_instance_get_all, gl_graph_instance_get_all */

int gl_search_string (const char *term, graph_inst_callback_t callback, /* {{{ */
    void *user_data)
{
  size_t i;

  for (i = 0; i < gl_active_num; i++)
  {
    int status;

    status = graph_inst_search (gl_active[i], term,
        /* callback  = */ callback,
        /* user data = */ user_data);
    if (status != 0)
      return (status);
  }

  for (i = 0; i < gl_dynamic_num; i++)
  {
    int status;

    status = graph_inst_search (gl_dynamic[i], term,
        /* callback  = */ callback,
        /* user data = */ user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int gl_search_string */

int gl_search_field (graph_ident_field_t field, /* {{{ */
    const char *field_value,
    graph_inst_callback_t callback, void *user_data)
{
  size_t i;

  if ((field_value == NULL) || (callback == NULL))
    return (EINVAL);

  for (i = 0; i < gl_active_num; i++)
  {
    int status;

    status = graph_inst_search_field (gl_active[i],
        field, field_value,
        /* callback  = */ callback,
        /* user data = */ user_data);
    if (status != 0)
      return (status);
  }

  for (i = 0; i < gl_dynamic_num; i++)
  {
    int status;

    status = graph_inst_search_field (gl_dynamic[i],
        field, field_value,
        /* callback  = */ callback,
        /* user data = */ user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int gl_search_field */

int gl_foreach_host (int (*callback) (const char *host, void *user_data), /* {{{ */
    void *user_data)
{
  int status;
  size_t i;

  for (i = 0; i < host_list_len; i++)
  {
    status = (*callback) (host_list[i], user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int gl_foreach_host */

int gl_update (void) /* {{{ */
{
  time_t now;
  int status;
  size_t i;

  /*
  printf ("Content-Type: text/plain\n\n");
  */

  now = time (NULL);

  if ((gl_last_update + UPDATE_INTERVAL) >= now)
    return (0);

  /* Clear state */
  gl_clear_instances ();
  gl_clear_hosts ();
  gl_destroy (&gl_dynamic, &gl_dynamic_num);

  graph_read_config ();

  status = fs_scan (/* callback = */ gl_register_file, /* user data = */ NULL);

  if (host_list_len > 0)
    qsort (host_list, host_list_len, sizeof (*host_list),
        gl_compare_hosts);

  gl_last_update = now;

  for (i = 0; i < gl_active_num; i++)
    graph_sort_instances (gl_active[i]);

  return (status);
} /* }}} int gl_update */

/* vim: set sw=2 sts=2 et fdm=marker : */
