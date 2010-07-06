/**
 * collection4 - graph_types.h
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

#ifndef GRAPH_TYPES_H
#define GRAPH_TYPES_H 1

/*
 * Opaque types
 */
struct graph_config_s;
typedef struct graph_config_s graph_config_t;

struct graph_def_s;
typedef struct graph_def_s graph_def_t;

struct graph_ident_s;
typedef struct graph_ident_s graph_ident_t;

struct graph_instance_s;
typedef struct graph_instance_s graph_instance_t;

/*
 * Callback types
 */
typedef int (*graph_callback_t) (graph_config_t *cfg,
    void *user_data);

typedef int (*graph_inst_callback_t) (graph_config_t *cfg,
    graph_instance_t *inst, void *user_data);

typedef int (*def_callback_t) (graph_def_t *def,
    void *user_data);

typedef int (*inst_callback_t) (graph_instance_t *inst,
		void *user_data);

#endif /* GRAPH_TYPES_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
