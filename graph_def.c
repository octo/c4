#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "graph_def.h"
#include "graph.h"
#include "graph_config.h"
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

  ident = gl_graph_get_selector (cfg);
  if (ident == NULL)
  {
    fprintf (stderr, "def_config_get_obj: gl_graph_get_selector failed");
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

/*
 * Public functions
 */
graph_def_t *def_create (graph_config_t *cfg, graph_ident_t *ident, /* {{{ */
    const char *ds_name)
{
  graph_ident_t *selector;
  graph_def_t *ret;

  if ((cfg == NULL) || (ident == NULL) || (ds_name == NULL))
    return (NULL);

  selector = gl_graph_get_selector (cfg);
  if (selector == NULL)
    return (NULL);

  ret = malloc (sizeof (*ret));
  if (ret == NULL)
  {
    ident_destroy (selector);
    return (NULL);
  }
  memset (ret, 0, sizeof (*ret));
  ret->legend = NULL;

  ret->ds_name = strdup (ds_name);
  if (ret->ds_name == NULL)
  {
    ident_destroy (selector);
    free (ret);
    return (NULL);
  }

  ret->color = get_random_color ();
  ret->next = NULL;

  ret->select = ident_copy_with_selector (selector, ident,
      IDENT_FLAG_REPLACE_ALL);
  if (ret->select == NULL)
  {
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
    else
      fprintf (stderr, "def_config: Ignoring unknown config option \"%s\"",
          child->key);
  }

  return (gl_graph_add_def (cfg, def));
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
    str_array_t *args)
{
  char *file;
  int index;

  if ((def == NULL) || (ident == NULL) || (args == NULL))
    return (EINVAL);

  file = ident_to_file (ident);
  if (file == NULL)
  {
    DEBUG ("gl_ident_get_rrdargs: ident_to_file returned NULL.\n");
    return (-1);
  }

  DEBUG ("gl_ident_get_rrdargs: file = %s;\n", file);

  index = array_argc (args);

  /* CDEFs */
  array_append_format (args, "DEF:def_%04i_min=%s:%s:MIN",
      index, file, def->ds_name);
  array_append_format (args, "DEF:def_%04i_avg=%s:%s:AVERAGE",
      index, file, def->ds_name);
  array_append_format (args, "DEF:def_%04i_max=%s:%s:MAX",
      index, file, def->ds_name);
  /* VDEFs */
  array_append_format (args, "VDEF:vdef_%04i_min=def_%04i_min,MINIMUM",
      index, index);
  array_append_format (args, "VDEF:vdef_%04i_avg=def_%04i_avg,AVERAGE",
      index, index);
  array_append_format (args, "VDEF:vdef_%04i_max=def_%04i_max,MAXIMUM",
      index, index);
  array_append_format (args, "VDEF:vdef_%04i_lst=def_%04i_avg,LAST",
      index, index);

  /* Graph part */
  array_append_format (args, "%s:def_%04i_avg#%06"PRIx32":%s%s",
      def->area ? "AREA" : "LINE1",
      index, def->color,
      (def->legend != NULL) ? def->legend : def->ds_name,
      def->stack ? ":STACK" : "");
  array_append_format (args, "GPRINT:vdef_%04i_min:%%lg min,", index);
  array_append_format (args, "GPRINT:vdef_%04i_avg:%%lg avg,", index);
  array_append_format (args, "GPRINT:vdef_%04i_max:%%lg max,", index);
  array_append_format (args, "GPRINT:vdef_%04i_lst:%%lg last\\l", index);

  free (file);

  return (0);
} /* }}} int def_get_rrdargs */

/* vim: set sw=2 sts=2 et fdm=marker : */
