#ifndef GRAPH_LIST_H
#define GRAPH_LIST_H 1

/*
 * Data types
 */
struct graph_config_s;
typedef struct graph_config_s graph_config_t;

#include "graph_def.h"
#include "graph_ident.h"
#include "graph_instance.h"
#include "utils_array.h"
#include "oconfig.h"

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
int graph_config_add (const oconfig_item_t *ci);
int graph_config_submit (void);

int gl_graph_get_all (gl_cfg_callback callback,
    void *user_data);

graph_config_t *graph_get_selected (void);

int gl_graph_get_title (graph_config_t *cfg,
    char *buffer, size_t buffer_size);

graph_ident_t *gl_graph_get_selector (graph_config_t *cfg);

graph_instance_t *gl_graph_get_instances (graph_config_t *cfg);

graph_def_t *gl_graph_get_defs (graph_config_t *cfg);

int gl_graph_add_def (graph_config_t *cfg, graph_def_t *def);

int gl_graph_instance_get_all (graph_config_t *cfg,
    gl_inst_callback callback, void *user_data);

int gl_instance_get_all (gl_inst_callback callback,
    void *user_data);

int gl_update (void);

#endif /* GRAPH_LIST_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
