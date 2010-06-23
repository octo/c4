#ifndef GRAPH_INSTANCE_H
#define GRAPH_INSTANCE_H 1

#include <time.h>

#include "graph_types.h"
#include "graph_ident.h"
#include "utils_array.h"

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

int inst_compare_ident (graph_instance_t *inst, const graph_ident_t *ident);

_Bool inst_matches_ident (graph_instance_t *inst, const graph_ident_t *ident);

_Bool inst_matches_string (graph_config_t *cfg, graph_instance_t *inst,
    const char *term);

_Bool inst_matches_field (graph_instance_t *inst,
    graph_ident_field_t field, const char *field_value);

int inst_describe (graph_config_t *cfg, graph_instance_t *inst,
    char *buffer, size_t buffer_size);

time_t inst_get_mtime (graph_instance_t *inst);

#endif /* GRAPH_INSTANCE_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
