/**
 * collection4 - utils_search.h
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

#ifndef UTILS_SEARCH_H
#define UTILS_SEARCH_H 1

#include "graph_types.h"

struct search_info_s;
typedef struct search_info_s search_info_t;

search_info_t *search_parse (const char *search);
void search_destroy (search_info_t *si);

/* Returns true if at least one of the ident fields is defined (not a
 * wildcard), false otherwise. If no field has been specified, searching is a
 * lot easier. */
_Bool search_has_selector (search_info_t *si);

graph_ident_t *search_to_ident (search_info_t *si);

_Bool search_graph_title_matches (search_info_t *si, const char *title);

_Bool search_graph_inst_matches (search_info_t *si,
    graph_config_t *cfg, graph_instance_t *inst,
    const char *title);

#endif /* UTILS_SEARCH_H */
/* vim: set sw=2 sts=2 et fdm=marker : */

