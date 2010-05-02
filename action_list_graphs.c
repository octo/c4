#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <fcgiapp.h>
#include <fcgi_stdio.h>

#include "graph_list.h"
#include "utils_params.h"

static int print_graph_json (const graph_list_t *gl, void *user_data) /* {{{ */
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
} /* }}} int print_graph_json */

static int print_graph_html (const graph_list_t *gl,
    void __attribute__((unused)) *user_data)
{
  if (gl == NULL)
    return (EINVAL);

  printf ("<li>%s/%s", gl->host, gl->plugin);
  if (gl->plugin_instance != NULL)
    printf ("-%s", gl->plugin_instance);
  printf ("/%s", gl->type);
  if (gl->type_instance != NULL)
    printf ("-%s", gl->type_instance);
  printf ("</li>\n");

  return (0);
}

static int list_graphs_json (void) /* {{{ */
{
  _Bool first = 1;

  printf ("Content-Type: application/json\n\n");

  printf ("[\n");
  gl_foreach (print_graph_json, /* user_data = */ &first);
  printf ("\n]");

  return (0);
} /* }}} int list_graphs_json */

static int list_graphs_html (void) /* {{{ */
{
  printf ("Content-Type: text/html\n\n");

  printf ("<ul>\n");
  gl_foreach (print_graph_html, /* user_data = */ NULL);
  printf ("</ul>\n");

  return (0);
} /* }}} int list_graphs_html */

int action_list_graphs (void) /* {{{ */
{
  const char *format;

  gl_update ();

  format = param ("format");
  if (format == NULL)
    format = "html";

  if (strcmp ("json", format) == 0)
    return (list_graphs_json ());
  else
    return (list_graphs_html ());
} /* }}} int action_list_graphs */

/* vim: set sw=2 sts=2 et fdm=marker : */
