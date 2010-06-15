#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "graph_instance.h"
#include "graph_ident.h"
#include "common.h"
#include "utils_params.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

struct graph_instance_s /* {{{ */
{
  graph_ident_t *select;

  graph_ident_t **files;
  size_t files_num;

  graph_instance_t *next;
}; /* }}} struct graph_instance_s */

struct def_callback_data_s
{
  graph_instance_t *inst;
  str_array_t *args;
};
typedef struct def_callback_data_s def_callback_data_t;

/*
 * Private functions
 */
/* Create one DEF for each data source in the file. Called by
 * "inst_get_default_defs" for each file. */
static graph_def_t *ident_get_default_defs (graph_config_t *cfg, /* {{{ */
    graph_ident_t *ident, graph_def_t *def_head)
{
  graph_def_t *defs = NULL;
  char *file;
  char **dses = NULL;
  size_t dses_num = 0;
  int status;
  size_t i;

  if ((cfg == NULL) || (ident == NULL))
    return (def_head);

  file = ident_to_file (ident);
  if (file == NULL)
  {
    fprintf (stderr, "ident_get_default_defs: ident_to_file failed\n");
    return (def_head);
  }

  status = ds_list_from_rrd_file (file, &dses_num, &dses);
  if (status != 0)
  {
    free (file);
    return (def_head);
  }

  for (i = 0; i < dses_num; i++)
  {
    graph_def_t *def;

    def = def_search (def_head, ident, dses[i]);
    if (def != NULL)
      continue;

    def = def_create (cfg, ident, dses[i]);
    if (def == NULL)
      continue;

    if (defs == NULL)
      defs = def;
    else
      def_append (defs, def);

    free (dses[i]);
  }

  free (dses);
  free (file);

  return (defs);
} /* }}} int ident_get_default_defs */

/* Create one or more DEFs for each file in the graph instance. The number
 * depends on the number of data sources in each of the files. Called from
 * "inst_get_rrdargs" if no DEFs are available from the configuration.
 * */
