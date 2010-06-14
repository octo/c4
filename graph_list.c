#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include "graph_list.h"
#include "graph_ident.h"
#include "graph_def.h"
#include "graph_config.h"
#include "common.h"
#include "utils_params.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

/*
 * Defines
 */
#define UPDATE_INTERVAL 10

#define ANY_TOKEN "/any/"
#define ALL_TOKEN "/all/"

/*
 * Data types
 */
struct gl_ident_stage_s /* {{{ */
{
  char *host;
  char *plugin;
  char *plugin_instance;
  char *type;
  char *type_instance;
}; /* }}} */
typedef struct gl_ident_stage_s gl_ident_stage_t;

struct graph_instance_s /* {{{ */
{
  graph_ident_t *select;

  graph_ident_t **files;
  size_t files_num;

  graph_instance_t *next;
}; /* }}} struct graph_instance_s */

struct graph_config_s /* {{{ */
{
  graph_ident_t *select;

  char *title;
  char *vertical_label;

  graph_def_t *defs;

  graph_instance_t *instances;

  graph_config_t *next;
}; /* }}} struct graph_config_s */

struct def_callback_data_s
{
  graph_instance_t *inst;
  str_array_t *args;
};
typedef struct def_callback_data_s def_callback_data_t;

/*
 * Global variables
 */
static graph_config_t *graph_config_head = NULL;
static graph_config_t *graph_config_staging = NULL;

static time_t gl_last_update = 0;

/*
 * Private functions
 */
/* FIXME: These "print_*" functions are used for debugging. They should be
 * removed at some point. */
static int print_files (const graph_instance_t *inst) /* {{{ */
{
  size_t i;

  for (i = 0; i < inst->files_num; i++)
  {
    graph_ident_t *ident = inst->files[i];
    char *file;

    file = ident_to_file (ident);
    printf ("    File \"%s\"\n", file);
    free (file);
  }

  return (0);
} /* }}} int print_instances */

static int print_instances (const graph_config_t *cfg) /* {{{ */
{
  graph_instance_t *inst;

  for (inst = cfg->instances; inst != NULL; inst = inst->next)
  {
    char *str;

    str = ident_to_string (inst->select);
    printf ("  Instance \"%s\"\n", str);
    free (str);

    print_files (inst);
  }

  return (0);
} /* }}} int print_instances */

static int print_graphs (void) /* {{{ */
{
  graph_config_t *cfg;

  for (cfg = graph_config_head; cfg != NULL; cfg = cfg->next)
  {
    char *str;

    str = ident_to_string (cfg->select);
    printf ("Graph \"%s\"\n", str);
    free (str);

    print_instances (cfg);
  }

  return (0);
} /* }}} int print_graphs */

#if 0
/* "Safe" version of strcmp(3): Either or both pointers may be NULL. */
static int strcmp_s (const char *s1, const char *s2) /* {{{ */
{
  if ((s1 == NULL) && (s2 == NULL))
    return (0);
  else if (s1 == NULL)
    return (1);
  else if (s2 == NULL)
    return (-1);
  assert ((s1 != NULL) && (s2 != NULL));

  return (strcmp (s1, s2));
} /* }}} int strcmp_s */
#endif

static void instance_destroy (graph_instance_t *inst) /* {{{ */
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

  instance_destroy (next);
} /* }}} void instance_destroy */

/*
 * Copy part of an identifier. If the "template" value is ANY_TOKEN, "value" is
 * copied and returned. This function is used when creating graph_instance_t
 * from graph_config_t.
 */
static graph_instance_t *instance_create (graph_config_t *cfg, /* {{{ */
    const graph_ident_t *file)
{
  graph_instance_t *i;

  if ((cfg == NULL) || (file == NULL))
    return (NULL);

  i = malloc (sizeof (*i));
  if (i == NULL)
    return (NULL);
  memset (i, 0, sizeof (*i));

  i->select = ident_copy_with_selector (cfg->select, file,
      IDENT_FLAG_REPLACE_ANY);
  if (i->select == NULL)
  {
    DEBUG ("instance_create: ident_copy_with_selector returned NULL.\n");
    free (i);
    return (NULL);
  }

  i->files = NULL;
  i->files_num = 0;

  i->next = NULL;

  if (cfg->instances == NULL)
    cfg->instances = i;
  else
  {
    graph_instance_t *last;

    last = cfg->instances;
    while (last->next != NULL)
      last = last->next;

    last->next = i;
  }

  return (i);
} /* }}} graph_instance_t *instance_create */

