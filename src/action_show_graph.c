#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "action_show_graph.h"
#include "common.h"
#include "graph_list.h"
#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

#define OUTPUT_ERROR(...) do {             \
  printf ("Content-Type: text/plain\n\n"); \
  printf (__VA_ARGS__);                    \
  return (0);                              \
} while (0)

struct show_graph_data_s
{
  graph_config_t *cfg;
  graph_instance_t *inst;
};
typedef struct show_graph_data_s show_graph_data_t;

static int show_instance_list_cb (graph_instance_t *inst, /* {{{ */
    void *user_data)
{
  show_graph_data_t *data = user_data;
  char descr[128];
  char params[1024];

  memset (descr, 0, sizeof (descr));
  inst_describe (data->cfg, inst, descr, sizeof (descr));
  html_escape_buffer (descr, sizeof (descr));

  if (inst == data->inst)
  {
    printf ("    <li class=\"instance\"><strong>%s</strong></li>\n", descr);
    return (0);
  }

  memset (params, 0, sizeof (params));
  inst_get_params (data->cfg, inst, params, sizeof (params));
  html_escape_buffer (params, sizeof (params));

  printf ("    <li class=\"instance\"><a href=\"%s?action=show_graph;%s\">%s</a></li>\n",
      script_name (), params, descr);

  return (0);
} /* }}} int show_instance_list_cb */

static int show_instance_list (void *user_data) /* {{{ */
{
  show_graph_data_t *data = user_data;
  graph_instance_t *inst;
  char title[128];

  memset (title, 0, sizeof (title));
  graph_get_title (data->cfg, title, sizeof (title));
  html_escape_buffer (title, sizeof (title));

  printf ("<ul class=\"graph_list\">\n"
      "  <li class=\"graph\">%s\n"
      "  <ul class=\"instance_list\">\n", title);

  inst = graph_get_instances (data->cfg);
  inst_foreach (inst, show_instance_list_cb, user_data);

  printf ("  </ul>\n"
      "</ul>\n");

  return (0);
} /* }}} int show_instance_list */

static int show_graph (void *user_data) /* {{{ */
{
  show_graph_data_t *data = user_data;
  char title[128];
  char descr[128];
  char params[1024];

  memset (title, 0, sizeof (title));
  graph_get_title (data->cfg, title, sizeof (title));
  html_escape_buffer (title, sizeof (title));

  memset (descr, 0, sizeof (descr));
  inst_describe (data->cfg, data->inst, descr, sizeof (descr));
  html_escape_buffer (descr, sizeof (descr));

  memset (params, 0, sizeof (params));
  inst_get_params (data->cfg, data->inst, params, sizeof (params));
  html_escape_buffer (params, sizeof (params));

  printf ("<div class=\"graph-img\"><img src=\"%s?action=graph;%s\" "
      "title=\"%s / %s\" /></div>\n",
      script_name (), params, title, descr);

  return (0);
} /* }}} int show_graph */

int action_show_graph (void) /* {{{ */
{
  page_callbacks_t pg_callbacks = PAGE_CALLBACKS_INIT;
  show_graph_data_t pg_data;

  char title[128];
  char descr[128];
  char html_title[128];

  pg_data.cfg = gl_graph_get_selected ();
  if (pg_data.cfg == NULL)
    OUTPUT_ERROR ("gl_graph_get_selected () failed.\n");

  pg_data.inst = inst_get_selected (pg_data.cfg);
  if (pg_data.inst == NULL)
    OUTPUT_ERROR ("inst_get_selected (%p) failed.\n", (void *) pg_data.cfg);

  memset (title, 0, sizeof (title));
  graph_get_title (pg_data.cfg, title, sizeof (title));

  memset (descr, 0, sizeof (descr));
  inst_describe (pg_data.cfg, pg_data.inst, descr, sizeof (descr));

  snprintf (html_title, sizeof (html_title), "Graph \"%s / %s\"",
      title, descr);
  html_title[sizeof (html_title) - 1] = 0;

  pg_callbacks.top_right = html_print_search_box;
  pg_callbacks.middle_center = show_graph;
  pg_callbacks.middle_left = show_instance_list;

  html_print_page (html_title, &pg_callbacks, &pg_data);

  return (0);
} /* }}} int action_graph */

/* vim: set sw=2 sts=2 et fdm=marker : */
