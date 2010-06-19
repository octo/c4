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
