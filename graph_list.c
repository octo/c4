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
#include "filesystem.h"
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

struct graph_config_s /* {{{ */
{
  graph_ident_t *select;

  char *title;
  char *vertical_label;

  graph_def_t *defs;

  graph_instance_t *instances;

  graph_config_t *next;
}; /* }}} struct graph_config_s */

/*
 * Global variables
 */
static graph_config_t *graph_config_head = NULL;
static graph_config_t *graph_config_staging = NULL;

static time_t gl_last_update = 0;

/*
 * Private functions
 */
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

/*
 * Copy part of an identifier. If the "template" value is ANY_TOKEN, "value" is
 * copied and returned. This function is used when creating graph_instance_t
 * from graph_config_t.
 */

static graph_instance_t *graph_find_instance (graph_config_t *cfg, /* {{{ */
    const graph_ident_t *ident)
{
  if ((cfg == NULL) || (ident == NULL))
    return (NULL);

  return (inst_find_matching (cfg->instances, ident));
} /* }}} graph_instance_t *graph_find_instance */

static int graph_add_file (graph_config_t *cfg, const graph_ident_t *file) /* {{{ */
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
  inst_destroy (cfg->instances);

  graph_destroy (next);
} /* }}} void graph_destroy */

static int gl_register_file (const graph_ident_t *file, /* {{{ */
    __attribute__((unused)) void *user_data)
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
  graph_config_t *cfg;

  for (cfg = graph_config_head; cfg != NULL; cfg = cfg->next)
  {
    inst_destroy (cfg->instances);
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

/* gl_instance_get_all, gl_graph_instance_get_all {{{ */
struct gl_inst_callback_data /* {{{ */
{
  graph_config_t *cfg;
  gl_inst_callback callback;
  void *user_data;
}; /* }}} struct gl_inst_callback_data */

static int gl_inst_callback_handler (graph_instance_t *inst, /* {{{ */
    void *user_data)
{
  struct gl_inst_callback_data *data = user_data;

  return ((*data->callback) (data->cfg, inst, data->user_data));
} /* }}} int gl_inst_callback_handler */

int gl_graph_instance_get_all (graph_config_t *cfg, /* {{{ */
    gl_inst_callback callback, void *user_data)
{
  struct gl_inst_callback_data data =
  {
    cfg,
    callback,
    user_data
  };

  if ((cfg == NULL) || (callback == NULL))
    return (EINVAL);

  return (inst_foreach (cfg->instances, gl_inst_callback_handler, &data));
} /* }}} int gl_graph_instance_get_all */

int gl_instance_get_all (gl_inst_callback callback, /* {{{ */
    void *user_data)
{
  graph_config_t *cfg;

  gl_update ();

  for (cfg = graph_config_head; cfg != NULL; cfg = cfg->next)
  {
    int status;

    status = gl_graph_instance_get_all (cfg, callback, user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int gl_instance_get_all */
/* }}} gl_instance_get_all, gl_graph_instance_get_all */

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

graph_instance_t *gl_graph_get_instances (graph_config_t *cfg) /* {{{ */
{
  if (cfg == NULL)
    return (NULL);

  return (cfg->instances);
} /* }}} graph_instance_t *gl_graph_get_instances */

graph_def_t *gl_graph_get_defs (graph_config_t *cfg) /* {{{ */
{
  if (cfg == NULL)
    return (NULL);

  return (cfg->defs);
} /* }}} graph_def_t *gl_graph_get_defs */

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
  status = fs_scan (/* callback = */ gl_register_file, /* user data = */ NULL);

  gl_last_update = now;

  return (status);
} /* }}} int gl_update */

/* vim: set sw=2 sts=2 et fdm=marker : */
