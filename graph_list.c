#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include <fcgiapp.h>
#include <fcgi_stdio.h>

#include "graph_list.h"
#include "common.h"

/*
 * Defines
 */
#define UPDATE_INTERVAL 10

#define ANY_TOKEN "/any/"
#define ALL_TOKEN "/all/"

/*
 * Data types
 */
struct graph_ident_s /* {{{ */
{
  char *host;
  char *plugin;
  char *plugin_instance;
  char *type;
  char *type_instance;
}; /* }}} struct graph_ident_s */

struct graph_instance_s;
typedef struct graph_instance_s graph_instance_t;
struct graph_instance_s /* {{{ */
{
  graph_ident_t select;

  graph_ident_t *files;
  size_t files_num;

  graph_instance_t *next;
}; /* }}} struct graph_instance_s */

struct graph_config_s;
typedef struct graph_config_s graph_config_t;
struct graph_config_s /* {{{ */
{
  graph_ident_t select;

  char *title;
  char *vertical_label;

  graph_instance_t *instances;

  graph_config_t *next;
}; /* }}} struct graph_config_s */

/*
 * Global variables
 */
static graph_config_t *graph_config_head = NULL;

static graph_ident_t *graph_list = NULL;
static size_t graph_list_length = 0;
static time_t gl_last_update = 0;

/*
 * Private functions
 */
static int print_files (const graph_instance_t *inst) /* {{{ */
{
  size_t i;

  for (i = 0; i < inst->files_num; i++)
  {
    graph_ident_t *file = inst->files + i;

    printf ("    File \"%s/%s-%s/%s-%s\"\n",
        file->host,
        file->plugin, file->plugin_instance,
        file->type, file->type_instance);
  }

  return (0);
} /* }}} int print_instances */

static int print_instances (const graph_config_t *cfg) /* {{{ */
{
  graph_instance_t *inst;

  for (inst = cfg->instances; inst != NULL; inst = inst->next)
  {
    printf ("  Instance \"%s/%s-%s/%s-%s\"\n",
        inst->select.host,
        inst->select.plugin, inst->select.plugin_instance,
        inst->select.type, inst->select.type_instance);

    print_files (inst);
  }

  return (0);
} /* }}} int print_instances */

static int print_graphs (void) /* {{{ */
{
  graph_config_t *cfg;

  for (cfg = graph_config_head; cfg != NULL; cfg = cfg->next)
  {
    printf ("Graph \"%s/%s-%s/%s-%s\"\n",
        cfg->select.host,
        cfg->select.plugin, cfg->select.plugin_instance,
        cfg->select.type, cfg->select.type_instance);

    print_instances (cfg);
  }

  return (0);
} /* }}} int print_graphs */


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

static _Bool part_matches (const char *sel, const char *val) /* {{{ */
{
  if ((sel == NULL) && (val == NULL))
    return (1);

  if (sel == NULL) /* && (val != NULL) */
    return (0);

  if ((strcasecmp (ALL_TOKEN, sel) == 0)
      || (strcasecmp (ANY_TOKEN, sel) == 0))
    return (1);

  if (val == NULL) /* && (sel != NULL) */
    return (0);

  if (strcmp (sel, val) == 0)
    return (1);

  return (0);
} /* }}} _Bool part_matches */

static _Bool file_matches (const graph_ident_t *sel, /* {{{ */
    const graph_ident_t *val)
{
  if ((sel == NULL) && (val == NULL))
    return (0);
  else if (sel == NULL)
    return (-1);
  else if (val == NULL)
    return (1);

  if (!part_matches (sel->host, val->host))
    return (0);

  if (!part_matches (sel->plugin, val->plugin))
    return (0);

  if (!part_matches (sel->plugin_instance, val->plugin_instance))
    return (0);

  if (!part_matches (sel->type, val->type))
    return (0);

  if (!part_matches (sel->type_instance, val->type_instance))
    return (0);

  return (1);
} /* }}} _Bool ident_compare */

/*
 * Copy part of an identifier. If the "template" value is ANY_TOKEN, "value" is
 * copied and returned. This function is used when creating graph_instance_t
 * from graph_config_t.
 */
