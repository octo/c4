#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "graph_list.h"
#include "graph.h"
#include "graph_ident.h"
#include "graph_def.h"
#include "graph_config.h"
#include "common.h"
#include "filesystem.h"
#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

/*
 * Defines
 */
#define UPDATE_INTERVAL 10

/*
 * Global variables
 */
static graph_config_t **gl_active = NULL;
static size_t gl_active_num = 0;

static graph_config_t **gl_staging = NULL;
static size_t gl_staging_num = 0;

static time_t gl_last_update = 0;

/*
 * Private functions
 */
int gl_add_graph_internal (graph_config_t *cfg, /* {{{ */
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
    gl_add_graph_internal (cfg, &gl_active, &gl_active_num);
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
  size_t i;

  old = gl_active;
  old_num = gl_active_num;

  gl_active = gl_staging;
  gl_active_num = gl_staging_num;

  gl_staging = NULL;
  gl_staging_num = 0;

  for (i = 0; i < old_num; i++)
  {
    graph_destroy (old[i]);
    old[i] = NULL;
  }
  free (old);

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

  return (0);
} /* }}} int gl_instance_get_all */
/* }}} gl_instance_get_all, gl_graph_instance_get_all */

int gl_search (const char *term, graph_inst_callback_t callback, /* {{{ */
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

  return (0);
} /* }}} int gl_search */

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

  return (0);
} /* }}} int gl_search_field */

int gl_update (void) /* {{{ */
{
  time_t now;
  int status;

  /*
  printf ("Content-Type: text/plain\n\n");
  */

  now = time (NULL);

  if ((gl_last_update + UPDATE_INTERVAL) >= now)
    return (0);

  graph_read_config ();

  gl_clear_instances ();
  status = fs_scan (/* callback = */ gl_register_file, /* user data = */ NULL);

  gl_last_update = now;

  return (status);
} /* }}} int gl_update */

/* vim: set sw=2 sts=2 et fdm=marker : */
