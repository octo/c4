#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include "graph.h"
#include "graph_list.h"
#include "graph_ident.h"
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

  graph_instance_t *instances;
}; /* }}} struct graph_config_s */

/*
 * Private functions
 */
static graph_instance_t *graph_find_instance (graph_config_t *cfg, /* {{{ */
    const graph_ident_t *ident)
{
  if ((cfg == NULL) || (ident == NULL))
    return (NULL);

  return (inst_find_matching (cfg->instances, ident));
} /* }}} graph_instance_t *graph_find_instance */

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
  if (cfg == NULL)
    return;

  ident_destroy (cfg->select);

  free (cfg->title);
  free (cfg->vertical_label);

  def_destroy (cfg->defs);
  inst_destroy (cfg->instances);
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

  inst = graph_find_instance (cfg, file);
  if (inst == NULL)
  {
    inst = inst_create (cfg, file);
    if (inst == NULL)
      return (ENOMEM);

    if (cfg->instances == NULL)
      cfg->instances = inst;
    else
      inst_append (cfg->instances, inst);
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

graph_ident_t *graph_get_selector (graph_config_t *cfg) /* {{{ */
{
  if (cfg == NULL)
    return (NULL);

  return (ident_clone (cfg->select));
} /* }}} graph_ident_t *graph_get_selector */

graph_instance_t *graph_get_instances (graph_config_t *cfg) /* {{{ */
{
  if (cfg == NULL)
    return (NULL);

  return (cfg->instances);
} /* }}} graph_instance_t *graph_get_instances */

graph_def_t *graph_get_defs (graph_config_t *cfg) /* {{{ */
{
  if (cfg == NULL)
    return (NULL);

  return (cfg->defs);
} /* }}} graph_def_t *graph_get_defs */

int graph_add_def (graph_config_t *cfg, graph_def_t *def) /* {{{ */
{
  if ((cfg == NULL) || (def == NULL))
    return (EINVAL);

  if (cfg->defs == NULL)
  {
    cfg->defs = def;
    return (0);
  }

  return (def_append (cfg->defs, def));
} /* }}} int graph_add_def */

_Bool graph_matches (graph_config_t *cfg, const graph_ident_t *ident) /* {{{ */
{
  if ((cfg == NULL) || (ident == NULL))
    return (0);

  return (ident_matches (cfg->select, ident));
} /* }}} _Bool graph_matches */

struct graph_search_data_s
{
  graph_config_t *cfg;
  graph_inst_callback_t callback;
  void *user_data;
};
typedef struct graph_search_data_s graph_search_data_t;

static int graph_search_submit (graph_instance_t *inst, /* {{{ */
    void *user_data)
{
  graph_search_data_t *data = user_data;

  if ((inst == NULL) || (data == NULL))
    return (EINVAL);

  return ((*data->callback) (data->cfg, inst, data->user_data));
} /* }}} int graph_search_submit */

int graph_search (graph_config_t *cfg, const char *term, /* {{{ */
    graph_inst_callback_t callback,
    void *user_data)
{
  graph_search_data_t data = { cfg, callback, user_data };
  char buffer[1024];
  int status;

  status = graph_get_title (cfg, buffer, sizeof (buffer));
  if (status != 0)
  {
    fprintf (stderr, "graph_search: graph_get_title failed\n");
    return (status);
  }

  if (strstr (buffer, term) != NULL)
  {
    status = inst_foreach (cfg->instances, graph_search_submit, &data);
    if (status != 0)
      return (status);
  }
  else
  {
    status = inst_search (cfg, cfg->instances, term,
        graph_search_submit, &data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int graph_search */

int graph_compare (graph_config_t *cfg, const graph_ident_t *ident) /* {{{ */
{
  if ((cfg == NULL) || (ident == NULL))
    return (0);

  return (ident_compare (cfg->select, ident));
} /* }}} int graph_compare */

int graph_clear_instances (graph_config_t *cfg) /* {{{ */
{
  if (cfg == NULL)
    return (EINVAL);

  inst_destroy (cfg->instances);
  cfg->instances = NULL;

  return (0);
} /* }}} int graph_clear_instances */

int graph_get_rrdargs (graph_config_t *cfg, graph_instance_t *inst, /* {{{ */
    str_array_t *args)
{
  if ((cfg == NULL) || (inst == NULL) || (args == NULL))
    return (EINVAL);

  if (cfg->title != NULL)
  {
    array_append (args, "-t");
    array_append (args, cfg->title);
  }

  if (cfg->vertical_label != NULL)
  {
    array_append (args, "-v");
    array_append (args, cfg->vertical_label);
  }

  if (cfg->show_zero)
  {
    array_append (args, "-l");
    array_append (args, "0");
  }

  return (0);
} /* }}} int graph_get_rrdargs */

/* vim: set sw=2 sts=2 et fdm=marker : */