static char *identifier_copy_part (const char *template, const char *value) /* {{{ */
{
  if (template == NULL)
  {
    return (NULL);
  }
  else if (strcasecmp (ANY_TOKEN, template) == 0)
  {
    /* ANY in the graph selection => concrete value in the instance. */
    if (value == NULL)
      return (NULL);
    else
      return (strdup (value));
  }
  else if (strcasecmp (ALL_TOKEN, template) == 0)
  {
    /* ALL in the graph selection => ALL in the instance. */
    return (strdup (ALL_TOKEN));
  }
  else
  {
    assert (value != NULL);
    assert (strcmp (template, value) == 0);
    return (strdup (template));
  }
} /* }}} int identifier_copy_part */

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

  i->select.host = identifier_copy_part (cfg->select.host, file->host);
  i->select.plugin = identifier_copy_part (cfg->select.plugin, file->plugin);
  i->select.plugin_instance = identifier_copy_part (cfg->select.plugin_instance,
      file->plugin_instance);
  i->select.type = identifier_copy_part (cfg->select.type, file->type);
  i->select.type_instance = identifier_copy_part (cfg->select.type_instance,
      file->type_instance);

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
  graph_ident_t *tmp;

  tmp = realloc (inst->files, sizeof (*inst->files) * (inst->files_num + 1));
  if (tmp == NULL)
    return (ENOMEM);
  inst->files = tmp;
  tmp = inst->files + inst->files_num;

  memset (tmp, 0, sizeof (*tmp));
  tmp->host = strdup (file->host);
  tmp->plugin = strdup (file->plugin);
  if (file->plugin_instance != NULL)
    tmp->plugin_instance = strdup (file->plugin_instance);
  else
    tmp->plugin_instance = NULL;
  tmp->type = strdup (file->type);
  if (file->type_instance != NULL)
    tmp->type_instance = strdup (file->type_instance);
  else
    tmp->type_instance = NULL;

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
    if (file_matches (&i->select, file))
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

static int graph_append (graph_config_t *cfg) /* {{{ */
{
  graph_config_t *last;

  if (cfg == NULL)
    return (EINVAL);

  if (graph_config_head == NULL)
  {
    graph_config_head = cfg;
    return (0);
  }

  last = graph_config_head;
  while (last->next != NULL)
    last = last->next;

  last->next = cfg;

  return (0);
} /* }}} int graph_append */

static int graph_create_from_file (const graph_ident_t *file) /* {{{ */
{
  graph_config_t *cfg;

  cfg = malloc (sizeof (*cfg));
  if (cfg == NULL)
    return (ENOMEM);
  memset (cfg, 0, sizeof (*cfg));

  cfg->select.host = strdup (file->host);
  cfg->select.plugin = strdup (file->plugin);
  cfg->select.plugin_instance = strdup (file->plugin_instance);
  cfg->select.type = strdup (file->type);
  cfg->select.type_instance = strdup (file->type_instance);

  cfg->title = NULL;
  cfg->vertical_label = NULL;
  cfg->instances = NULL;
  cfg->next = NULL;

  graph_append (cfg);

  return (graph_add_file (cfg, file));
} /* }}} int graph_create_from_file */

