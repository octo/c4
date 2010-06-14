#ifndef COMMON_H
#define COMMON_H 1

#define DATA_DIR "/var/lib/collectd/rrd"

#include "graph_list.h"

typedef int (*callback_type_t)   (const char *type,   void *user_data);
typedef int (*callback_plugin_t) (const char *plugin, void *user_data);
typedef int (*callback_host_t)   (const char *host,   void *user_data);

int foreach_type (const char *host, const char *plugin,
    callback_type_t, void *user_data);
int foreach_plugin (const char *host, callback_plugin_t, void *user_data);
int foreach_host (callback_host_t, void *user_data);

size_t c_strlcat (char *dst, const char *src, size_t size);
#define strlcat c_strlcat

int ds_list_from_rrd_file (char *file,
    size_t *ret_dses_num, char ***ret_dses);

#endif /* COMMON_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