static int instance_add_file (graph_instance_t *inst, /* {{{ */
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
} /* }}} int instance_add_file */

static graph_instance_t *graph_find_instance (graph_config_t *cfg, /* {{{ */
    const graph_ident_t *file)
{
  graph_instance_t *i;

  if ((cfg == NULL) || (file == NULL))
    return (NULL);

  for (i = cfg->instances; i != NULL; i = i->next)
    if (ident_matches (i->select, file))
      return (i);

  return (NULL);
} /* }}} graph_instance_t *graph_find_instance */

static int graph_add_file (graph_config_t *cfg, const graph_ident_t *file) /* {{{ */
{
  graph_instance_t *inst;

  inst = graph_find_instance (cfg, file);
  if (inst == NULL)
  {
    inst = instance_create (cfg, file);
    if (inst == NULL)
      return (ENOMEM);
  }

  return (instance_add_file (inst, file));
} /* }}} int graph_add_file */

static int graph_append (graph_config_t **head, /* {{{ */
    graph_config_t *cfg)
{
  graph_config_t *last;

  if (cfg == NULL)
    return (EINVAL);

  if (head == NULL)
    head = &graph_config_head;

  if (*head == NULL)
  {
    *head = cfg;
    return (0);
  }

  last = *head;
  while (last->next != NULL)
    last = last->next;

  last->next = cfg;

  return (0);
} /* }}} int graph_append */

static graph_config_t *graph_create (const graph_ident_t *selector) /* {{{ */
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
  cfg->next = NULL;

  return (cfg);
} /* }}} int graph_create */

static void graph_destroy (graph_config_t *cfg) /* {{{ */
{
  graph_config_t *next;

  if (cfg == NULL)
    return;

  next = cfg->next;

  ident_destroy (cfg->select);

  free (cfg->title);
  free (cfg->vertical_label);

  def_destroy (cfg->defs);
  instance_destroy (cfg->instances);

  graph_destroy (next);
} /* }}} void graph_destroy */

static int register_file (const graph_ident_t *file) /* {{{ */
{
  graph_config_t *cfg;
  int num_graphs = 0;

  for (cfg = graph_config_head; cfg != NULL; cfg = cfg->next)
  {
    int status;

    if (!ident_matches (cfg->select, file))
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
    graph_append (/* head = */ NULL, cfg);
    graph_add_file (cfg, file);
  }

  return (0);
} /* }}} int register_file */

static int callback_type (const char *type, void *user_data) /* {{{ */
{
  gl_ident_stage_t *gl;
  graph_ident_t *ident;
  int status;

  if ((type == NULL) || (user_data == NULL))
    return (EINVAL);

  gl = user_data;
  if ((gl->type != NULL) || (gl->type_instance != NULL))
    return (EINVAL);

  gl->type = strdup (type);
  if (gl->type == NULL)
    return (ENOMEM);

  gl->type_instance = strchr (gl->type, '-');
  if (gl->type_instance != NULL)
  {
    *gl->type_instance = 0;
    gl->type_instance++;
  }
  else
  {
    gl->type_instance = gl->type + strlen (gl->type);
  }

  ident = ident_create (gl->host,
      gl->plugin, gl->plugin_instance,
      gl->type, gl->type_instance);
  if (ident == 0)
  {
    status = -1;
  }
  else
  {
    status = register_file (ident);
    ident_destroy (ident);
  }

  free (gl->type);
  gl->type = NULL;
  gl->type_instance = NULL;

  return (status);
} /* }}} int callback_type */

static int callback_plugin (const char *plugin, void *user_data) /* {{{ */
{
  gl_ident_stage_t *gl;
  int status;

  if ((plugin == NULL) || (user_data == NULL))
    return (EINVAL);

  gl = user_data;
  if ((gl->plugin != NULL) || (gl->plugin_instance != NULL))
    return (EINVAL);

  gl->plugin = strdup (plugin);
  if (gl->plugin == NULL)
    return (ENOMEM);

  gl->plugin_instance = strchr (gl->plugin, '-');
  if (gl->plugin_instance != NULL)
  {
    *gl->plugin_instance = 0;
    gl->plugin_instance++;
  }
  else
  {
    gl->plugin_instance = gl->plugin + strlen (gl->plugin);
  }

  status = foreach_type (gl->host, plugin, callback_type, gl);

  free (gl->plugin);
  gl->plugin = NULL;
  gl->plugin_instance = NULL;

  return (status);
} /* }}} int callback_plugin */