static int register_file (const graph_ident_t *file) /* {{{ */
{
  graph_config_t *cfg;
  int num_graphs = 0;

  for (cfg = graph_config_head; cfg != NULL; cfg = cfg->next)
  {
    int status;

    if (!file_matches (&cfg->select, file))
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
    graph_create_from_file (file);

  return (0);
} /* }}} int register_file */

static int FIXME_graph_create_from_file (const graph_ident_t *file) /* {{{ */
{
  graph_config_t *cfg;

  cfg = malloc (sizeof (*cfg));
  if (cfg == NULL)
    return (ENOMEM);
  memset (cfg, 0, sizeof (*cfg));

  cfg->select.host = strdup (file->host);
  cfg->select.plugin = strdup (file->plugin);
  cfg->select.plugin_instance = strdup (file->plugin_instance);
  cfg->select.type = strdup (file->type);
  cfg->select.type_instance = strdup (file->type_instance);

  cfg->title = NULL;
  cfg->vertical_label = NULL;
  cfg->instances = NULL;
  cfg->next = NULL;

  graph_append (cfg);

  return (0);
} /* }}} int FIXME_graph_create_from_file */

static int read_graph_config (void) /* {{{ */
{
  if (graph_config_head != NULL)
    return (0);

  graph_ident_t ident;


  ident.host = ANY_TOKEN;
  ident.plugin = "cpu";
  ident.plugin_instance = ANY_TOKEN;
  ident.type = "cpu";
  ident.type_instance = ALL_TOKEN;
  FIXME_graph_create_from_file (&ident);

  ident.plugin = "memory";
  ident.plugin_instance = "";
  ident.type = "memory";
  FIXME_graph_create_from_file (&ident);

  ident.plugin = "swap";
  ident.plugin_instance = "";
  ident.type = "swap";
  FIXME_graph_create_from_file (&ident);

  ident.plugin = ANY_TOKEN;
  ident.plugin_instance = ANY_TOKEN;
  ident.type = "ps_state";
  FIXME_graph_create_from_file (&ident);

  ident.host = ALL_TOKEN;
  ident.plugin = "cpu";
  ident.plugin_instance = ALL_TOKEN;
  ident.type = "cpu";
  ident.type_instance = "idle";
  FIXME_graph_create_from_file (&ident);

  return (0);
} /* }}} int read_graph_config */



static int gl_compare (const void *p0, const void *p1) /* {{{ */
{
  const graph_ident_t *gl0 = p0;
  const graph_ident_t *gl1 = p1;
  int status;

  status = strcmp (gl0->host, gl1->host);
  if (status != 0)
    return (status);

  status = strcmp (gl0->plugin, gl1->plugin);
  if (status != 0)
    return (status);

  status = strcmp_s (gl0->plugin_instance, gl1->plugin_instance);
  if (status != 0)
    return (status);

  status = strcmp (gl0->type, gl1->type);
  if (status != 0)
    return (status);

  return (strcmp_s (gl0->type_instance, gl1->type_instance));
} /* }}} int gl_compare */

static void gl_clear_entry (graph_ident_t *gl) /* {{{ */
{
  if (gl == NULL)
    return;

  free (gl->host);
  free (gl->plugin);
  free (gl->plugin_instance);
  free (gl->type);
  free (gl->type_instance);

  gl->host = NULL;
  gl->plugin = NULL;
  gl->plugin_instance = NULL;
  gl->type = NULL;
  gl->type_instance = NULL;
} /* }}} void gl_clear_entry */

static void gl_clear (void) /* {{{ */
{
  size_t i;

  for (i = 0; i < graph_list_length; i++)
    gl_clear_entry (graph_list + i);

  free (graph_list);
  graph_list = NULL;
  graph_list_length = 0;
  gl_last_update = 0;
} /* }}} void gl_clear */

static int gl_add_copy (graph_ident_t *gl) /* {{{ */
{
  graph_ident_t *ptr;
  int status;

  if (gl == NULL)
    return (EINVAL);

  ptr = realloc (graph_list, sizeof (*graph_list) * (graph_list_length + 1));
  if (ptr == NULL)
    return (ENOMEM);
  graph_list = ptr;

  ptr = graph_list + graph_list_length;
  memset (ptr, 0, sizeof (*ptr));
  ptr->host = NULL;
  ptr->plugin = NULL;
  ptr->plugin_instance = NULL;
  ptr->type = NULL;
  ptr->type_instance = NULL;

#define DUP_OR_BREAK(member) do {                    \
  ptr->member = NULL;                                \
  if (gl->member != NULL)                            \
  {                                                  \
    ptr->member = strdup (gl->member);               \
    if (ptr->member == NULL)                         \
      break;                                         \
  }                                                  \
} while (0)

  status = ENOMEM;
  do
  {
    DUP_OR_BREAK(host);
    DUP_OR_BREAK(plugin);
    DUP_OR_BREAK(plugin_instance);
    DUP_OR_BREAK(type);
    DUP_OR_BREAK(type_instance);

    status = 0;
  } while (0);

#undef DUP_OR_BREAK

  if (status != 0)
  {
    free (ptr->host);
    free (ptr->plugin);
    free (ptr->plugin_instance);
    free (ptr->type);
    free (ptr->type_instance);
    return (status);
  }

  graph_list_length++;
  return (0);
} /* }}} int gl_add_copy */

static int callback_type (const char *type, void *user_data) /* {{{ */
{
  graph_ident_t *gl;
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

  register_file (gl);

  status = gl_add_copy (gl);

  free (gl->type);
  gl->type = NULL;
  gl->type_instance = NULL;

  return (status);
} /* }}} int callback_type */

static int callback_plugin (const char *plugin, void *user_data) /* {{{ */
{
  graph_ident_t *gl;
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
  graph_ident_t *gl;
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

/*
 * Global functions
 */
int gl_update (void) /* {{{ */
{
  time_t now;
  graph_ident_t gl;
  int status;

  printf ("Content-Type: text/plain\n\n");

  read_graph_config ();

  now = time (NULL);

  if ((gl_last_update + UPDATE_INTERVAL) >= now)
    return (0);

  gl_clear ();

  memset (&gl, 0, sizeof (gl));
  gl.host = NULL;
  gl.plugin = NULL;
  gl.plugin_instance = NULL;
  gl.type = NULL;
  gl.type_instance = NULL;

  status = foreach_host (callback_host, &gl);

  print_graphs ();

  if (graph_list_length > 1)
    qsort (graph_list, graph_list_length, sizeof (*graph_list), gl_compare);

  return (status);
} /* }}} int gl_update */

int gl_foreach (gl_callback callback, void *user_data) /* {{{ */
{
  size_t i;

  for (i = 0; i < graph_list_length; i++)
  {
    int status;

    status = (*callback) ((void *) (graph_list + i), user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int gl_foreach */

/* vim: set sw=2 sts=2 et fdm=marker : */
