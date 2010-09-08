/**
 * collection4 - graph_def.h
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

#ifndef GRAPH_DEF_H
#define GRAPH_DEF_H 1

#include <yajl/yajl_gen.h>

#include "graph_types.h"
#include "utils_array.h"
#include "oconfig.h"
#include "rrd_args.h"

graph_def_t *def_create (graph_config_t *cfg, graph_ident_t *ident,
    const char *ds_name);

void def_destroy (graph_def_t *def);

int def_config (graph_config_t *cfg, const oconfig_item_t *ci);

int def_append (graph_def_t *head, graph_def_t *def);

graph_def_t *def_search (graph_def_t *head, graph_ident_t *ident,
    const char *ds_name);

_Bool def_matches (graph_def_t *def, graph_ident_t *ident);

int def_foreach (graph_def_t *def, def_callback_t callback, void *user_data);

int def_get_rrdargs (graph_def_t *def, graph_ident_t *ident,
    rrd_args_t *args);

int def_to_json (const graph_def_t *def, yajl_gen handler);

/* vim: set sw=2 sts=2 et fdm=marker : */
#endif
