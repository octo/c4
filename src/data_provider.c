/**
 * collection4 - data_provider.c
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
#include <errno.h>

#include "data_provider.h"
#include "dp_rrdtool.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

/* TODO: Turn this into an array for multiple data providers. */
static data_provider_t *data_provider = NULL;

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

/* vim: set sw=2 sts=2 et fdm=marker : */
