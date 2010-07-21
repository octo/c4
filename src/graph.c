/**
 * collection4 - graph.c
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
#include <assert.h>

#include "graph.h"
#include "graph_ident.h"
#include "graph_instance.h"
#include "graph_list.h"
#include "graph_def.h"
#include "graph_config.h"
#include "common.h"
#include "filesystem.h"
#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

/*
 * Data types
 */
struct graph_config_s /* {{{ */
{
  graph_ident_t *select;

  char *title;
  char *vertical_label;
  _Bool show_zero;

  graph_def_t *defs;

  graph_instance_t **instances;
  size_t instances_num;
}; /* }}} struct graph_config_s */

/*
 * Private functions
 */

/*
 * Config functions
 */
static graph_ident_t *graph_config_get_selector (const oconfig_item_t *ci) /* {{{ */
{
  char *host = NULL;
  char *plugin = NULL;
  char *plugin_instance = NULL;
  char *type = NULL;
  char *type_instance = NULL;
  graph_ident_t *ret;
  int i;

  for (i = 0; i < ci->children_num; i++)
  {
    oconfig_item_t *child;

    child = ci->children + i;

    if (strcasecmp ("Host", child->key) == 0)
      graph_config_get_string (child, &host);
    else if (strcasecmp ("Plugin", child->key) == 0)
      graph_config_get_string (child, &plugin);
    else if (strcasecmp ("PluginInstance", child->key) == 0)
      graph_config_get_string (child, &plugin_instance);
    else if (strcasecmp ("Type", child->key) == 0)
      graph_config_get_string (child, &type);
    else if (strcasecmp ("TypeInstance", child->key) == 0)
      graph_config_get_string (child, &type_instance);
    /* else: ignore all other directives here. */
  } /* for */

  ret = ident_create (host, plugin, plugin_instance, type, type_instance);

  free (host);
  free (plugin);
  free (plugin_instance);
  free (type);
  free (type_instance);

  return (ret);
} /* }}} int graph_config_get_selector */

/*
 * Global functions
 */
graph_config_t *graph_create (const graph_ident_t *selector) /* {{{ */
{
  graph_config_t *cfg;

  cfg = malloc (sizeof (*cfg));
  if (cfg == NULL)
    return (NULL);
  memset (cfg, 0, sizeof (*cfg));

  if (selector != NULL)
    cfg->select = ident_clone (selector);
  else
    cfg->select = NULL;

  cfg->title = NULL;
  cfg->vertical_label = NULL;
  cfg->defs = NULL;
  cfg->instances = NULL;

  return (cfg);
} /* }}} int graph_create */

void graph_destroy (graph_config_t *cfg) /* {{{ */
{
  size_t i;

  if (cfg == NULL)
    return;

  ident_destroy (cfg->select);

  free (cfg->title);
  free (cfg->vertical_label);

  def_destroy (cfg->defs);

  for (i = 0; i < cfg->instances_num; i++)
    inst_destroy (cfg->instances[i]);
  free (cfg->instances);
} /* }}} void graph_destroy */

int graph_config_add (const oconfig_item_t *ci) /* {{{ */
{
  graph_ident_t *select;
  graph_config_t *cfg = NULL;
  int i;

  select = graph_config_get_selector (ci);
  if (select == NULL)
    return (EINVAL);

  cfg = graph_create (/* selector = */ NULL);
  if (cfg == NULL)
    return (ENOMEM);

  cfg->select = select;

  for (i = 0; i < ci->children_num; i++)
  {
    oconfig_item_t *child;

    child = ci->children + i;

    if (strcasecmp ("Title", child->key) == 0)
      graph_config_get_string (child, &cfg->title);
    else if (strcasecmp ("VerticalLabel", child->key) == 0)
      graph_config_get_string (child, &cfg->vertical_label);
    else if (strcasecmp ("ShowZero", child->key) == 0)
      graph_config_get_bool (child, &cfg->show_zero);
    else if (strcasecmp ("DEF", child->key) == 0)
      def_config (cfg, child);
  } /* for */

  gl_add_graph (cfg);

  return (0);
} /* }}} graph_config_add */

int graph_add_file (graph_config_t *cfg, const graph_ident_t *file) /* {{{ */
{
  graph_instance_t *inst;

  inst = graph_inst_find_matching (cfg, file);
  if (inst == NULL)
  {
    graph_instance_t **tmp;

    tmp = realloc (cfg->instances,
        sizeof (*cfg->instances) * (cfg->instances_num + 1));
    if (tmp == NULL)
      return (ENOMEM);
    cfg->instances = tmp;

    inst = inst_create (cfg, file);
    if (inst == NULL)
      return (ENOMEM);

    cfg->instances[cfg->instances_num] = inst;
    cfg->instances_num++;
  }

  return (inst_add_file (inst, file));
} /* }}} int graph_add_file */

