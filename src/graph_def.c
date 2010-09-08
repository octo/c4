/**
 * collection4 - graph_def.c
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

#include "graph_def.h"
#include "graph.h"
#include "graph_config.h"
#include "graph_ident.h"
#include "common.h"
#include "oconfig.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

/*
 * Data structures
 */
struct graph_def_s
{
  graph_ident_t *select;

  char *ds_name;
  char *legend;
  uint32_t color;
  _Bool stack;
  _Bool area;
  char *format;

  graph_def_t *next;
};

/*
 * Private functions
 */
#define DEF_CONFIG_FIELD(field) \
static int def_config_##field (const oconfig_item_t *ci, graph_ident_t *ident) \
{                                                                              \
  char *tmp = NULL;                                                            \
  int status = graph_config_get_string (ci, &tmp);                             \
  if (status != 0)                                                             \
    return (status);                                                           \
  ident_set_##field (ident, tmp);                                              \
  free (tmp);                                                                  \
  return (0);                                                                  \
} /* }}} int def_config_field */

DEF_CONFIG_FIELD (host);
DEF_CONFIG_FIELD (plugin);
DEF_CONFIG_FIELD (plugin_instance);
DEF_CONFIG_FIELD (type);
DEF_CONFIG_FIELD (type_instance);

#undef DEF_CONFIG_FIELD

static int def_config_color (const oconfig_item_t *ci, uint32_t *ret_color) /* {{{ */
{
  char *tmp;
  char *endptr;
  uint32_t color;

  if ((ci->values_num != 1) || (ci->values[0].type != OCONFIG_TYPE_STRING))
    return (EINVAL);

  tmp = ci->values[0].value.string;

  endptr = NULL;
  errno = 0;
  color = (uint32_t) strtoul (tmp, &endptr, /* base = */ 16);
  if ((errno != 0) || (endptr == tmp) || (color > 0x00ffffff))
    return (EINVAL);

  *ret_color = color;

  return (0);
} /* }}} int def_config_color */

static graph_def_t *def_config_get_obj (graph_config_t *cfg, /* {{{ */
    const oconfig_item_t *ci)
{
  graph_ident_t *ident;
  char *ds_name = NULL;
  graph_def_t *def;
  int i;

  ident = graph_get_selector (cfg);
  if (ident == NULL)
  {
    fprintf (stderr, "def_config_get_obj: graph_get_selector failed");
    return (NULL);
  }

  for (i = 0; i < ci->children_num; i++)
  {
    oconfig_item_t *child;

#define HANDLE_FIELD(name,field) \
    else if (strcasecmp (name, child->key) == 0) \
      def_config_##field (child, ident)

    child = ci->children + i;
    if (strcasecmp ("DSName", child->key) == 0)
      graph_config_get_string (child, &ds_name);

    HANDLE_FIELD ("Host", host);
    HANDLE_FIELD ("Plugin", plugin);
    HANDLE_FIELD ("PluginInstance", plugin_instance);
    HANDLE_FIELD ("Type", type);
    HANDLE_FIELD ("TypeInstance", type_instance);

#undef HANDLE_FIELD
  }

  def = def_create (cfg, ident, ds_name);
  if (def == NULL)
  {
    fprintf (stderr, "def_config_get_obj: def_create failed\n");
    ident_destroy (ident);
    free (ds_name);
    return (NULL);
  }

  ident_destroy (ident);
  free (ds_name);

  return (def);
} /* }}} graph_def_t *def_config_get_obj */

static int def_to_json_recursive (const graph_def_t *def, /* {{{ */
    yajl_gen handler)
{
  char color[16];

  if (def == NULL)
    return (0);

  snprintf (color, sizeof (color), "#%06"PRIx32, def->color);
  color[sizeof (color) - 1] = 0;

  yajl_gen_map_open (handler);

#define yajl_gen_string_cast(h,p,l) \
  yajl_gen_string (h, (unsigned char *) p, (unsigned int) l)

  yajl_gen_string_cast (handler, "select", strlen ("select"));
  ident_to_json (def->select, handler);
  yajl_gen_string_cast (handler, "ds_name", strlen ("ds_name"));
  yajl_gen_string_cast (handler, def->ds_name, strlen (def->ds_name));
  yajl_gen_string_cast (handler, "legend", strlen ("legend"));
  yajl_gen_string_cast (handler, def->legend, strlen (def->legend));
  yajl_gen_string_cast (handler, "color", strlen ("color"));
  yajl_gen_string_cast (handler, color, strlen (color));
  yajl_gen_string_cast (handler, "stack", strlen ("stack"));
  yajl_gen_bool   (handler, def->stack);
  yajl_gen_string_cast (handler, "area", strlen ("area"));
  yajl_gen_bool   (handler, def->area);
  yajl_gen_string_cast (handler, "format", strlen ("format"));
  yajl_gen_string_cast (handler, def->format, strlen (def->format));

  yajl_gen_map_close (handler);

  return (def_to_json_recursive (def->next, handler));
#undef yajl_gen_string_cast
} /* }}} int def_to_json_recursive */