static int callback_host (const char *host, void *user_data) /* {{{ */
{
  gl_ident_stage_t *gl;
  int status;

  if ((host == NULL) || (user_data == NULL))
    return (EINVAL);

  gl = user_data;
  if (gl->host != NULL)
    return (EINVAL);

  gl->host = strdup (host);
  if (gl->host == NULL)
    return (ENOMEM);

  status =  foreach_plugin (host, callback_plugin, gl);

  free (gl->host);
  gl->host = NULL;

  return (status);
} /* }}} int callback_host */

static const char *get_part_from_param (const char *prim_key, /* {{{ */
    const char *sec_key)
{
  const char *val;

  val = param (prim_key);
  if (val != NULL)
    return (val);
  
  return (param (sec_key));
} /* }}} const char *get_part_from_param */

/* Create one DEF for each data source in the file. Called by
 * "gl_inst_get_default_defs" for each file. */
static graph_def_t *gl_ident_get_default_defs (graph_config_t *cfg, /* {{{ */
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
    DEBUG ("gl_ident_get_default_defs: ident_to_file returned NULL.\n");
    return (def_head);
  }

  DEBUG ("gl_ident_get_default_defs: file = %s;\n", file);

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
} /* }}} int gl_ident_get_default_defs */

/* Create one or more DEFs for each file in the graph instance. The number
 * depends on the number of data sources in each of the files. Called from
 * "gl_instance_get_rrdargs" if no DEFs are available from the configuration.
 * */
static graph_def_t *gl_inst_get_default_defs (graph_config_t *cfg, /* {{{ */
    graph_instance_t *inst)
{
  graph_def_t *defs = NULL;
  size_t i;

  if ((cfg == NULL) || (inst == NULL))
    return (NULL);

  for (i = 0; i < inst->files_num; i++)
  {
    graph_def_t *def;

    def = gl_ident_get_default_defs (cfg, inst->files[i], defs);
    if (def == NULL)
      continue;

    if (defs == NULL)
      defs = def;
    else
      def_append (defs, def);
  }

  return (defs);
} /* }}} graph_def_t *gl_inst_get_default_defs */

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

static int gl_clear_instances (void) /* {{{ */
{
  graph_config_t *cfg;

  for (cfg = graph_config_head; cfg != NULL; cfg = cfg->next)
  {
    instance_destroy (cfg->instances);
    cfg->instances = NULL;
  }

  return (0);
} /* }}} int gl_clear_instances */


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
    else if (strcasecmp ("DEF", child->key) == 0)
      def_config (cfg, child);
  } /* for */

  graph_append (&graph_config_staging, cfg);

  return (0);
} /* }}} graph_config_add */

int graph_config_submit (void) /* {{{ */
{
  graph_config_t *tmp;

  tmp = graph_config_head;
  graph_config_head = graph_config_staging;
  graph_config_staging = NULL;

  graph_destroy (tmp);

  return (0);
} /* }}} int graph_config_submit */

int gl_instance_get_params (graph_config_t *cfg, graph_instance_t *inst, /* {{{ */
    char *buffer, size_t buffer_size)
{
  if ((inst == NULL) || (buffer == NULL) || (buffer_size < 1))
    return (EINVAL);

  buffer[0] = 0;

#define COPY_FIELD(field) do {                                  \
  const char *cfg_f  = ident_get_##field (cfg->select);         \
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

  return (0);
} /* }}} int gl_instance_get_params */

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

  for (inst = cfg->instances; inst != NULL; inst = inst->next)
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

