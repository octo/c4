#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <fcgiapp.h>
#include <fcgi_stdio.h>

#include "action_list_graphs.h"
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

static int print_graph_inst_html (graph_config_t *cfg, /* {{{ */
    graph_instance_t *inst,
    __attribute__((unused)) void *user_data)
{
  char buffer[1024];

  memset (buffer, 0, sizeof (buffer));
  gl_instance_get_params (cfg, inst, buffer, sizeof (buffer));

  printf ("<li><a href=\"test.fcgi?action=graph;%s\">%s</a></li>\n", buffer, buffer);

  return (0);
} /* }}} int print_graph_inst_html */

static int print_graph_html (graph_config_t *cfg, /* {{{ */
    __attribute__((unused)) void *user_data)
{
  char buffer[1024];

  memset (buffer, 0, sizeof (buffer));
  gl_graph_get_title (cfg, buffer, sizeof (buffer));

  printf ("<li>%s\n<ul>\n", buffer);
  gl_graph_instance_get_all (cfg, print_graph_inst_html, /* user_data = */ NULL);
  printf ("</ul>\n");

  return (0);
} /* }}} int print_graph_html */

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
  gl_graph_get_all (print_graph_html, /* user_data = */ NULL);
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
