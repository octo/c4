#ifndef GRAPH_CONFIG_H
#define GRAPH_CONFIG_H 1

#include "oconfig.h"

int graph_read_config (void);

int graph_config_get_string (const oconfig_item_t *ci, char **ret_str);
int graph_config_get_bool (const oconfig_item_t *ci, _Bool *ret_bool);

/* vim: set sw=2 sts=2 et fdm=marker : */
#endif /* GRAPH_CONFIG_H */