static graph_def_t *inst_get_default_defs (graph_config_t *cfg, /* {{{ */
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

/* Called with each DEF in turn. Calls "def_get_rrdargs" with every appropriate
 * file / DEF pair. */
static int gl_instance_get_rrdargs_cb (graph_def_t *def, void *user_data) /* {{{ */
{
  def_callback_data_t *data = user_data;
  graph_instance_t *inst = data->inst;
  str_array_t *args = data->args;

  size_t i;

  for (i = 0; i < inst->files_num; i++)
  {
    if (!def_matches (def, inst->files[i]))
      continue;

    def_get_rrdargs (def, inst->files[i], args);
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

  selector = gl_graph_get_selector (cfg);
  if (selector == NULL)
  {
    fprintf (stderr, "inst_create: gl_graph_get_selector failed\n");
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

  i->next = NULL;

  return (i);
} /* }}} graph_instance_t *inst_create */

void inst_destroy (graph_instance_t *inst) /* {{{ */
{
  graph_instance_t *next;
  size_t i;

  if (inst == NULL)
    return;

  next = inst->next;

  ident_destroy (inst->select);

  for (i = 0; i < inst->files_num; i++)
    ident_destroy (inst->files[i]);
  free (inst->files);

  free (inst);

  inst_destroy (next);
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
  const char *host = get_part_from_param ("inst_host", "host");
  const char *plugin = get_part_from_param ("inst_plugin", "plugin");
  const char *plugin_instance = get_part_from_param ("inst_plugin_instance", "plugin_instance");
  const char *type = get_part_from_param ("inst_type", "type");
  const char *type_instance = get_part_from_param ("inst_type_instance", "type_instance");
  graph_ident_t *ident;
  graph_instance_t *inst;

  if (cfg == NULL)
    cfg = graph_get_selected ();

  if (cfg == NULL)
  {
    DEBUG ("inst_get_selected: cfg == NULL;\n");
    return (NULL);
  }

  if ((host == NULL)
      || (plugin == NULL) || (plugin_instance == NULL)
      || (type == NULL) || (type_instance == NULL))
  {
    DEBUG ("inst_get_selected: A parameter is NULL.\n");
    return (NULL);
  }

  ident = ident_create (host, plugin, plugin_instance, type, type_instance);

  for (inst = gl_graph_get_instances (cfg); inst != NULL; inst = inst->next)
  {
    if (ident_compare (ident, inst->select) != 0)
      continue;

    ident_destroy (ident);
    return (inst);
  }

  DEBUG ("inst_get_selected: No match found.\n");
  ident_destroy (ident);
  return (NULL);
} /* }}} graph_instance_t *inst_get_selected */

int inst_get_rrdargs (graph_config_t *cfg, /* {{{ */
    graph_instance_t *inst,
    str_array_t *args)
{
  def_callback_data_t data = { inst, args };
  graph_def_t *defs;
  int status;

  if ((cfg == NULL) || (inst == NULL) || (args == NULL))
    return (EINVAL);

/* FIXME: Re-enable title and vertical label stuff. */
#if 0
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
#endif

  defs = gl_graph_get_defs (cfg);
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

  cfg_select = gl_graph_get_selector (cfg);
  if (cfg_select == NULL)
  {
    fprintf (stderr, "inst_get_params: gl_graph_get_selector failed");
    return (-1);
  }

  buffer[0] = 0;

#define COPY_FIELD(field) do {                                  \
  const char *cfg_f  = ident_get_##field (cfg_select);          \
  const char *inst_f = ident_get_##field (inst->select);        \
  if (strcmp (cfg_f, inst_f) == 0)                              \
  {                                                             \
    strlcat (buffer, #field, buffer_size);                      \
    strlcat (buffer, "=", buffer_size);                         \
    strlcat (buffer, cfg_f, buffer_size);                       \
  }                                                             \
  else                                                          \
  {                                                             \
    strlcat (buffer, "graph_", buffer_size);                    \
    strlcat (buffer, #field, buffer_size);                      \
    strlcat (buffer, "=", buffer_size);                         \
    strlcat (buffer, cfg_f, buffer_size);                       \
    strlcat (buffer, ";", buffer_size);                         \
    strlcat (buffer, "inst_", buffer_size);                     \
    strlcat (buffer, #field, buffer_size);                      \
    strlcat (buffer, "=", buffer_size);                         \
    strlcat (buffer, inst_f, buffer_size);                      \
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

  ident_destroy (cfg_select);

  return (0);
} /* }}} int inst_get_params */

int inst_append (graph_instance_t *head, graph_instance_t *inst) /* {{{ */
{
  graph_instance_t *ptr;

  if ((head == NULL) || (inst == NULL))
    return (EINVAL);

  ptr = head;
  while (ptr->next != NULL)
    ptr = ptr->next;

  ptr->next = inst;

  return (0);
} /* }}} int inst_append */

int inst_foreach (graph_instance_t *inst, /* {{{ */
		inst_callback_t cb, void *user_data)
{
  graph_instance_t *ptr;

  if ((inst == NULL) || (cb == NULL))
    return (EINVAL);

  for (ptr = inst; ptr != NULL; ptr = ptr->next)
  {
    int status;

    status = (*cb) (ptr, user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int inst_foreach */

graph_instance_t *inst_find_matching (graph_instance_t *inst, /* {{{ */
    const graph_ident_t *ident)
{
  graph_instance_t *ptr;

  if ((inst == NULL) || (ident == NULL))
    return (NULL);

  for (ptr = inst; ptr != NULL; ptr = ptr->next)
    if (ident_matches (ptr->select, ident))
      return (ptr);

  return (NULL);
} /* }}} graph_instance_t *inst_find_matching */

/* vim: set sw=2 sts=2 et fdm=marker : */
