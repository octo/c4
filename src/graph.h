#ifndef GRAPH_H
#define GRAPH_H 1

#include "graph_types.h"
#include "oconfig.h"
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

_Bool graph_matches (graph_config_t *cfg, const graph_ident_t *ident);

int graph_inst_foreach (graph_config_t *cfg,
    inst_callback_t cb, void *user_data);

graph_instance_t *graph_inst_find_exact (graph_config_t *cfg,
    graph_ident_t *ident);

graph_instance_t *graph_inst_find_matching (graph_config_t *cfg,
    const graph_ident_t *ident);

int graph_inst_search (graph_config_t *cfg, const char *term,
    graph_inst_callback_t callback, void *user_data);

int graph_compare (graph_config_t *cfg, const graph_ident_t *ident);

int graph_clear_instances (graph_config_t *cfg);

int graph_get_rrdargs (graph_config_t *cfg, graph_instance_t *inst,
    str_array_t *args);

#endif /* GRAPH_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
