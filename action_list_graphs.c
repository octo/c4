#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <fcgiapp.h>
#include <fcgi_stdio.h>

#include "graph_list.h"

static int print_graph (const graph_list_t *gl, void *user_data) /* {{{ */
{
  _Bool *first;

  if ((gl == NULL) || (user_data == NULL))
    return (EINVAL);

  first = (_Bool *) user_data;
  if (!*first)
    printf (",\n");
  *first = 0;

  printf (" {");

  printf ("\"host\":\"%s\"", gl->host);
      
  printf (",\"plugin\":\"%s\"", gl->plugin);
  if (gl->plugin_instance != NULL)
    printf (",\"plugin_instance\":\"%s\"", gl->plugin_instance);
  else
    printf (",\"plugin_instance\":null");

  printf (",\"type\":\"%s\"", gl->type);
  if (gl->type_instance != NULL)
    printf (",\"type_instance\":\"%s\"", gl->type_instance);
  else
    printf (",\"type_instance\":null");

  printf ("}");

  return (0);
} /* }}} int print_graph */

int action_list_graphs (void) /* {{{ */
{
  _Bool first = 1;

  printf ("Content-Type: text/plain\n\n");

  gl_update ();

  printf ("[\n");
  gl_foreach (print_graph, /* user_data = */ &first);
  printf ("\n]");
} /* }}} int action_list_graphs */

/* vim: set sw=2 sts=2 et fdm=marker : */
