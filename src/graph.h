/**
 * collection4 - graph.h
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

#ifndef GRAPH_H
#define GRAPH_H 1

#include "graph_types.h"
#include "graph_ident.h"
#include "oconfig.h"
#include "rrd_args.h"
#include "utils_array.h"

/*
 * Functions
 */
graph_config_t *graph_create (const graph_ident_t *selector);

void graph_destroy (graph_config_t *graph);

int graph_config_add (const oconfig_item_t *ci);

int graph_add_file (graph_config_t *cfg, const graph_ident_t *file);

int graph_get_title (graph_config_t *cfg,
    char *buffer, size_t buffer_size);

int graph_get_params (graph_config_t *cfg, char *buffer, size_t buffer_size);

graph_ident_t *graph_get_selector (graph_config_t *cfg);

graph_def_t *graph_get_defs (graph_config_t *cfg);

int graph_add_def (graph_config_t *cfg, graph_def_t *def);

/* Returns true if the given "ident" matches the (more general) selector of
 * the graph "cfg". */
_Bool graph_ident_matches (graph_config_t *cfg, const graph_ident_t *ident);

/* Returns true if the selector of the graph "cfg" matches the more general
 * ident "selector". */
_Bool graph_matches_ident (graph_config_t *cfg,
    const graph_ident_t *selector);

/* Compares the given string with the appropriate field of the selector. If the
 * selector field is "/all/" or "/any/", returns true without checking the
 * instances. See "graph_inst_search_field" for finding all matching instances.
 * */
_Bool graph_matches_field (graph_config_t *cfg,
    graph_ident_field_t field, const char *field_value);

int graph_inst_foreach (graph_config_t *cfg,
    inst_callback_t cb, void *user_data);

graph_instance_t *graph_inst_find_exact (graph_config_t *cfg,
    graph_ident_t *ident);

graph_instance_t *graph_inst_find_matching (graph_config_t *cfg,
    const graph_ident_t *ident);

int graph_inst_find_all_matching (graph_config_t *cfg,
    const graph_ident_t *ident,
    graph_inst_callback_t callback, void *user_data);

int graph_inst_search (graph_config_t *cfg, const char *term,
    graph_inst_callback_t callback, void *user_data);

/* Iterates over all instances and calls "inst_matches_field". If that method
 * returns true, calls the callback with the graph and instance pointers. */
int graph_inst_search_field (graph_config_t *cfg,
    graph_ident_field_t field, const char *field_value,
    graph_inst_callback_t callback, void *user_data);

int graph_compare (graph_config_t *cfg, const graph_ident_t *ident);

size_t graph_num_instances (graph_config_t *cfg);

int graph_sort_instances (graph_config_t *cfg);

int graph_clear_instances (graph_config_t *cfg);

int graph_get_rrdargs (graph_config_t *cfg, graph_instance_t *inst,
    rrd_args_t *args);

#endif /* GRAPH_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