/*
 * Public functions
 */
graph_def_t *def_create (graph_config_t *cfg, graph_ident_t *ident, /* {{{ */
    const char *ds_name)
{
  graph_ident_t *selector;
  graph_def_t *ret;

  if ((cfg == NULL) || (ident == NULL) || (ds_name == NULL))
  {
    fprintf (stderr, "def_create: An argument is NULL\n");
    return (NULL);
  }

  selector = graph_get_selector (cfg);
  if (selector == NULL)
  {
    fprintf (stderr, "def_create: graph_get_selector failed\n");
    return (NULL);
  }

  ret = malloc (sizeof (*ret));
  if (ret == NULL)
  {
    fprintf (stderr, "def_create: malloc failed\n");
    ident_destroy (selector);
    return (NULL);
  }
  memset (ret, 0, sizeof (*ret));
  ret->legend = NULL;
  ret->format = NULL;

  ret->ds_name = strdup (ds_name);
  if (ret->ds_name == NULL)
  {
    fprintf (stderr, "def_create: Unable to copy DS name\n");
    ident_destroy (selector);
    free (ret);
    return (NULL);
  }

  ret->color = UINT32_MAX;
  ret->next = NULL;

  ret->select = ident_copy_with_selector (selector, ident,
      IDENT_FLAG_REPLACE_ALL);
  if (ret->select == NULL)
  {
    fprintf (stderr, "def_create: ident_copy_with_selector failed\n");
    ident_destroy (selector);
    free (ret->ds_name);
    free (ret);
    return (NULL);
  }

  ident_destroy (selector);
  return (ret);
} /* }}} graph_def_t *def_create */

void def_destroy (graph_def_t *def) /* {{{ */
{
  graph_def_t *next;

  if (def == NULL)
    return;

  next = def->next;

  ident_destroy (def->select);

  free (def->ds_name);
  free (def->legend);
  free (def->format);

  free (def);

  def_destroy (next);
} /* }}} void def_destroy */

int def_config (graph_config_t *cfg, const oconfig_item_t *ci) /* {{{ */
{
  graph_def_t *def;
  int i;

  def = def_config_get_obj (cfg, ci);
  if (def == NULL)
    return (EINVAL);

  for (i = 0; i < ci->children_num; i++)
  {
    oconfig_item_t *child;

    child = ci->children + i;
    if (strcasecmp ("Legend", child->key) == 0)
      graph_config_get_string (child, &def->legend);
    else if (strcasecmp ("Color", child->key) == 0)
      def_config_color (child, &def->color);
    else if (strcasecmp ("Stack", child->key) == 0)
      graph_config_get_bool (child, &def->stack);
    else if (strcasecmp ("Area", child->key) == 0)
      graph_config_get_bool (child, &def->area);
    else if (strcasecmp ("Format", child->key) == 0)
      graph_config_get_string (child, &def->format);
  }

  return (graph_add_def (cfg, def));
} /* }}} int def_config */

int def_append (graph_def_t *head, graph_def_t *def) /* {{{ */
{
  graph_def_t *ptr;

  if ((head == NULL) || (def == NULL))
    return (EINVAL);

  ptr = head;
  while (ptr->next != NULL)
    ptr = ptr->next;

  ptr->next = def;

  return (0);
} /* }}} int def_append */

graph_def_t *def_search (graph_def_t *head, graph_ident_t *ident, /* {{{ */
    const char *ds_name)
{
  graph_def_t *ptr;

  if ((head == NULL) || (ident == NULL) || (ds_name == NULL))
    return (NULL);

  for (ptr = head; ptr != NULL; ptr = ptr->next)
  {
    if (!ident_matches (ptr->select, ident))
      continue;

    if (strcmp (ptr->ds_name, ds_name) == 0)
      return (ptr);
  }

  return (NULL);
} /* }}} graph_def_t *def_search */

_Bool def_matches (graph_def_t *def, graph_ident_t *ident) /* {{{ */
{
  return (ident_matches (def->select, ident));
} /* }}} _Bool def_matches */

