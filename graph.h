#ifndef GRAPH_H
#define GRAPH_H 1

/*
 * Data types
 */
struct graph_config_s;
typedef struct graph_config_s graph_config_t;

#include "graph_def.h"
#include "graph_ident.h"
#include "graph_instance.h"
#include "oconfig.h"

/*
 * Functions
 */
graph_config_t *graph_create (const graph_ident_t *selector);

void graph_destroy (graph_config_t *graph);

int graph_config_add (const oconfig_item_t *ci);

int graph_add_file (graph_config_t *cfg, const graph_ident_t *file);

int graph_get_title (graph_config_t *cfg,
    char *buffer, size_t buffer_size);

graph_ident_t *graph_get_selector (graph_config_t *cfg);

graph_instance_t *graph_get_instances (graph_config_t *cfg);

graph_def_t *graph_get_defs (graph_config_t *cfg);

int graph_add_def (graph_config_t *cfg, graph_def_t *def);

_Bool graph_matches (graph_config_t *cfg, const graph_ident_t *ident);

int graph_compare (graph_config_t *cfg, const graph_ident_t *ident);

int graph_clear_instances (graph_config_t *cfg);

#endif /* GRAPH_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