int graph_get_title (graph_config_t *cfg, /* {{{ */
    char *buffer, size_t buffer_size)
{
  if ((cfg == NULL) || (buffer == NULL) || (buffer_size < 1))
    return (EINVAL);

  if (cfg->title == NULL)
    cfg->title = ident_to_string (cfg->select);

  if (cfg->title == NULL)
    return (ENOMEM);

  strncpy (buffer, cfg->title, buffer_size);
  buffer[buffer_size - 1] = 0;

  return (0);
} /* }}} int graph_get_title */

int graph_get_params (graph_config_t *cfg, /* {{{ */
    char *buffer, size_t buffer_size)
{
  buffer[0] = 0;

#define COPY_FIELD(field) do {                                       \
  const char *str = ident_get_##field (cfg->select);                 \
  char uri_str[1024];                                                \
  uri_escape_copy (uri_str, str, sizeof (uri_str));                  \
  strlcat (buffer, #field, buffer_size);                             \
  strlcat (buffer, "=", buffer_size);                                \
  strlcat (buffer, uri_str, buffer_size);                            \
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

  return (0);
} /* }}} int graph_get_params */

graph_ident_t *graph_get_selector (graph_config_t *cfg) /* {{{ */
{
  if (cfg == NULL)
    return (NULL);

  return (ident_clone (cfg->select));
} /* }}} graph_ident_t *graph_get_selector */

graph_def_t *graph_get_defs (graph_config_t *cfg) /* {{{ */
{
  if (cfg == NULL)
    return (NULL);

  return (cfg->defs);
} /* }}} graph_def_t *graph_get_defs */

int graph_add_def (graph_config_t *cfg, graph_def_t *def) /* {{{ */
{
  graph_def_t *tmp;

  if ((cfg == NULL) || (def == NULL))
    return (EINVAL);

  if (cfg->defs == NULL)
  {
    cfg->defs = def;
    return (0);
  }

  /* Insert in reverse order. This makes the order in the config file and the
   * order of the DEFs in the graph more natural. Really. */
  tmp = cfg->defs;
  cfg->defs = def;
  return (def_append (cfg->defs, tmp));
} /* }}} int graph_add_def */

_Bool graph_ident_matches (graph_config_t *cfg, const graph_ident_t *ident) /* {{{ */
{
#if C4_DEBUG
  if ((cfg == NULL) || (ident == NULL))
    return (0);
#endif

  return (ident_matches (cfg->select, ident));
} /* }}} _Bool graph_ident_matches */

_Bool graph_matches_ident (graph_config_t *cfg, /* {{{ */
    const graph_ident_t *selector)
{
#if C4_DEBUG
  if ((cfg == NULL) || (selector == NULL))
    return (0);
#endif

  return (ident_matches (selector, cfg->select));
} /* }}} _Bool graph_matches_ident */

_Bool graph_ident_intersect (graph_config_t *cfg, /* {{{ */
    const graph_ident_t *selector)
{
#if C4_DEBUG
  if ((cfg == NULL) || (selector == NULL))
    return (0);
#endif

  return (ident_intersect (cfg->select, selector));
} /* }}} _Bool graph_ident_intersect */

_Bool graph_matches_field (graph_config_t *cfg, /* {{{ */
    graph_ident_field_t field, const char *field_value)
{
  const char *selector_value;

  if ((cfg == NULL) || (field_value == NULL))
    return (0);

  selector_value = ident_get_field (cfg->select, field);
  if (selector_value == NULL)
    return (0);

  if (IS_ALL (selector_value) || IS_ANY (selector_value))
    return (1);
  else if (strcasecmp (selector_value, field_value) == 0)
    return (1);

  return (0);
} /* }}} _Bool graph_matches_field */

int graph_inst_foreach (graph_config_t *cfg, /* {{{ */
		inst_callback_t cb, void *user_data)
{
  size_t i;
  int status;

  for (i = 0; i < cfg->instances_num; i++)
  {
    status = (*cb) (cfg->instances[i], user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int graph_inst_foreach */

graph_instance_t *graph_inst_find_exact (graph_config_t *cfg, /* {{{ */
    graph_ident_t *ident)
{
  size_t i;

  if ((cfg == NULL) || (ident == NULL))
    return (NULL);

  for (i = 0; i < cfg->instances_num; i++)
    if (inst_compare_ident (cfg->instances[i], ident) == 0)
      return (cfg->instances[i]);

  return (NULL);
} /* }}} graph_instance_t *graph_inst_find_exact */

graph_instance_t *graph_inst_find_matching (graph_config_t *cfg, /* {{{ */
    const graph_ident_t *ident)
{
  size_t i;

  if ((cfg == NULL) || (ident == NULL))
    return (NULL);

  for (i = 0; i < cfg->instances_num; i++)
    if (inst_ident_matches (cfg->instances[i], ident))
      return (cfg->instances[i]);

  return (NULL);
} /* }}} graph_instance_t *graph_inst_find_matching */

int graph_inst_find_all_matching (graph_config_t *cfg, /* {{{ */
    const graph_ident_t *ident,
    graph_inst_callback_t callback, void *user_data)
{
  size_t i;

  if ((cfg == NULL) || (ident == NULL) || (callback == NULL))
    return (EINVAL);

  for (i = 0; i < cfg->instances_num; i++)
  {
    int status;

    if (!inst_matches_ident (cfg->instances[i], ident))
      continue;

    status = (*callback) (cfg, cfg->instances[i], user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int graph_inst_find_all_matching */

/* Lightweight variant of the "graph_search_inst" which is used if the
 * search_info_t doesn't select any field values explicitely. */
static int graph_search_inst_noselector (graph_config_t *cfg, /* {{{ */
    search_info_t *si, graph_inst_callback_t cb, void *user_data)
{
  char title[1024];
  int status;
  size_t i;

  /* parameters have already been checked in "graph_search_inst" */

  status = graph_get_title (cfg, title, sizeof (title));
  if (status != 0)
  {
    fprintf (stderr, "graph_search_inst_noselector: "
        "graph_get_title failed\n");
    return (status);
  }
  strtolower (title);

  for (i = 0; i < cfg->instances_num; i++)
  {
    if (search_graph_inst_matches (si, cfg, cfg->instances[i], title))
    {
      status = (*cb) (cfg, cfg->instances[i], user_data);
      if (status != 0)
        return (status);
    }
  } /* for (cfg->instances_num) */

  return (0);
} /* }}} int graph_search_inst_noselector */

/* When this function is called from graph_list, it will already have checked
 * that the selector of the graph does not contradict the field selections contained in
 * the search_info_t. We still have to check if the instance contradicts the
 * search parameters, though, since the "ANY" wildcard is filled in now -
 * possibly with contradicting values. */
int graph_search_inst (graph_config_t *cfg, search_info_t *si, /* {{{ */
    graph_inst_callback_t cb,
    void *user_data)
{
  char title[1024];
  int status;
  size_t i;
  graph_ident_t *search_selector;

  if ((cfg == NULL) || (si == NULL) || (cb == NULL))
    return (EINVAL);

  if (!search_has_selector (si))
    return (graph_search_inst_noselector (cfg, si, cb, user_data));

  search_selector = search_to_ident (si);
  if (search_selector == NULL)
    return (ENOMEM);

  status = graph_get_title (cfg, title, sizeof (title));
  if (status != 0)
  {
    ident_destroy (search_selector);
    fprintf (stderr, "graph_search_inst: graph_get_title failed\n");
    return (status);
  }
  strtolower (title);

  for (i = 0; i < cfg->instances_num; i++)
  {
    graph_ident_t *inst_selector;

    inst_selector = inst_get_selector (cfg->instances[i]);
    if (inst_selector == NULL)
      continue;

    /* If the two selectors contradict one another, there is no point in
     * calling the (more costly) "search_graph_inst_matches" function. */
    if (!ident_intersect (search_selector, inst_selector))
    {
      ident_destroy (inst_selector);
      continue;
    }

    if (search_graph_inst_matches (si, cfg, cfg->instances[i], title))
    {
      status = (*cb) (cfg, cfg->instances[i], user_data);
      if (status != 0)
      {
        ident_destroy (search_selector);
        ident_destroy (inst_selector);
        return (status);
      }
    }

    ident_destroy (inst_selector);
  } /* for (cfg->instances_num) */

  ident_destroy (search_selector);
  return (0);
} /* }}} int graph_search_inst */

int graph_search_inst_string (graph_config_t *cfg, const char *term, /* {{{ */
    graph_inst_callback_t cb,
    void *user_data)
{
  char buffer[1024];
  int status;
  size_t i;

  status = graph_get_title (cfg, buffer, sizeof (buffer));
  if (status != 0)
  {
    fprintf (stderr, "graph_search_inst_string: graph_get_title failed\n");
    return (status);
  }

  strtolower (buffer);

  if (strstr (buffer, term) != NULL)
  {
    for (i = 0; i < cfg->instances_num; i++)
    {
      status = (*cb) (cfg, cfg->instances[i], user_data);
      if (status != 0)
        return (status);
    }
  }
  else
  {
    for (i = 0; i < cfg->instances_num; i++)
    {
      if (inst_matches_string (cfg, cfg->instances[i], term))
      {
        status = (*cb) (cfg, cfg->instances[i], user_data);
        if (status != 0)
          return (status);
      }
    }
  }

  return (0);
} /* }}} int graph_search_inst_string */

int graph_inst_search_field (graph_config_t *cfg, /* {{{ */
    graph_ident_field_t field, const char *field_value,
    graph_inst_callback_t callback, void *user_data)
{
  size_t i;
  const char *selector_field;
  _Bool need_check_instances = 0;

  if ((cfg == NULL) || (field_value == NULL) || (callback == NULL))
    return (EINVAL);

  if (!graph_matches_field (cfg, field, field_value))
    return (0);

  selector_field = ident_get_field (cfg->select, field);
  if (selector_field == NULL)
    return (-1);

  if (IS_ALL (selector_field) || IS_ANY (selector_field))
    need_check_instances = 1;

  for (i = 0; i < cfg->instances_num; i++)
  {
    int status;

    if (need_check_instances
        && !inst_matches_field (cfg->instances[i], field, field_value))
      continue;

    status = (*callback) (cfg, cfg->instances[i], user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int graph_inst_search_field */

int graph_compare (graph_config_t *cfg, const graph_ident_t *ident) /* {{{ */
{
  if ((cfg == NULL) || (ident == NULL))
    return (0);

  return (ident_compare (cfg->select, ident));
} /* }}} int graph_compare */

size_t graph_num_instances (graph_config_t *cfg) /* {{{ */
{
  if (cfg == NULL)
    return ((size_t) -1);

  return (cfg->instances_num);
} /* }}} size_t graph_num_instances */

int graph_to_json (const graph_config_t *cfg,
    yajl_gen handler)
{
  size_t i;

  if ((cfg == NULL) || (handler == NULL))
    return (EINVAL);

  yajl_gen_map_open (handler);
  yajl_gen_string (handler,
      (unsigned char *) "select",
      (unsigned int) strlen ("select"));
  ident_to_json (cfg->select, handler);
  yajl_gen_string (handler,
      (unsigned char *) "instances",
      (unsigned int) strlen ("instances"));
  yajl_gen_array_open (handler);
  for (i = 0; i < cfg->instances_num; i++)
    inst_to_json (cfg->instances[i], handler);
  yajl_gen_array_close (handler);
  yajl_gen_map_close (handler);

  return (0);
}

static int graph_sort_instances_cb (const void *v0, const void *v1) /* {{{ */
{
  return (inst_compare (*(graph_instance_t * const *) v0,
        *(graph_instance_t * const *) v1));
} /* }}} int graph_sort_instances_cb */

int graph_sort_instances (graph_config_t *cfg) /* {{{ */
{
  if (cfg == NULL)
    return (EINVAL);

  if (cfg->instances_num < 2)
    return (0);

  qsort (cfg->instances, cfg->instances_num, sizeof (*cfg->instances),
      graph_sort_instances_cb);

  return (0);
} /* }}} int graph_sort_instances */

int graph_clear_instances (graph_config_t *cfg) /* {{{ */
{
  size_t i;

  if (cfg == NULL)
    return (EINVAL);

  for (i = 0; i < cfg->instances_num; i++)
    inst_destroy (cfg->instances[i]);
  free (cfg->instances);
  cfg->instances = NULL;
  cfg->instances_num = 0;

  return (0);
} /* }}} int graph_clear_instances */

int graph_get_rrdargs (graph_config_t *cfg, graph_instance_t *inst, /* {{{ */
    rrd_args_t *args)
{
  if ((cfg == NULL) || (inst == NULL) || (args == NULL))
    return (EINVAL);

  if (cfg->title != NULL)
  {
    array_append (args->options, "-t");
    array_append (args->options, cfg->title);
  }

  if (cfg->vertical_label != NULL)
  {
    array_append (args->options, "-v");
    array_append (args->options, cfg->vertical_label);
  }

  if (cfg->show_zero)
  {
    array_append (args->options, "-l");
    array_append (args->options, "0");
  }

  return (0);
} /* }}} int graph_get_rrdargs */

/* vim: set sw=2 sts=2 et fdm=marker : */
