#ifndef FILESYSTEM_G
#define FILESYSTEM_G 1

#include "graph_ident.h"

typedef int (*fs_ident_cb_t) (const graph_ident_t *ident, void *user_data);

int fs_scan (fs_ident_cb_t callback, void *user_data);

#endif /* FILESYSTEM_G */
/* vim: set sw=2 sts=2 et fdm=marker : */
