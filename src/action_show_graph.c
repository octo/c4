/**
 * collection4 - action_show_graph.c
 * Copyright (C) 2010  Florian octo Forster
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors:
 *   Florian octo Forster <ff at octo.it>
 **/

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

#define MAX_SHOW_GRAPHS 10

struct show_graph_data_s
{
  graph_config_t *cfg;
  const char *search_term;
};
typedef struct show_graph_data_s show_graph_data_t;

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

static int left_menu (void *user_data) /* {{{ */
{
  show_graph_data_t *data = user_data;

  printf ("\n<ul class=\"menu left\">\n");

  if ((data->search_term != NULL) && (data->search_term[0] != 0))
  {
    char params[1024];

    memset (params, 0, sizeof (params));
    graph_get_params (data->cfg, params, sizeof (params));
    html_escape_buffer (params, sizeof (params));

    printf ("  <li><a href=\"%s?action=show_graph;%s\">"
        "All instances</a></li>\n",
        script_name (), params);
  }

  printf ("  <li><a href=\"%s?action=list_graphs\">All graphs</a></li>\n"
      "</ul>\n",
      script_name ());

  return (0);
} /* }}} int left_menu */

static int show_instance_cb (graph_instance_t *inst, /* {{{ */
    void *user_data)
{
  show_graph_data_t *data = user_data;
  char descr[128];
  char params[1024];

  if ((data->search_term != NULL) && (data->search_term[0] != 0))
  {
    _Bool matches = 0;
    char *term_lc = strtolower_copy (data->search_term);

    if (strncmp ("host:", term_lc, strlen ("host:")) == 0)
    {
      if (inst_matches_field (inst, GIF_HOST, term_lc + strlen ("host:")))
        matches = 1;
    }
    else if (strncmp ("plugin:", term_lc, strlen ("plugin:")) == 0)
    {
      if (inst_matches_field (inst, GIF_PLUGIN, term_lc + strlen ("plugin:")))
        matches = 1;
    }
    else if (strncmp ("plugin_instance:", term_lc,
          strlen ("plugin_instance:")) == 0)
    {
      if (inst_matches_field (inst, GIF_PLUGIN_INSTANCE,
            term_lc + strlen ("plugin_instance:")))
        matches = 1;
    }
    else if (strncmp ("type:", term_lc, strlen ("type:")) == 0)
    {
      if (inst_matches_field (inst, GIF_TYPE, term_lc + strlen ("type:")))
        matches = 1;
    }
    else if (strncmp ("type_instance:", term_lc,
          strlen ("type_instance:")) == 0)
    {
      if (inst_matches_field (inst, GIF_TYPE_INSTANCE,
            term_lc + strlen ("type_instance:")))
        matches = 1;
    }
    else if (inst_matches_string (data->cfg, inst, term_lc))
    {
      matches = 1;
    }

    free (term_lc);

    if (!matches)
      return (0);
  } /* if (data->search_term) */

  memset (descr, 0, sizeof (descr));
  inst_describe (data->cfg, inst, descr, sizeof (descr));
  html_escape_buffer (descr, sizeof (descr));

  memset (params, 0, sizeof (params));
  inst_get_params (data->cfg, inst, params, sizeof (params));
  html_escape_buffer (params, sizeof (params));

  printf ("  <li class=\"instance\"><a href=\"%s?action=show_instance;%s\">"
      "%s</a></li>\n",
      script_name (), params, descr);

  return (0);
} /* }}} int show_instance_cb */

static int show_graph (void *user_data) /* {{{ */
{
  show_graph_data_t *data = user_data;

  if ((data->search_term == NULL) || (data->search_term[0] == 0))
    printf ("<h2>All instances</h2>\n");
  else
  {
    char *search_term_html = html_escape (data->search_term);
    printf ("<h2>Instances matching &quot;%s&quot;</h2>\n",
        search_term_html);
    free (search_term_html);
  }

  printf ("<ul class=\"instance_list\">\n");
  graph_inst_foreach (data->cfg, show_instance_cb, data);
  printf ("</ul>\n");

  return (0);
} /* }}} int show_graph */

int action_show_graph (void) /* {{{ */
{
  page_callbacks_t pg_callbacks = PAGE_CALLBACKS_INIT;
  show_graph_data_t pg_data;

  char tmp[128];
  char title[128];

  memset (&pg_data, 0, sizeof (pg_data));
  pg_data.cfg = gl_graph_get_selected ();
  if (pg_data.cfg == NULL)
    OUTPUT_ERROR ("gl_graph_get_selected () failed.\n");

  pg_data.search_term = param ("q");

  memset (tmp, 0, sizeof (tmp));
  graph_get_title (pg_data.cfg, tmp, sizeof (tmp));
  snprintf (title, sizeof (title), "Graph \"%s\"", tmp);
  title[sizeof (title) - 1] = 0;

  pg_callbacks.top_right = html_print_search_box;
  pg_callbacks.middle_left = left_menu;
  pg_callbacks.middle_center = show_graph;
  pg_callbacks.middle_right = show_time_selector;

  html_print_page (title, &pg_callbacks, &pg_data);

  return (0);
} /* }}} int action_show_graph */

/* vim: set sw=2 sts=2 et fdm=marker : */
