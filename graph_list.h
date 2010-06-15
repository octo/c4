#ifndef GRAPH_LIST_H
#define GRAPH_LIST_H 1

#include "graph_instance.h"

/*
 * Callback types
 */
typedef int (*gl_cfg_callback) (graph_config_t *cfg,
    void *user_data);

typedef int (*gl_inst_callback) (graph_config_t *cfg,
    graph_instance_t *inst, void *user_data);

/*
 * Functions
 */
int gl_add_graph (graph_config_t *cfg);

int gl_graph_get_all (gl_cfg_callback callback,
    void *user_data);

graph_config_t *graph_get_selected (void);

int gl_graph_instance_get_all (graph_config_t *cfg,
    gl_inst_callback callback, void *user_data);

int gl_instance_get_all (gl_inst_callback callback,
    void *user_data);

int gl_update (void);

#endif /* GRAPH_LIST_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