int gl_graph_get_all (gl_cfg_callback callback, /* {{{ */
    void *user_data)
{
  graph_config_t *cfg;

  if (callback == NULL)
    return (EINVAL);

  gl_update ();

  for (cfg = graph_config_head; cfg != NULL; cfg = cfg->next)
  {
    int status;

    status = (*callback) (cfg, user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int gl_graph_get_all */

graph_config_t *graph_get_selected (void) /* {{{ */
{
  const char *host = get_part_from_param ("graph_host", "host");
  const char *plugin = get_part_from_param ("graph_plugin", "plugin");
  const char *plugin_instance = get_part_from_param ("graph_plugin_instance", "plugin_instance");
  const char *type = get_part_from_param ("graph_type", "type");
  const char *type_instance = get_part_from_param ("graph_type_instance", "type_instance");
  graph_ident_t *ident;
  graph_config_t *cfg;

  if ((host == NULL)
      || (plugin == NULL) || (plugin_instance == NULL)
      || (type == NULL) || (type_instance == NULL))
    return (NULL);

  ident = ident_create (host, plugin, plugin_instance, type, type_instance);

  gl_update ();

  for (cfg = graph_config_head; cfg != NULL; cfg = cfg->next)
  {
    if (ident_compare (ident, cfg->select) != 0)
      continue;

    ident_destroy (ident);
    return (cfg);
  }

  ident_destroy (ident);
  return (NULL);
} /* }}} graph_config_t *graph_get_selected */

int gl_graph_instance_get_all (graph_config_t *cfg, /* {{{ */
    gl_inst_callback callback, void *user_data)
{
  graph_instance_t *inst;

  if ((cfg == NULL) || (callback == NULL))
    return (EINVAL);

  for (inst = cfg->instances; inst != NULL; inst = inst->next)
  {
    int status;

    status = (*callback) (cfg, inst, user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int gl_graph_instance_get_all */

int gl_graph_get_title (graph_config_t *cfg, /* {{{ */
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
} /* }}} int gl_graph_get_title */

graph_ident_t *gl_graph_get_selector (graph_config_t *cfg) /* {{{ */
{
  if (cfg == NULL)
    return (NULL);

  return (ident_clone (cfg->select));
} /* }}} graph_ident_t *gl_graph_get_selector */

int gl_graph_add_def (graph_config_t *cfg, graph_def_t *def) /* {{{ */
{
  if ((cfg == NULL) || (def == NULL))
    return (EINVAL);

  if (cfg->defs == NULL)
  {
    cfg->defs = def;
    return (0);
  }

  return (def_append (cfg->defs, def));
} /* }}} int gl_graph_add_def */


int gl_instance_get_all (gl_inst_callback callback, /* {{{ */
    void *user_data)
{
  graph_config_t *cfg;

  gl_update ();

  for (cfg = graph_config_head; cfg != NULL; cfg = cfg->next)
  {
    graph_instance_t *inst;

    for (inst = cfg->instances; inst != NULL; inst = inst->next)
    {
      int status;

      status = (*callback) (cfg, inst, user_data);
      if (status != 0)
        return (status);
    }
  }

  return (0);
} /* }}} int gl_instance_get_all */

int gl_instance_get_rrdargs (graph_config_t *cfg, /* {{{ */
    graph_instance_t *inst,
    str_array_t *args)
{
  def_callback_data_t data = { inst, args };
  graph_def_t *default_defs;
  int status;

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

  if (cfg->defs == NULL)
  {
    default_defs = gl_inst_get_default_defs (cfg, inst);

    if (default_defs == NULL)
      return (-1);

    status = def_foreach (default_defs, gl_instance_get_rrdargs_cb, &data);

    if (default_defs != NULL)
      def_destroy (default_defs);
  }
  else
  {
    status = def_foreach (cfg->defs, gl_instance_get_rrdargs_cb, &data);
  }

  return (status);
} /* }}} int gl_instance_get_rrdargs */

graph_ident_t *gl_instance_get_selector (graph_instance_t *inst) /* {{{ */
{
  if (inst == NULL)
    return (NULL);

  return (ident_clone (inst->select));
} /* }}} graph_ident_t *gl_instance_get_selector */

int gl_update (void) /* {{{ */
{
  time_t now;
  gl_ident_stage_t gl;
  int status;

  /*
  printf ("Content-Type: text/plain\n\n");
  */

  now = time (NULL);

  if ((gl_last_update + UPDATE_INTERVAL) >= now)
    return (0);

  graph_read_config ();

  memset (&gl, 0, sizeof (gl));
  gl.host = NULL;
  gl.plugin = NULL;
  gl.plugin_instance = NULL;
  gl.type = NULL;
  gl.type_instance = NULL;

  gl_clear_instances ();
  status = foreach_host (callback_host, &gl);

  gl_last_update = now;

  return (status);
} /* }}} int gl_update */

/* vim: set sw=2 sts=2 et fdm=marker : */
