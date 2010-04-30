#ifndef GRAPH_LIST_H
#define GRAPH_LIST_H 1

struct graph_list_s
{
  char *host;
  char *plugin;
  char *plugin_instance;
  char *type;
  char *type_instance;
};
typedef struct graph_list_s graph_list_t;

typedef int (*gl_callback) (const graph_list_t *, void *);

int gl_update (void);
int gl_foreach (gl_callback callback, void *user_data);


#endif /* GRAPH_LIST_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
