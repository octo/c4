#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "action_show_graph.h"
#include "common.h"
#include "graph.h"
#include "graph_ident.h"
#include "graph_instance.h"
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

static void show_breadcrump_field (const char *str, /* {{{ */
    const char *field_name)
{
  if ((str == NULL) || (str[0] == 0))
    printf ("<em>none</em>");
  else if (IS_ANY (str))
    printf ("<em>any</em>");
  else if (IS_ALL (str))
    printf ("<em>all</em>");
  else
  {
    char *str_html = html_escape (str);

    if (field_name != NULL)
      printf ("<a href=\"%s?action=list_graphs;q=%s:%s\">%s</a>",
          script_name (), field_name, str_html, str_html);
    else
      printf ("<a href=\"%s?action=list_graphs;q=%s\">%s</a>",
          script_name (), str_html, str_html);

    free (str_html);
  }
} /* }}} void show_breadcrump_field */

static int show_breadcrump (show_graph_data_t *data) /* {{{ */
{
  graph_ident_t *ident;
  char *prefix;

  if (data->inst != NULL)
  {
    prefix = "Instance";
    ident = inst_get_selector (data->inst);
  }
  else
  {
    prefix = "Graph";
    ident = graph_get_selector (data->cfg);
  }

  printf ("<div class=\"breadcrump\">%s: &quot;", prefix);
  show_breadcrump_field (ident_get_host (ident), "host");
  printf ("&nbsp;/ ");
  show_breadcrump_field (ident_get_plugin (ident), "plugin");
  printf ("&nbsp;&ndash; ");
  show_breadcrump_field (ident_get_plugin_instance (ident), "plugin_instance");
  printf ("&nbsp;/ ");
  show_breadcrump_field (ident_get_type (ident), "type");
  printf ("&nbsp;&ndash; ");
  show_breadcrump_field (ident_get_type_instance (ident), "type_instance");
  printf ("&quot;</div>\n");

  return (0);
} /* }}} int show_breadcrump */

static int show_time_selector (__attribute__((unused)) void *user_data) /* {{{ */
{
  param_list_t *pl;

  pl = param_create (/* query string = */ NULL);
  param_set (pl, "begin", NULL);
  param_set (pl, "end", NULL);
  param_set (pl, "button", NULL);

  printf ("<form action=\"%s\" method=\"get\">\n", script_name ());

  param_print_hidden (pl);

  printf ("  <select name=\"begin\">\n"
      "    <option value=\"-3600\">Hour</option>\n"
      "    <option value=\"-86400\">Day</option>\n"
      "    <option value=\"-604800\">Week</option>\n"
      "    <option value=\"-2678400\">Month</option>\n"
      "    <option value=\"-31622400\">Year</option>\n"
      "  </select>\n"
      "  <input type=\"submit\" name=\"button\" value=\"Go\" />\n");

  printf ("</form>\n");

  param_destroy (pl);

  return (0);
} /* }}} int show_time_selector */

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

  printf ("    <li class=\"instance\">"
      "<a href=\"%s?action=show_graph;%s\">%s</a></li>\n",
      script_name (), params, descr);

  return (0);
} /* }}} int show_instance_list_cb */

static int show_instance_list (void *user_data) /* {{{ */
{
  show_graph_data_t *data = user_data;
  char title[128];
  char params[1024];

  memset (title, 0, sizeof (title));
  graph_get_title (data->cfg, title, sizeof (title));
  html_escape_buffer (title, sizeof (title));

  memset (params, 0, sizeof (params));
  graph_get_params (data->cfg, params, sizeof (params));
  html_escape_buffer (params, sizeof (params));

  printf ("<ul class=\"graph_list\">\n"
      "  <li class=\"graph\"><a href=\"%s?action=show_graph;%s\">%s</a>\n"
      "  <ul class=\"instance_list\">\n",
      script_name (), params, title);

  graph_inst_foreach (data->cfg, show_instance_list_cb, user_data);

  printf ("  </ul>\n"
      "</ul>\n");

  return (0);
} /* }}} int show_instance_list */

static int show_instance (void *user_data) /* {{{ */
{
  show_graph_data_t *data = user_data;
  char title[128];
  char descr[128];
  param_list_t *pl;
  char *params;
  char params_html[1024];

  show_breadcrump (data);

  memset (title, 0, sizeof (title));
  graph_get_title (data->cfg, title, sizeof (title));
  html_escape_buffer (title, sizeof (title));

  memset (descr, 0, sizeof (descr));
  inst_describe (data->cfg, data->inst, descr, sizeof (descr));
  html_escape_buffer (descr, sizeof (descr));

  pl = param_create (/* query string = */ NULL);
  param_set (pl, "action", "graph");
  param_set (pl, "button", NULL);

  params = param_as_string (pl);
  if (params == NULL)
  {
    printf ("<div class=\"error\">param_as_string failed.</div>\n");
    param_destroy (pl);
    return (-1);
  }

  memset (params_html, 0, sizeof (params_html));
  html_escape_copy (params_html, params, sizeof (params_html));

  printf ("<div class=\"graph-img\"><img src=\"%s?%s\" "
      "title=\"%s / %s\" /></div>\n",
      script_name (), params, title, descr);

  param_destroy (pl);
  free (params);
  return (0);
} /* }}} int show_instance */

static int show_graph (void *user_data) /* {{{ */
{
  show_graph_data_t *data = user_data;

  show_breadcrump (data);
  return (show_instance_list (user_data));
} /* }}} int show_graph */

int action_show_graph (void) /* {{{ */
{
  page_callbacks_t pg_callbacks = PAGE_CALLBACKS_INIT;
  show_graph_data_t pg_data;

  char title[128];

  pg_data.cfg = gl_graph_get_selected ();
  if (pg_data.cfg == NULL)
    OUTPUT_ERROR ("gl_graph_get_selected () failed.\n");

  memset (title, 0, sizeof (title));
  graph_get_title (pg_data.cfg, title, sizeof (title));

  pg_data.inst = inst_get_selected (pg_data.cfg);
  if (pg_data.inst != NULL)
  {
    char descr[128];
    char html_title[128];

    memset (descr, 0, sizeof (descr));
    inst_describe (pg_data.cfg, pg_data.inst, descr, sizeof (descr));

    snprintf (html_title, sizeof (html_title), "Graph \"%s / %s\"",
        title, descr);
    html_title[sizeof (html_title) - 1] = 0;

    pg_callbacks.top_right = html_print_search_box;
    pg_callbacks.middle_center = show_instance;
    pg_callbacks.middle_left = show_instance_list;
    pg_callbacks.middle_right = show_time_selector;

    html_print_page (html_title, &pg_callbacks, &pg_data);
  }
  else /* if (pg_data.inst == NULL) */
  {
    pg_callbacks.top_right = html_print_search_box;
    pg_callbacks.middle_center = show_graph;

    html_print_page (title, &pg_callbacks, &pg_data);
  }

  return (0);
} /* }}} int action_graph */

/* vim: set sw=2 sts=2 et fdm=marker : */
