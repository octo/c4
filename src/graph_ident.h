/**
 * collection4 - graph_ident.h
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

#ifndef GRAPH_IDENT_H
#define GRAPH_IDENT_H 1

#include <time.h>
#include "graph_types.h"

#define ANY_TOKEN "/any/"
#define ALL_TOKEN "/all/"

#define IS_ANY(str) (((str) != NULL) && (strcasecmp (ANY_TOKEN, (str)) == 0))
#define IS_ALL(str) (((str) != NULL) && (strcasecmp (ALL_TOKEN, (str)) == 0))

enum graph_ident_field_e
{
  GIF_HOST,
  GIF_PLUGIN,
  GIF_PLUGIN_INSTANCE,
  GIF_TYPE,
  GIF_TYPE_INSTANCE,
  _GIF_LAST
};
typedef enum graph_ident_field_e graph_ident_field_t;

graph_ident_t *ident_create (const char *host,
    const char *plugin, const char *plugin_instance,
    const char *type, const char *type_instance);
graph_ident_t *ident_clone (const graph_ident_t *ident);

#define IDENT_FLAG_REPLACE_ALL 0x01
#define IDENT_FLAG_REPLACE_ANY 0x02
graph_ident_t *ident_copy_with_selector (const graph_ident_t *selector,
    const graph_ident_t *ident, unsigned int flags);

void ident_destroy (graph_ident_t *ident);

const char *ident_get_host (const graph_ident_t *ident);
const char *ident_get_plugin (const graph_ident_t *ident);
const char *ident_get_plugin_instance (const graph_ident_t *ident);
const char *ident_get_type (const graph_ident_t *ident);
const char *ident_get_type_instance (const graph_ident_t *ident);
const char *ident_get_field (const graph_ident_t *ident,
    graph_ident_field_t field);

int ident_set_host (graph_ident_t *ident, const char *host);
int ident_set_plugin (graph_ident_t *ident, const char *plugin);
int ident_set_plugin_instance (graph_ident_t *ident,
    const char *plugin_instance);
int ident_set_type (graph_ident_t *ident, const char *type);
int ident_set_type_instance (graph_ident_t *ident,
    const char *type_instance);

int ident_compare (const graph_ident_t *i0,
    const graph_ident_t *i1);

_Bool ident_matches (const graph_ident_t *selector,
    const graph_ident_t *ident);

/* The "ident_intersect" function takes two selectors and returns true if a
 * file may apply to both selectors. If the selectors contradict one another,
 * for example one issuing "plugin = cpu" and the other "plugin = memory",
 * then false is returned. */
_Bool ident_intersect (const graph_ident_t *s0, const graph_ident_t *s1);

char *ident_to_string (const graph_ident_t *ident);
char *ident_to_file (const graph_ident_t *ident);
char *ident_to_json (const graph_ident_t *ident);

int ident_describe (const graph_ident_t *ident, const graph_ident_t *selector,
    char *buffer, size_t buffer_size);

time_t ident_get_mtime (const graph_ident_t *ident);

/* vim: set sw=2 sts=2 et fdm=marker : */
#endif /* GRAPH_IDENT_H */
