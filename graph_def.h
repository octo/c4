#ifndef GRAPH_DEF_H
#define GRAPH_DEF_H 1

#include "graph_ident.h"
#include "utils_array.h"
#include "graph_list.h"

struct graph_def_s;
typedef struct graph_def_s graph_def_t;

typedef int (*def_callback_t) (graph_def_t *def,
    void *user_data);

graph_def_t *def_create (graph_config_t *cfg, graph_ident_t *ident,
    const char *ds_name);

void def_destroy (graph_def_t *def);

int def_append (graph_def_t *head, graph_def_t *def);

graph_def_t *def_search (graph_def_t *head, graph_ident_t *ident,
    const char *ds_name);

_Bool def_matches (graph_def_t *def, graph_ident_t *ident);

int def_foreach (graph_def_t *def, def_callback_t callback, void *user_data);

int def_get_rrdargs (graph_def_t *def, graph_ident_t *ident,
    str_array_t *args);

/* vim: set sw=2 sts=2 et fdm=marker : */
#endif
