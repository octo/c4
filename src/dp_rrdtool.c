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

#include "graph_types.h"
#include "graph_ident.h"
#include "graph_list.h"
#include "data_provider.h"
#include "filesystem.h"
#include "oconfig.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

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
    const char *sub_dir, void *ud)
{ /* {{{ */
  dp_get_idents_data_t *data = ud;
  size_t sub_dir_len;
  char type_copy[1024];
  char *type_inst;

  sub_dir_len = strlen (sub_dir);
  if (sub_dir_len < 5)
    return (0);

  /* Ignore files that don't end in ".rrd". */
  if (strcasecmp (".rrd", sub_dir + (sub_dir_len - 4)) != 0)
    return (0);

  strncpy (type_copy, sub_dir, sizeof (type_copy));
  type_copy[sub_dir_len - 4] = 0;

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
  priv = NULL;
  ident = NULL;
  cb = NULL;
  ud = NULL;
  return (0);
} /* }}} int get_ident_ds_names */

static int get_ident_data (void *priv,
    graph_ident_t *ident, const char *ds_name,
    dp_time_t begin, dp_time_t end,
    dp_get_ident_data_callback cb, void *ud)
{ /* {{{ */
  dp_rrdtool_t *config = priv;

  ident = NULL;
  ds_name = NULL;
  begin.tv_sec = 0;
  end.tv_sec = 0;
  cb = NULL;
  ud = NULL;

  config = NULL;

  return (EINVAL);
} /* }}} int get_ident_data */

static int print_graph (void *priv,
    graph_config_t *cfg, graph_instance_t *inst)
{ /* {{{ */
  priv = NULL;
  cfg = NULL;
  inst = NULL;

  return (-1);
} /* }}} int print_graph */

int dp_rrdtool_config (oconfig_item_t *ci)
{ /* {{{ */
  dp_rrdtool_t *conf;

  data_provider_t dp =
  {
    get_idents,
    get_ident_ds_names,
    get_ident_data,
    print_graph,
    /* private_data = */ NULL
  };

  /* FIXME: Actuelly do config parsing here. */
  ci = NULL; /* FIXME */
  conf = malloc (sizeof (dp_rrdtool_t));
  conf->data_dir = strdup ("/var/lib/collectd/rrd");

  dp.private_data = conf;

  gl_register_data_provider ("rrdtool", &dp);

  return (0);
} /* }}} int dp_rrdtool_config */

/* vim: set sw=2 sts=2 et fdm=marker : */
