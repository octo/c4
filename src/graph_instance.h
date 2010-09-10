/**
 * collection4 - graph_instance.h
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

#ifndef GRAPH_INSTANCE_H
#define GRAPH_INSTANCE_H 1

#include <time.h>

#include <yajl/yajl_gen.h>

#include "graph_types.h"
#include "data_provider.h"
#include "graph_ident.h"
#include "rrd_args.h"
#include "utils_array.h"

/*
 * Methods
 */
graph_instance_t *inst_create (graph_config_t *cfg,
		const graph_ident_t *ident);

void inst_destroy (graph_instance_t *inst);

int inst_add_file (graph_instance_t *inst, const graph_ident_t *file);

graph_instance_t *inst_get_selected (graph_config_t *cfg);

int inst_get_all_selected (graph_config_t *cfg,
    graph_inst_callback_t callback, void *user_data);

int inst_get_params (graph_config_t *cfg, graph_instance_t *inst,
    char *buffer, size_t buffer_size);

int inst_get_rrdargs (graph_config_t *cfg, graph_instance_t *inst,
    rrd_args_t *args);

/* Returns a copy of the selector which must be freed by the caller. */
graph_ident_t *inst_get_selector (graph_instance_t *inst);

int inst_compare (const graph_instance_t *i0, const graph_instance_t *i1);

int inst_compare_ident (graph_instance_t *inst, const graph_ident_t *ident);

/* Returns true if "ident" matches the instance's selector. */
_Bool inst_ident_matches (graph_instance_t *inst, const graph_ident_t *ident);

/* Returns true if the instance's selector matches the (more general)
 * "selector" ident. */
_Bool inst_matches_ident (graph_instance_t *inst,
    const graph_ident_t *selector);

_Bool inst_matches_string (graph_config_t *cfg, graph_instance_t *inst,
    const char *term);

/* Compares the given string with the appropriate field of the selector or, if
 * the selector field is "/all/", iterates over all the files of the instance
 * and checks the appropriate field. Returns true if the field of the selector
 * or of one of the files matches. The string must match entirely but
 * comparison is done case-insensitive. */
_Bool inst_matches_field (graph_instance_t *inst,
    graph_ident_field_t field, const char *field_value);

int inst_to_json (const graph_instance_t *inst, yajl_gen handler);
int inst_data_to_json (const graph_instance_t *inst,
    dp_time_t begin, dp_time_t end,
    yajl_gen handler);

int inst_describe (graph_config_t *cfg, graph_instance_t *inst,
    char *buffer, size_t buffer_size);

time_t inst_get_mtime (graph_instance_t *inst);

#endif /* GRAPH_INSTANCE_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
