/**
 * collection4 - data_provider.h
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#include <rrd.h>

#include "graph_types.h"
#include "graph_config.h"
#include "graph_ident.h"
#include "data_provider.h"
#include "filesystem.h"
#include "oconfig.h"
#include "common.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

struct dp_rrdtool_s
{
  char *data_dir;
};
typedef struct dp_rrdtool_s dp_rrdtool_t;

struct dp_get_idents_data_s
{ /* {{{ */
  graph_ident_t *ident;
  dp_get_idents_callback callback;
  void *user_data;
}; /* }}} */
typedef struct dp_get_idents_data_s dp_get_idents_data_t;

static int scan_type_cb (__attribute__((unused)) const char *base_dir,
    const char *file, void *ud)
{ /* {{{ */
  dp_get_idents_data_t *data = ud;
  size_t file_len;
  char type_copy[1024];
  size_t type_copy_len;
  char *type_inst;

  file_len = strlen (file);
  if (file_len < 5)
    return (0);

  /* Ignore files that don't end in ".rrd". */
  if (strcasecmp (".rrd", file + (file_len - 4)) != 0)
    return (0);

  strncpy (type_copy, file, sizeof (type_copy));
  type_copy_len = file_len - 4;
  if (type_copy_len > (sizeof (type_copy) - 1))
    type_copy_len = sizeof (type_copy) - 1;
  type_copy[type_copy_len] = 0;

  type_inst = strchr (type_copy, '-');
  if (type_inst != NULL)
  {
    *type_inst = 0;
    type_inst++;
  }
  else
  {
    type_inst = "";
  }

  ident_set_type (data->ident, type_copy);
  ident_set_type_instance (data->ident, type_inst);

  return (data->callback (data->ident, data->user_data));
} /* }}} int scan_type_cb */

static int scan_plugin_cb (const char *base_dir,
    const char *sub_dir, void *ud)
{ /* {{{ */
  char plugin_copy[1024];
  char *plugin_inst;

  dp_get_idents_data_t *data = ud;
  char abs_dir[PATH_MAX + 1];

  strncpy (plugin_copy, sub_dir, sizeof (plugin_copy));
  plugin_copy[sizeof (plugin_copy) - 1] = 0;

  plugin_inst = strchr (plugin_copy, '-');
  if (plugin_inst != NULL)
  {
    *plugin_inst = 0;
    plugin_inst++;
  }
  else
  {
    plugin_inst = "";
  }

  ident_set_plugin (data->ident, plugin_copy);
  ident_set_plugin_instance (data->ident, plugin_inst);

  snprintf (abs_dir, sizeof (abs_dir), "%s/%s", base_dir, sub_dir);
  abs_dir[sizeof (abs_dir) - 1] = 0;

  return (fs_foreach_file (abs_dir, scan_type_cb, data));
} /* }}} int scan_host_cb */

static int scan_host_cb (const char *base_dir,
    const char *sub_dir, void *ud)
{ /* {{{ */
  dp_get_idents_data_t *data = ud;
  char abs_dir[PATH_MAX + 1];

  ident_set_host (data->ident, sub_dir);

  snprintf (abs_dir, sizeof (abs_dir), "%s/%s", base_dir, sub_dir);
  abs_dir[sizeof (abs_dir) - 1] = 0;

  return (fs_foreach_dir (abs_dir, scan_plugin_cb, data));
} /* }}} int scan_host_cb */

static int ident_to_rrdfile (const graph_ident_t *ident, /* {{{ */
    dp_rrdtool_t *config,
    char *buffer, size_t buffer_size)
{
  const char *plugin_instance;
  const char *type_instance;

  plugin_instance = ident_get_plugin_instance (ident);
  if ((plugin_instance != NULL) && (plugin_instance[0] == 0))
    plugin_instance = NULL;

  type_instance = ident_get_type_instance (ident);
  if ((type_instance != NULL) && (type_instance[0] == 0))
    type_instance = NULL;

  buffer[0] = 0;

  strlcat (buffer, config->data_dir, buffer_size);
  strlcat (buffer, "/", buffer_size);

  strlcat (buffer, ident_get_host (ident), buffer_size);
  strlcat (buffer, "/", buffer_size);
  strlcat (buffer, ident_get_plugin (ident), buffer_size);
  if (plugin_instance != NULL)
  {
    strlcat (buffer, "-", buffer_size);
    strlcat (buffer, plugin_instance, buffer_size);
  }
  strlcat (buffer, "/", buffer_size);
  strlcat (buffer, ident_get_type (ident), buffer_size);
  if (type_instance != NULL)
  {
    strlcat (buffer, "-", buffer_size);
    strlcat (buffer, type_instance, buffer_size);
  }

  strlcat (buffer, ".rrd", buffer_size);

  return (0);
} /* }}} int ident_to_rrdfile */

/*
 * Callback functions
 */
static int get_idents (void *priv,
    dp_get_idents_callback cb, void *ud)
{ /* {{{ */
  dp_rrdtool_t *config = priv;
  dp_get_idents_data_t data;
  int status;

  data.ident = ident_create ("", "", "", "", "");
  if (data.ident == NULL)
    return (ENOMEM);
  data.callback = cb;
  data.user_data = ud;

  status = fs_foreach_dir (config->data_dir, scan_host_cb, &data);

  ident_destroy (data.ident);
  return (status);
} /* }}} int get_idents */

