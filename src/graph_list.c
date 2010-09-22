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
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <yajl/yajl_parse.h>

#include "graph_list.h"
#include "common.h"
#include "data_provider.h"
#include "filesystem.h"
#include "graph.h"
#include "graph_config.h"
#include "graph_def.h"
#include "graph_ident.h"
#include "graph_instance.h"
#include "utils_cgi.h"
#include "utils_search.h"

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

    if (!graph_ident_matches (cfg, file))
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

static int gl_register_ident (graph_ident_t *ident, /* {{{ */
    __attribute__((unused)) void *user_data)
{
  /* TODO: Check for duplicates if multiple data providers are used. */

  return (gl_register_file (ident, user_data));
} /* }}} int gl_register_ident */

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

static void gl_dump_cb (void *ctx, /* {{{ */
    const char *str, unsigned int len)
{
  int fd = *((int *) ctx);
  const char *buffer;
  size_t buffer_size;
  ssize_t status;

  buffer = str;
  buffer_size = (size_t) len;
  while (buffer_size > 0)
  {
    status = write (fd, buffer, buffer_size);
    if (status < 0)
    {
      fprintf (stderr, "write(2) failed with status %i\n", errno);
      return;
    }

    buffer += status;
    buffer_size -= status;
  }
} /* }}} void gl_dump_cb */