int def_foreach (graph_def_t *def, def_callback_t callback, /* {{{ */
    void *user_data)
{
  graph_def_t *ptr;

  if ((def == NULL) || (callback == NULL))
    return (EINVAL);

  for (ptr = def; ptr != NULL; ptr = ptr->next)
  {
    int status;

    status = (*callback) (ptr, user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int def_foreach */

int def_get_rrdargs (graph_def_t *def, graph_ident_t *ident, /* {{{ */
    rrd_args_t *args)
{
  char *file;
  int index;
  char draw_def[64];
  char legend[256];
  uint32_t color;

  if ((def == NULL) || (ident == NULL) || (args == NULL))
    return (EINVAL);

  file = ident_to_file (ident);
  if (file == NULL)
  {
    DEBUG ("gl_ident_get_rrdargs: ident_to_file returned NULL.\n");
    return (-1);
  }

  DEBUG ("gl_ident_get_rrdargs: file = %s;\n", file);

  if (def->legend != NULL)
  {
    strncpy (legend, def->legend, sizeof (legend));
    legend[sizeof (legend) - 1] = 0;
  }
  else
  {
    ident_describe (ident, def->select,
        legend, sizeof (legend));

    if ((legend[0] == 0) || (strcmp ("default", legend) == 0))
    {
      strncpy (legend, def->ds_name, sizeof (legend));
      legend[sizeof (legend) - 1] = 0;
    }
  }

  color = def->color;
  if (color > 0x00ffffff)
    color = get_random_color ();

  index = args->index;
  args->index++;

  /* CDEFs */
  array_append_format (args->data, "DEF:def_%04i_min=%s:%s:MIN",
      index, file, def->ds_name);
  array_append_format (args->data, "DEF:def_%04i_avg=%s:%s:AVERAGE",
      index, file, def->ds_name);
  array_append_format (args->data, "DEF:def_%04i_max=%s:%s:MAX",
      index, file, def->ds_name);
  /* VDEFs */
  array_append_format (args->data, "VDEF:vdef_%04i_min=def_%04i_min,MINIMUM",
      index, index);
  array_append_format (args->data, "VDEF:vdef_%04i_avg=def_%04i_avg,AVERAGE",
      index, index);
  array_append_format (args->data, "VDEF:vdef_%04i_max=def_%04i_max,MAXIMUM",
      index, index);
  array_append_format (args->data, "VDEF:vdef_%04i_lst=def_%04i_avg,LAST",
      index, index);

  if (def->stack)
  {
    if (args->last_stack_cdef[0] != 0)
    {
      array_append_format (args->calc, "CDEF:cdef_%04i_stack=%s,def_%04i_avg,+",
          index, args->last_stack_cdef, index);
      snprintf (draw_def, sizeof (draw_def), "cdef_%04i_stack", index);
    }
    else
    {
      snprintf (draw_def, sizeof (draw_def), "def_%04i_avg", index);
    }
  }
  else
  {
    snprintf (draw_def, sizeof (draw_def), "def_%04i_avg", index);
  }

  if (def->area)
    array_prepend_format (args->areas, "AREA:%s#%06"PRIx32,
        draw_def, fade_color (color));

  /* Graph part */
  array_prepend_format (args->lines, "GPRINT:vdef_%04i_lst:%s last\\l",
      index, (def->format != NULL) ? def->format : "%6.2lf");
  array_prepend_format (args->lines, "GPRINT:vdef_%04i_max:%s max,",
      index, (def->format != NULL) ? def->format : "%6.2lf");
  array_prepend_format (args->lines, "GPRINT:vdef_%04i_avg:%s avg,",
      index, (def->format != NULL) ? def->format : "%6.2lf");
  array_prepend_format (args->lines, "GPRINT:vdef_%04i_min:%s min,",
      index, (def->format != NULL) ? def->format : "%6.2lf");
  array_prepend_format (args->lines, "LINE1:%s#%06"PRIx32":%s",
      draw_def, color, legend);

  free (file);

  memcpy (args->last_stack_cdef, draw_def, sizeof (args->last_stack_cdef));

  return (0);
} /* }}} int def_get_rrdargs */

int def_to_json (const graph_def_t *def, /* {{{ */
    yajl_gen handler)
{
  if (handler == NULL)
    return (EINVAL);

  yajl_gen_array_open (handler);
  def_to_json_recursive (def, handler);
  yajl_gen_array_close (handler);

  return (0);
} /* }}} int def_to_json */

/* vim: set sw=2 sts=2 et fdm=marker : */
