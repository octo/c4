/**
 * collection4 - data_provider.c
 * Copyright (C) 2010  Florian octo Forster
 * Copyright (C) 2011  noris network AG
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
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "data_provider.h"
#include "dp_rrdtool.h"
#include "graph_ident.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

#include <collectd/client.h>

/* TODO: Turn this into an array for multiple data providers. */
static data_provider_t *data_provider = NULL;

static lcc_connection_t *collectd_connection = NULL;

static int data_provider_ident_flush (const graph_ident_t *ident) /* {{{ */
{
  char *ident_str;
  lcc_identifier_t ident_lcc;
  int status;

  if (ident == NULL)
    return (EINVAL);

  ident_str = ident_to_string (ident);
  if (ident_str == NULL)
    return (ENOMEM);

  if (collectd_connection == NULL)
  {
    /* TODO: Make socket path configurable */
    status = lcc_connect (/* path = */ "/var/run/collectd-unixsock",
		    &collectd_connection);
    if (status != 0)
    {
      assert (collectd_connection == NULL);
      fprintf (stderr, "data_provider_ident_flush: lcc_connect failed "
          "with status %i.\n", status);
      return (status);
    }
    assert (collectd_connection != NULL);
  }

  memset (&ident_lcc, 0, sizeof (ident_lcc));
  status = lcc_string_to_identifier (collectd_connection,
      &ident_lcc, ident_str);
  if (status != 0)
  {
    fprintf (stderr, "data_provider_ident_flush: lcc_string_to_identifier "
        "failed: %s (%i)\n",
        lcc_strerror (collectd_connection), status);
    free (ident_str);
    return (status);
  }

  status = lcc_flush (collectd_connection,
      /* write-plugin = */ NULL,
      /* identifier   = */ &ident_lcc,
      /* timeout      = */ -1);
  if (status != 0)
  {
    fprintf (stderr, "data_provider_ident_flush: lcc_flush (\"%s\") failed: %s (%i)\n",
        ident_str, lcc_strerror (collectd_connection), status);
    free (ident_str);

    lcc_disconnect (collectd_connection);
    collectd_connection = NULL;

    return (status);
  }

  /* fprintf (stderr, "data_provider_ident_flush: lcc_flush (\"%s\") succeeded.\n", ident_str); */
  free (ident_str);
  return (0);
} /* }}} int data_provider_ident_flush */

int data_provider_config (const oconfig_item_t *ci) /* {{{ */
{
  /* FIXME: Actually determine which data provider to call. */
  return (dp_rrdtool_config (ci));
} /* }}} int data_provider_config */

int data_provider_register (const char *name, data_provider_t *p) /* {{{ */
{
  fprintf (stderr, "data_provider_register (name = %s, ptr = %p)\n",
      name, (void *) p);

  if (data_provider == NULL)
    data_provider = malloc (sizeof (*data_provider));
  if (data_provider == NULL)
    return (ENOMEM);

  *data_provider = *p;

  return (0);
} /* }}} int data_provider_register */

int data_provider_get_idents (dp_get_idents_callback callback, /* {{{ */
    void *user_data)
{
  int status;

  if (data_provider == NULL)
    return (EINVAL);

  /* TODO: Iterate over all data providers */
  status = data_provider->get_idents (data_provider->private_data,
      callback, user_data);

  return (status);
} /* }}} int data_provider_get_idents */

int data_provider_get_ident_ds_names (graph_ident_t *ident, /* {{{ */
    dp_list_get_ident_ds_names_callback callback, void *user_data)
{
  if (data_provider == NULL)
    return (EINVAL);

  return (data_provider->get_ident_ds_names (data_provider->private_data,
        ident, callback, user_data));
} /* }}} int data_provider_get_ident_ds_names */

int data_provider_get_ident_data (graph_ident_t *ident, /* {{{ */
    const char *ds_name,
    dp_time_t begin, dp_time_t end,
    dp_get_ident_data_callback callback, void *user_data)
{
  if (data_provider == NULL)
    return (EINVAL);

  data_provider_ident_flush (ident);

  return (data_provider->get_ident_data (data_provider->private_data,
        ident, ds_name, begin, end, callback, user_data));
} /* }}} int data_provider_get_ident_data */

/* vim: set sw=2 sts=2 et fdm=marker : */
