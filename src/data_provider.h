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

#ifndef DATA_PROVIDER_H
#define DATA_PROVIDER_H 1

#include "graph_types.h"
#include "oconfig.h"

#include <time.h>

typedef struct timespec dp_time_t;

struct dp_data_point_s
{
  dp_time_t time;
  double value;
};
typedef struct dp_data_point_s dp_data_point_t;

/* Callback passed to the "get_idents" function. */
typedef int (*dp_get_idents_callback) (const graph_ident_t *, void *);

/* Callback passed to the "get_ident_ds_names" function. */
typedef int (*dp_list_get_ident_ds_names_callback) (const graph_ident_t *,
    const char *ds_name, void *);

/* Callback passed to the "get_ident_data" function. */
typedef int (*dp_get_ident_data_callback) (graph_ident_t *, const char *ds_name,
    const dp_data_point_t *, void *);

struct data_provider_s
{
  int (*get_idents) (void *priv, dp_get_idents_callback, void *);
  int (*get_ident_ds_names) (void *priv, graph_ident_t *,
      dp_list_get_ident_ds_names_callback, void *);
  int (*get_ident_data) (void *priv,
      graph_ident_t *, const char *ds_name,
      dp_time_t begin, dp_time_t end,
      dp_get_ident_data_callback, void *);
  /* Optional method: Prints graph to STDOUT, including HTTP header. */
  int (*print_graph) (void *priv, graph_config_t *cfg, graph_instance_t *inst);
  void *private_data;
};
typedef struct data_provider_s data_provider_t;

int data_provider_config (const oconfig_item_t *ci);

#endif /* DATA_PROVIDER_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