static int gl_update_cache (void) /* {{{ */
{
  int fd;
  yajl_gen handler;
  yajl_gen_config handler_config = { /* pretty = */ 1, /* indent = */ "  " };
  const char *cache_file = graph_config_get_cache_file ();
  struct flock lock;
  struct stat statbuf;
  int status;
  size_t i;

  memset (&statbuf, 0, sizeof (statbuf));
  status = stat (cache_file, &statbuf);
  if (status == 0)
  {
    if (statbuf.st_mtime >= gl_last_update)
      /* Not writing to cache because it's at least as new as our internal data */
      return (0);
  }
  else
  {
    status = errno;
    fprintf (stderr, "gl_update_cache: stat(2) failed with status %i\n",
        status);
    /* Continue writing the file if possible. */
  }

  fd = open (cache_file, O_WRONLY | O_TRUNC | O_CREAT,
      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
  if (fd < 0)
  {
    status = errno;
    fprintf (stderr, "gl_update_cache: open(2) failed with status %i\n",
        status);
    return (status);
  }

  memset (&lock, 0, sizeof (lock));
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0; /* lock everything */

  while (42)
  {
    status = fcntl (fd, F_SETLKW, &lock);
    if (status == 0)
      break;

    if (errno == EINTR)
      continue;

    fprintf (stderr, "gl_update_cache: fcntl(2) failed with status %i\n", errno);
    close (fd);
    return (errno);
  }

  handler = yajl_gen_alloc2 (gl_dump_cb, &handler_config,
      /* alloc funcs = */ NULL, /* ctx = */ &fd);
  if (handler == NULL)
  {
    close (fd);
    return (-1);
  }

  fprintf (stderr, "gl_update_cache: Start writing data\n");
  fflush (stderr);

  yajl_gen_array_open (handler);

  for (i = 0; i < gl_active_num; i++)
    graph_to_json (gl_active[i], handler);

  for (i = 0; i < gl_dynamic_num; i++)
    graph_to_json (gl_dynamic[i], handler);

  yajl_gen_array_close (handler);

  yajl_gen_free (handler);
  close (fd);

  fprintf (stderr, "gl_update_cache: Finished writing data\n");
  fflush (stderr);

  return (0);
} /* }}} int gl_update_cache */

/*
 * JSON parsing functions
 */
#define CTX_MASK                  0xff000000
#define CTX_GRAPH                 0x01000000
#define CTX_GRAPH_SELECT          0x02000000
#define CTX_INST                  0x03000000
#define CTX_INST_SELECT           0x04000000
#define CTX_INST_FILE             0x05000000

#define CTX_IDENT_MASK            0x00ff0000
#define CTX_IDENT_HOST            0x00010000
#define CTX_IDENT_PLUGIN          0x00020000
#define CTX_IDENT_PLUGIN_INSTANCE 0x00030000
#define CTX_IDENT_TYPE            0x00040000
#define CTX_IDENT_TYPE_INSTANCE   0x00050000

struct gl_json_context_s
{
  uint32_t          state;

  graph_config_t   *cfg;
  graph_instance_t *inst;
  graph_ident_t    *ident;

  _Bool             dynamic_graph;
};
typedef struct gl_json_context_s gl_json_context_t;

static void set_state (gl_json_context_t *ctx, /* {{{ */
    uint32_t new_state, uint32_t mask)
{
  uint32_t old_state = ctx->state;
  ctx->state = (old_state & ~mask) | (new_state & mask);
} /* }}} void set_state */

static int gl_json_string (void *user_data, /* {{{ */
    const unsigned char *str,
    unsigned int str_length)
{
  gl_json_context_t *ctx = user_data;
  char buffer[str_length + 1];

  memcpy (buffer, str, str_length);
  buffer[str_length] = 0;

  if (((ctx->state & CTX_MASK) == CTX_GRAPH_SELECT)
      || ((ctx->state & CTX_MASK) == CTX_INST_SELECT)
      || ((ctx->state & CTX_MASK) == CTX_INST_FILE))
  {
    switch (ctx->state & CTX_IDENT_MASK)
    {
      case CTX_IDENT_HOST:
        ident_set_host (ctx->ident, buffer);
        break;
      case CTX_IDENT_PLUGIN:
        ident_set_plugin (ctx->ident, buffer);
        break;
      case CTX_IDENT_PLUGIN_INSTANCE:
        ident_set_plugin_instance (ctx->ident, buffer);
        break;
      case CTX_IDENT_TYPE:
        ident_set_type (ctx->ident, buffer);
        break;
      case CTX_IDENT_TYPE_INSTANCE:
        ident_set_type_instance (ctx->ident, buffer);
        break;
    }
  }

  return (1);
} /* }}} int gl_json_string */

static int gl_json_start_map (void *user_data) /* {{{ */
{
  gl_json_context_t *ctx = user_data;

  if (((ctx->state & CTX_MASK) == CTX_GRAPH_SELECT)
      || ((ctx->state & CTX_MASK) == CTX_INST_SELECT)
      || ((ctx->state & CTX_MASK) == CTX_INST_FILE))
  {
      assert (ctx->ident == NULL);
      ctx->ident = ident_create (ANY_TOKEN,
          ANY_TOKEN, ANY_TOKEN,
          ANY_TOKEN, ANY_TOKEN);
  }

  return (1);
} /* }}} int gl_json_start_map */

static int gl_json_end_map (void *user_data) /* {{{ */
{
  gl_json_context_t *ctx = user_data;

  if ((ctx->state & CTX_MASK) == CTX_GRAPH_SELECT)
  {
    size_t i;

    /* ctx->ident should now hold the valid selector */
    assert (ctx->cfg == NULL);
    assert (ctx->inst  == NULL);
    assert (ctx->ident != NULL);

    for (i = 0; i < gl_active_num; i++)
    {
      if (graph_compare (gl_active[i], ctx->ident) != 0)
        continue;

      ctx->cfg = gl_active[i];
      ctx->dynamic_graph = 0;
      break;
    }

    if (ctx->cfg == NULL)
    {
      ctx->cfg = graph_create (ctx->ident);
      ctx->dynamic_graph = 1;
    }

    ident_destroy (ctx->ident);
    ctx->ident = NULL;

    set_state (ctx, CTX_GRAPH, CTX_MASK);
  }
  else if ((ctx->state & CTX_MASK) == CTX_INST_SELECT)
  {
    /* ctx->ident should now hold the valid selector */
    assert (ctx->cfg   != NULL);
    assert (ctx->inst  == NULL);
    assert (ctx->ident != NULL);

    ctx->inst = inst_create (ctx->cfg, ctx->ident);
    ident_destroy (ctx->ident);
    ctx->ident = NULL;

    set_state (ctx, CTX_INST, CTX_MASK);
  }
  else if ((ctx->state & CTX_MASK) == CTX_INST_FILE)
  {
    /* ctx->ident should now hold the valid file */
    assert (ctx->cfg   != NULL);
    assert (ctx->inst  != NULL);
    assert (ctx->ident != NULL);

    inst_add_file (ctx->inst, ctx->ident);
    ident_destroy (ctx->ident);
    ctx->ident = NULL;

    /* Don't reset the state here, files are in an array. */
  }
  else if ((ctx->state & CTX_MASK) == CTX_INST)
  {
    /* ctx->inst should now hold a complete instance */
    assert (ctx->cfg   != NULL);
    assert (ctx->inst  != NULL);
    assert (ctx->ident == NULL);

    graph_add_inst (ctx->cfg, ctx->inst);
    /* don't destroy / free ctx->inst */
    ctx->inst = NULL;

    /* Don't reset the state here, instances are in an array. */
  }
  else if ((ctx->state & CTX_MASK) == CTX_GRAPH)
  {
    /* ctx->cfg should now hold a complete graph */
    assert (ctx->cfg   != NULL);
    assert (ctx->inst  == NULL);
    assert (ctx->ident == NULL);

    if (ctx->dynamic_graph)
      gl_add_graph_internal (ctx->cfg, &gl_dynamic, &gl_dynamic_num);
    /* else: already contained in gl_active */
    ctx->cfg = NULL;

    /* Don't reset the state here, graphs are in an array. */
  }

  return (1);
} /* }}} int gl_json_end_map */

static int gl_json_end_array (void *user_data) /* {{{ */
{
  gl_json_context_t *ctx = user_data;

  if ((ctx->state & CTX_MASK) == CTX_INST_FILE)
    set_state (ctx, CTX_INST, CTX_MASK);
  else if ((ctx->state & CTX_MASK) == CTX_INST)
    set_state (ctx, CTX_GRAPH, CTX_MASK);
  else if ((ctx->state & CTX_MASK) == CTX_GRAPH)
  {
    /* We're done */
  }

  return (1);
} /* }}} int gl_json_end_array */

static int gl_json_key (void *user_data, /* {{{ */
    const unsigned char *str,
    unsigned int str_length)
{
  gl_json_context_t *ctx = user_data;
  char buffer[str_length + 1];

  memcpy (buffer, str, str_length);
  buffer[str_length] = 0;

  if ((ctx->state & CTX_MASK) == CTX_GRAPH)
  {
    if (strcasecmp ("select", buffer) == 0)
      set_state (ctx, CTX_GRAPH_SELECT, CTX_MASK);
    else if (strcasecmp ("instances", buffer) == 0)
      set_state (ctx, CTX_INST, CTX_MASK);
  }
  else if ((ctx->state & CTX_MASK) == CTX_INST)
  {
    if (strcasecmp ("select", buffer) == 0)
      set_state (ctx, CTX_INST_SELECT, CTX_MASK);
    else if (strcasecmp ("files", buffer) == 0)
      set_state (ctx, CTX_INST_FILE, CTX_MASK);
  }
  else if (((ctx->state & CTX_MASK) == CTX_GRAPH_SELECT)
      || ((ctx->state & CTX_MASK) == CTX_INST_SELECT)
      || ((ctx->state & CTX_MASK) == CTX_INST_FILE))
  {
    assert (ctx->ident != NULL);

    if (strcasecmp ("host", buffer) == 0)
      set_state (ctx, CTX_IDENT_HOST, CTX_IDENT_MASK);
    else if (strcasecmp ("plugin", buffer) == 0)
      set_state (ctx, CTX_IDENT_PLUGIN, CTX_IDENT_MASK);
    else if (strcasecmp ("plugin_instance", buffer) == 0)
      set_state (ctx, CTX_IDENT_PLUGIN_INSTANCE, CTX_IDENT_MASK);
    else if (strcasecmp ("type", buffer) == 0)
      set_state (ctx, CTX_IDENT_TYPE, CTX_IDENT_MASK);
    else if (strcasecmp ("type_instance", buffer) == 0)
      set_state (ctx, CTX_IDENT_TYPE_INSTANCE, CTX_IDENT_MASK);
  }

  return (1);
} /* }}} int gl_json_key */

yajl_callbacks gl_json_callbacks =
{
  /*        null = */ NULL,
  /*     boolean = */ NULL,
  /*     integer = */ NULL,
  /*      double = */ NULL,
  /*      number = */ NULL,
  /*      string = */ gl_json_string,
  /*   start_map = */ gl_json_start_map,
  /*     map_key = */ gl_json_key,
  /*     end_map = */ gl_json_end_map,
  /* start_array = */ NULL,
  /*   end_array = */ gl_json_end_array
};

static int gl_read_cache (_Bool block) /* {{{ */
{
  yajl_handle handle;
  gl_json_context_t context;
  yajl_parser_config handle_config = { /* comments = */ 0, /* check UTF-8 */ 0 };

  int fd;
  int cmd;
  struct stat statbuf;
  struct flock lock;
  int status;
  time_t now;

  fd = open (graph_config_get_cache_file (), O_RDONLY);
  if (fd < 0)
  {
    fprintf (stderr, "gl_read_cache: open(2) failed with status %i\n", errno);
    return (errno);
  }

  if (block)
    cmd = F_SETLKW;
  else
    cmd = F_SETLK;

  memset (&lock, 0, sizeof (lock));
  lock.l_type = F_RDLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0; /* lock everything */

  while (42)
  {
    status = fcntl (fd, cmd, &lock);
    if (status == 0)
      break;

    if (!block && ((errno == EACCES) || (errno == EAGAIN)))
    {
      close (fd);
      return (0);
    }

    if (errno == EINTR)
      continue;

    status = errno;
    fprintf (stderr, "gl_read_cache: fcntl(2) failed with status %i\n",
        status);
    close (fd);
    return (status);
  }

  fprintf (stderr, "gl_read_cache: Opening and locking "
      "cache file successful\n");

  memset (&statbuf, 0, sizeof (statbuf));
  status = fstat (fd, &statbuf);
  if (status != 0)
  {
    status = errno;
    fprintf (stderr, "gl_read_cache: fstat(2) failed with status %i\n",
        status);
    close (fd);
    return (status);
  }

  now = time (NULL);

  if (block)
  {
    /* Read the file. No excuses. */
  }
  else if (statbuf.st_mtime <= gl_last_update)
  {
    /* Our current data is at least as new as the cache. Return. */
    fprintf (stderr, "gl_read_cache: Not using cache because "
        "the internal data is newer\n");
    fflush (stderr);
    close (fd);
    return (0);
  }
  else if ((statbuf.st_mtime + UPDATE_INTERVAL) < now)
  {
    /* We'll scan the directory anyway, so there is no need to parse the cache
     * here. */
    fprintf (stderr, "gl_read_cache: Not using cache because it's too old\n");
    fflush (stderr);
    close (fd);
    return (0);
  }

  memset (&context, 0, sizeof (context));
  context.state = CTX_GRAPH;
  context.cfg = NULL;
  context.inst = NULL;
  context.ident = NULL;

  handle = yajl_alloc (&gl_json_callbacks,
      &handle_config,
      /* alloc funcs = */ NULL,
      &context);

  fprintf (stderr, "gl_read_cache: Start parsing data\n");
  fflush (stderr);

  while (42)
  {
    ssize_t rd_status;
    char buffer[1024*1024];

    rd_status = read (fd, buffer, sizeof (buffer));
    if (rd_status < 0)
    {
      if ((errno == EINTR) || (errno == EAGAIN))
        continue;

      status = errno;
      fprintf (stderr, "gl_read_cache: read(2) failed with status %i\n",
          status);
      close (fd);
      return (status);
    }
    else if (rd_status == 0)
    {
      yajl_parse_complete (handle);
      break;
    }
    else
    {
      yajl_parse (handle,
          (unsigned char *) &buffer[0],
          (unsigned int) rd_status);
    }
  }

  yajl_free (handle);

  gl_last_update = statbuf.st_mtime;
  close (fd);

  fprintf (stderr, "gl_read_cache: Finished parsing data\n");
  fflush (stderr);

  return (0);
} /* }}} int gl_read_cache */

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

int gl_graph_get_all (_Bool include_dynamic, /* {{{ */
    graph_callback_t callback, void *user_data)
{
  size_t i;

  if (callback == NULL)
    return (EINVAL);

  gl_update (/* request served = */ 0);

  for (i = 0; i < gl_active_num; i++)
  {
    int status;

    status = (*callback) (gl_active[i], user_data);
    if (status != 0)
      return (status);
  }

  if (!include_dynamic)
    return (0);

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

  gl_update (/* request served = */ 0);

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

  gl_update (/* request served = */ 0);

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

int gl_search (search_info_t *si, /* {{{ */
    graph_inst_callback_t callback, void *user_data)
{
  size_t i;
  graph_ident_t *ident;

  if ((si == NULL) || (callback == NULL))
    return (EINVAL);

  if (search_has_selector (si))
  {
    ident = search_to_ident (si);
    if (ident == NULL)
    {
      fprintf (stderr, "gl_search: search_to_ident failed\n");
      return (-1);
    }
  }
  else
  {
    ident = NULL;
  }

  for (i = 0; i < gl_active_num; i++)
  {
    int status;

    if ((ident != NULL) && !graph_ident_intersect (gl_active[i], ident))
      continue;

    status = graph_search_inst (gl_active[i], si,
        /* callback  = */ callback,
        /* user data = */ user_data);
    if (status != 0)
      return (status);
  }

  for (i = 0; i < gl_dynamic_num; i++)
  {
    int status;

    if ((ident != NULL) && !graph_ident_intersect (gl_dynamic[i], ident))
      continue;

    status = graph_search_inst (gl_dynamic[i], si,
        /* callback  = */ callback,
        /* user data = */ user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int gl_search */

int gl_search_string (const char *term, graph_inst_callback_t callback, /* {{{ */
    void *user_data)
{
  size_t i;

  for (i = 0; i < gl_active_num; i++)
  {
    int status;

    status = graph_search_inst_string (gl_active[i], term,
        /* callback  = */ callback,
        /* user data = */ user_data);
    if (status != 0)
      return (status);
  }

  for (i = 0; i < gl_dynamic_num; i++)
  {
    int status;

    status = graph_search_inst_string (gl_dynamic[i], term,
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

int gl_update (_Bool request_served) /* {{{ */
{
  time_t now;
  int status;
  size_t i;

  if (!request_served && (gl_last_update > 0))
    return (0);

  now = time (NULL);

  if ((gl_last_update + UPDATE_INTERVAL) >= now)
  {
    /* Write data to cache if appropriate */
    if (request_served)
      gl_update_cache ();
    return (0);
  }

  /* Clear state */
  gl_clear_instances ();
  gl_clear_hosts ();
  gl_destroy (&gl_dynamic, &gl_dynamic_num);

  graph_read_config ();

  status = gl_read_cache (/* block = */ 1);
  /* We have *something* to work with. Even if it's outdated, just get on with
   * handling the request and take care of re-reading data later on. */
  if ((status == 0) && !request_served)
    return (0);

  if ((status != 0)
      || ((gl_last_update + UPDATE_INTERVAL) < now))
  {
    /* Clear state */
    gl_clear_instances ();
    gl_clear_hosts ();
    gl_destroy (&gl_dynamic, &gl_dynamic_num);

    data_provider_get_idents (gl_register_ident, /* user data = */ NULL);

    gl_last_update = now;
  }

  if (host_list_len > 0)
    qsort (host_list, host_list_len, sizeof (*host_list),
        gl_compare_hosts);

  for (i = 0; i < gl_active_num; i++)
    graph_sort_instances (gl_active[i]);

  if (request_served)
    gl_update_cache ();

  return (status);
} /* }}} int gl_update */

/* vim: set sw=2 sts=2 et fdm=marker : */
