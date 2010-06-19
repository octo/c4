#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "action_list_graphs.h"
#include "graph.h"
#include "graph_ident.h"
#include "graph_list.h"
#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

static int print_graph_inst_json (__attribute__((unused)) graph_config_t *cfg, /* {{{ */
    graph_instance_t *inst,
    void *user_data)
{
  _Bool *first;
  graph_ident_t *ident;
  char *json;

  first = user_data;

  ident = inst_get_selector (inst);
  if (ident == NULL)
    return (-1);

  json = ident_to_json (ident);
  if (json == NULL)
  {
    ident_destroy (ident);
    return (ENOMEM);
  }

  if (*first)
    printf ("%s", json);
  else
    printf (",\n%s", json);

  *first = 0;

  ident_destroy (ident);
  return (0);
} /* }}} int print_graph_inst_json */

static int print_graph_json (graph_config_t *cfg, /* {{{ */
    void *user_data)
{
  return (gl_graph_instance_get_all (cfg, print_graph_inst_json, user_data));
} /* }}} int print_graph_json */

static int list_graphs_json (void) /* {{{ */
{
  _Bool first = 1;

  printf ("Content-Type: application/json\n\n");

  printf ("[\n");
  gl_graph_get_all (print_graph_json, /* user_data = */ &first);
  printf ("\n]");

  return (0);
} /* }}} int list_graphs_json */

struct callback_data_s
{
  graph_config_t *cfg;
  int limit;
};
typedef struct callback_data_s callback_data_t;

static int print_graph_inst_html (graph_config_t *cfg, /* {{{ */
    graph_instance_t *inst,
    void *user_data)
{
  callback_data_t *data = user_data;
  char params[1024];
  char desc[1024];

  if (data->cfg != cfg)
  {
    if (data->cfg != NULL)
      printf ("  </ul></li>\n");

    memset (desc, 0, sizeof (desc));
    graph_get_title (cfg, desc, sizeof (desc));

    printf ("  <li>%s\n  <ul>\n", desc);

    data->cfg = cfg;
  }

  memset (params, 0, sizeof (params));
  inst_get_params (cfg, inst, params, sizeof (params));

  memset (desc, 0, sizeof (desc));
  inst_describe (cfg, inst, desc, sizeof (desc));

  printf ("    <li><a href=\"%s?action=graph;%s\">%s</a></li>\n",
      script_name (), params, desc);

  if (data->limit > 0)
    data->limit--;

  /* Abort scan if limit is reached. */
  if (data->limit == 0)
    return (1);

  return (0);
} /* }}} int print_graph_inst_html */

static int list_graphs_html (const char *term) /* {{{ */
{
  callback_data_t data = { NULL, /* limit = */ 20 };
  printf ("Content-Type: text/html\n\n");

  printf ("<html>\n  <head>\n");
  if (term != NULL)
    printf ("    <title>c4: Graphs matching &quot;%s&quot;</title>\n", term);
  else
    printf ("    <title>c4: List of all graphs</title>\n");
  printf ("  </head>\n  <body>\n");

  printf ("<form action=\"%s\" method=\"get\">\n"
      "  <input type=\"hidden\" name=\"action\" value=\"list_graphs\" />\n"
      "  <input type=\"text\" name=\"search\" value=\"%s\" />\n"
      "  <input type=\"submit\" name=\"button\" value=\"Search\" />\n"
      "</form>\n",
      script_name (), (term != NULL) ? term : "");

  printf ("    <ul>\n");
  if (term == NULL)
    gl_instance_get_all (print_graph_inst_html, /* user_data = */ &data);
  else
    gl_search (term, print_graph_inst_html, /* user_data = */ &data);

  if (data.cfg != NULL)
    printf ("      </ul></li>\n");

  printf ("    </ul>\n");

  printf ("  </body>\n</html>\n");

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
    return (list_graphs_html (param ("search")));
} /* }}} int action_list_graphs */

/* vim: set sw=2 sts=2 et fdm=marker : */
