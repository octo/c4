/**
 * collection4 - graph_list.h
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

#ifndef GRAPH_LIST_H
#define GRAPH_LIST_H 1

#include "graph_types.h"
#include "graph_ident.h"
#include "utils_search.h"

/*
 * Functions
 */
int gl_add_graph (graph_config_t *cfg);

int gl_config_submit (void);

graph_config_t *gl_graph_get_selected (void);

int gl_graph_get_all (_Bool include_dynamic,
    graph_callback_t callback, void *user_data);

int gl_graph_instance_get_all (graph_config_t *cfg, graph_inst_callback_t callback,
    void *user_data);

int gl_instance_get_all (graph_inst_callback_t callback, void *user_data);

int gl_search (search_info_t *si, graph_inst_callback_t callback,
    void *user_data);

int gl_search_string (const char *search, graph_inst_callback_t callback,
    void *user_data);

int gl_search_field (graph_ident_field_t field, const char *field_value,
    graph_inst_callback_t callback, void *user_data);

int gl_foreach_host (int (*callback) (const char *host, void *user_data),
    void *user_data);

int gl_update (_Bool request_served);

#endif /* GRAPH_LIST_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
