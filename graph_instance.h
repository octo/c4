#ifndef GRAPH_INSTANCE_H
#define GRAPH_INSTANCE_H 1

/*
 * Data types
 */
struct graph_instance_s;
typedef struct graph_instance_s graph_instance_t;

typedef int (*inst_callback_t) (graph_instance_t *inst, void *user_data);

#include "graph.h"
#include "utils_array.h"

/*
 * Callback types
 */
/*
 * Methods
 */
graph_instance_t *inst_create (graph_config_t *cfg,
		const graph_ident_t *ident);

void inst_destroy (graph_instance_t *inst);

int inst_add_file (graph_instance_t *inst, const graph_ident_t *file);

graph_instance_t *inst_get_selected (graph_config_t *cfg);

int inst_get_params (graph_config_t *cfg, graph_instance_t *inst,
    char *buffer, size_t buffer_size);

int inst_get_rrdargs (graph_config_t *cfg, graph_instance_t *inst,
    str_array_t *args);

graph_ident_t *inst_get_selector (graph_instance_t *inst);

int inst_append (graph_instance_t *head, graph_instance_t *inst);

int inst_foreach (graph_instance_t *inst,
		inst_callback_t cb, void *user_data);

graph_instance_t *inst_find_matching (graph_instance_t *inst,
    const graph_ident_t *ident);

int inst_describe (graph_config_t *cfg, graph_instance_t *inst,
    char *buffer, size_t buffer_size);

#endif /* GRAPH_INSTANCE_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
