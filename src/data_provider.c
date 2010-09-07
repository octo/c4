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

#include "data_provider.h"
#include "dp_rrdtool.h"

int data_provider_config (const oconfig_item_t *ci) /* {{{ */
{
  /* FIXME: Actually determine which data provider to call. */
  return (dp_rrdtool_config (ci));
} /* }}} int data_provider_config */

/* vim: set sw=2 sts=2 et fdm=marker : */