static int get_ident_ds_names (void *priv, graph_ident_t *ident,
    dp_list_get_ident_ds_names_callback cb, void *ud)
{ /* {{{ */
  dp_rrdtool_t *config = priv;
  char file[PATH_MAX + 1];
  int status;

  char *rrd_argv[] = { "info", file, NULL };
  int rrd_argc = (sizeof (rrd_argv) / sizeof (rrd_argv[0])) - 1;

  rrd_info_t *info;
  rrd_info_t *ptr;

  memset (file, 0, sizeof (file));
  status = ident_to_rrdfile (ident, config, file, sizeof (file));
  if (status != 0)
    return (status);

  info = rrd_info (rrd_argc, rrd_argv);
  if (info == NULL)
  {
    fprintf (stderr, "%s: rrd_info (%s) failed.\n", __func__, file);
    fflush (stderr);
    return (-1);
  }

  for (ptr = info; ptr != NULL; ptr = ptr->next)
  {
    size_t keylen;
    size_t dslen;
    char *ds;

    if (ptr->key[0] != 'd')
      continue;

    if (strncmp ("ds[", ptr->key, strlen ("ds[")) != 0)
      continue;

    keylen = strlen (ptr->key);
    if (keylen < strlen ("ds[?].type"))
      continue;

    dslen = keylen - strlen ("ds[].type");
    assert (dslen >= 1);

    if (strcmp ("].type", ptr->key + (strlen ("ds[") + dslen)) != 0)
      continue;

    ds = malloc (dslen + 1);
    if (ds == NULL)
      continue;

    memcpy (ds, ptr->key + strlen ("ds["), dslen);
    ds[dslen] = 0;

    status = (*cb) (ident, ds, ud);

    free (ds);

    if (status != 0)
      break;
  }

  rrd_info_free (info);

  return (status);
} /* }}} int get_ident_ds_names */

static int get_ident_data (void *priv,
    graph_ident_t *ident, const char *ds_name,
    dp_time_t begin, dp_time_t end,
    dp_get_ident_data_callback cb, void *ud)
{ /* {{{ */
  dp_rrdtool_t *config = priv;

  char filename[PATH_MAX + 1];
  const char *cf = "AVERAGE"; /* FIXME */
  time_t rrd_start;
  time_t rrd_end;
  unsigned long step;
  unsigned long ds_count;
  char **ds_namv;
  rrd_value_t *data;
  int status;

  unsigned long ds_index;
  unsigned long data_index;
  unsigned long data_length;

  dp_data_point_t *dp = NULL;
  size_t dp_num = 0;

  status = ident_to_rrdfile (ident, config, filename, sizeof (filename));
  if (status != 0)
    return (status);

  rrd_start = (time_t) begin.tv_sec;
  rrd_end = (time_t) end.tv_sec;
  step = 0;
  ds_count = 0;
  ds_namv = NULL;
  data = NULL;

  status = rrd_fetch_r (filename, cf,
      &rrd_start, &rrd_end,
      &step, &ds_count, &ds_namv,
      &data);
  if (status != 0)
    return (status);

#define BAIL_OUT(ret_status) do { \
  unsigned long i;                \
  for (i = 0; i < ds_count; i++)  \
    free (ds_namv[i]);            \
  free (ds_namv);                 \
  free (data);                    \
  free (dp);                      \
  return (ret_status);            \
} while (0)

  for (ds_index = 0; ds_index < ds_count; ds_index++)
    if (strcmp (ds_name, ds_namv[ds_index]) == 0)
      break;

  if (ds_index >= ds_count)
    BAIL_OUT (ENOENT);

  /* Number of data points returned. */
  data_length = (rrd_end - rrd_start) / step;

  dp_num = (size_t) data_length;
  dp = calloc (dp_num, sizeof (*dp));
  if (dp == NULL)
    BAIL_OUT (ENOMEM);

  for (data_index = 0; data_index < data_length; data_index++)
  {
    unsigned long index = (ds_count * data_index) + ds_index;

    dp[data_index].time.tv_sec = rrd_start + (data_index * step);
    dp[data_index].time.tv_nsec = 0;
    dp[data_index].value = (double) data[index];
  }

  status = (*cb) (ident, ds_name, dp, dp_num, ud);
  if (status != 0)
    BAIL_OUT (status);

  BAIL_OUT (0);
#undef BAIL_OUT
} /* }}} int get_ident_data */

static int print_graph (void *priv,
    graph_config_t *cfg, graph_instance_t *inst)
{ /* {{{ */
  priv = NULL;
  cfg = NULL;
  inst = NULL;

  return (-1);
} /* }}} int print_graph */

int dp_rrdtool_config (const oconfig_item_t *ci)
{ /* {{{ */
  dp_rrdtool_t *conf;
  int i;

  data_provider_t dp =
  {
    get_idents,
    get_ident_ds_names,
    get_ident_data,
    print_graph,
    /* private_data = */ NULL
  };

  conf = malloc (sizeof (*conf));
  if (conf == NULL)
    return (ENOMEM);
  memset (conf, 0, sizeof (*conf));
  conf->data_dir = NULL;

  for (i = 0; i < ci->children_num; i++)
  {
    oconfig_item_t *child = ci->children + i;

    if (strcasecmp ("DataDir", child->key) == 0)
      graph_config_get_string (child, &conf->data_dir);
    else
    {
      fprintf (stderr, "dp_rrdtool_config: Ignoring unknown config option "
          "\"%s\"\n", child->key);
      fflush (stderr);
    }
  }

  if (conf->data_dir == NULL)
    conf->data_dir = strdup ("/var/lib/collectd/rrd");
  if (conf->data_dir == NULL)
  {
    free (conf);
    return (ENOMEM);
  }

  dp.private_data = conf;

  data_provider_register ("rrdtool", &dp);

  return (0);
} /* }}} int dp_rrdtool_config */

/* vim: set sw=2 sts=2 et fdm=marker : */
