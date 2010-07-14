/**
 * collection4 - action_search.c
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "action_search.h"
#include "common.h"
#include "graph.h"
#include "graph_ident.h"
#include "graph_instance.h"
#include "graph_list.h"
#include "utils_cgi.h"
#include "utils_search.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

#define RESULT_LIMIT 50

struct callback_data_s
{
  graph_config_t *cfg;
  int graph_index;
  int graph_limit;
  _Bool graph_more;
  int inst_index;
  int inst_limit;
  _Bool inst_more;
  const char *search_term;
};
typedef struct callback_data_s callback_data_t;

static int left_menu (__attribute__((unused)) void *user_data) /* {{{ */
{
  printf ("\n<ul class=\"menu left\">\n"
      "  <li><a href=\"%s?action=list_graphs\">All graphs</a></li>\n"
      "  <li><a href=\"%s?action=list_hosts\">All hosts</a></li>\n"
      "</ul>\n",
      script_name (), script_name ());

  return (0);
} /* }}} int left_menu */

static int print_graph_inst_html (graph_config_t *cfg, /* {{{ */
    graph_instance_t *inst,
    void *user_data)
{
  callback_data_t *data = user_data;
  char params[1024];
  char desc[1024];

  if (data->cfg != cfg)
  {
    data->graph_index++;
    if (data->graph_index >= data->graph_limit)
    {
      data->graph_more = 1;
      return (1);
    }

    if (data->cfg != NULL)
      printf ("  </ul></li>\n");

    memset (desc, 0, sizeof (desc));
    graph_get_title (cfg, desc, sizeof (desc));
    html_escape_buffer (desc, sizeof (desc));

    printf ("  <li class=\"graph\">%s\n"
        "  <ul class=\"instance_list\">\n", desc);

    data->cfg = cfg;
    data->inst_index = -1;
    data->inst_more = 0;
  }

  data->inst_index++;
  if (data->inst_index >= data->inst_limit)
  {
    if (!data->inst_more)
    {
      char *search_term_html = html_escape (data->search_term);
      char param_search_term[1024];

      memset (params, 0, sizeof (params));
      graph_get_params (cfg, params, sizeof (params));
      html_escape_buffer (params, sizeof (params));

      param_search_term[0] = 0;
      if (search_term_html != NULL)
      {
        snprintf (param_search_term, sizeof (param_search_term), ";q=%s",
            search_term_html);
        param_search_term[sizeof (param_search_term) - 1] = 0;
      }

      free (search_term_html);

      printf ("    <li class=\"instance more\"><a href=\"%s"
          "?action=show_graph;%s%s\">More &#x2026;</a></li>\n",
          script_name (), params, param_search_term);

      data->inst_more = 1;
    }
    return (0);
  }

  memset (params, 0, sizeof (params));
  inst_get_params (cfg, inst, params, sizeof (params));
  html_escape_buffer (params, sizeof (params));

  memset (desc, 0, sizeof (desc));
  inst_describe (cfg, inst, desc, sizeof (desc));
  html_escape_buffer (desc, sizeof (desc));

  printf ("    <li class=\"instance\"><a href=\"%s?action=show_instance;%s\">%s</a></li>\n",
      script_name (), params, desc);

  return (0);
} /* }}} int print_graph_inst_html */

struct page_data_s
{
  char *search_term;
  search_info_t *search_info;
};
typedef struct page_data_s page_data_t;

static int print_search_result (void *user_data) /* {{{ */
{
  page_data_t *pg_data = user_data;
  callback_data_t cb_data = { /* cfg = */ NULL,
    /* graph_index = */ -1, /* graph_limit = */ 20, /* graph_more = */ 0,
    /* inst_index = */  -1, /* inst_limit = */   5, /* inst more = */  0,
    /* search_term = */ pg_data->search_term };
  char *search_term_html;

  assert (pg_data->search_term != NULL);

  search_term_html = html_escape (pg_data->search_term);
  printf ("    <h2>Search results for &quot;%s&quot;</h2>\n",
      search_term_html);
  free (search_term_html);

  printf ("    <ul id=\"search-output\" class=\"graph_list\">\n");

  gl_search (pg_data->search_info, print_graph_inst_html,
      /* user_data = */ &cb_data);


  if (cb_data.cfg != NULL)
    printf ("      </ul></li>\n");

  if (cb_data.graph_more)
  {
    printf ("    <li class=\"graph more\">More ...</li>\n");
  }

  printf ("    </ul>\n");

  return (0);
} /* }}} int print_search_result */

static int print_search_form (void *user_data) /* {{{ */
{
  page_data_t *pg_data = user_data;
  char search_term_html[1024];

  if (pg_data->search_term != NULL)
  {
    html_escape_copy (search_term_html, pg_data->search_term,
        sizeof (search_term_html));
  }
  else
  {
    search_term_html[0] = 0;
  }

  printf ("<form action=\"%s\" method=\"get\">\n"
      "  <input type=\"hidden\" name=\"action\" value=\"search\" />\n"
      "  <fieldset>\n"
      "    <legend>Advanced search</legend>\n"
#if 0
      "      <span class=\"nbr\">\n"
      "        <input type=\"checkbox\" name=\"sfh\" value=\"1\" checked=\"checked\" id=\"search_for_host\" />\n"
      "        <label for=\"search_for_host\">Host</label>\n"
      "      </span>\n"
      "      <span class=\"nbr\">\n"
      "        <input type=\"checkbox\" name=\"sfp\" value=\"1\" checked=\"checked\" id=\"search_for_plugin\" />\n"
      "        <label for=\"search_for_plugin\">Plugin</label>\n"
      "      </span>\n"
      "      <span class=\"nbr\">\n"
      "        <input type=\"checkbox\" name=\"sfpi\" value=\"1\" checked=\"checked\" id=\"search_for_plugin_instance\" />\n"
      "        <label for=\"search_for_plugin_instance\">Plugin instance</label>\n"
      "      </span>\n"
      "      <span class=\"nbr\">\n"
      "        <input type=\"checkbox\" name=\"sft\" value=\"1\" checked=\"checked\" id=\"search_for_type\" />\n"
      "        <label for=\"search_for_type\">Type</label>\n"
      "      </span>\n"
      "      <span class=\"nbr\">\n"
      "        <input type=\"checkbox\" name=\"sfti\" value=\"1\" checked=\"checked\" id=\"search_for_type_instance\" />\n"
      "        <label for=\"search_for_type_instance\">Type instance</label>\n"
      "      </span>\n"
#endif
      "    <div>Search for <input type=\"text\" name=\"q\" value=\"%s\" size=\"50\" /></div>\n"
      "    <input type=\"submit\" name=\"button\" value=\"Advanced search\" />"
      "  </fieldset>\n"
      "</form>\n",
      script_name (), search_term_html);

  return (0);
} /* }}} int print_search_form */

static int search_html (page_data_t *pg_data) /* {{{ */
{
  page_callbacks_t pg_callbacks = PAGE_CALLBACKS_INIT;
  char title[512];

  if (pg_data->search_term != NULL)
    snprintf (title, sizeof (title), "Graphs matching \"%s\"",
        pg_data->search_term);
  else
    strncpy (title, "Search", sizeof (title));
  title[sizeof (title) - 1] = 0;

  pg_callbacks.top_right = html_print_search_box;
  pg_callbacks.middle_left = left_menu;
  if (pg_data->search_term != NULL)
    pg_callbacks.middle_center = print_search_result;
  else
    pg_callbacks.middle_center = print_search_form;

  html_print_page (title, &pg_callbacks, pg_data);

  return (0);
} /* }}} int search_html */

int action_search (void) /* {{{ */
{
  page_data_t pg_data;
  int status;

  gl_update ();

  pg_data.search_term = strtolower_copy (param ("q"));
  if ((pg_data.search_term != NULL) && (pg_data.search_term[0] == 0))
  {
    free (pg_data.search_term);
    pg_data.search_term = NULL;
    pg_data.search_info = NULL;
  }
  else
  {
    pg_data.search_info = search_parse (pg_data.search_term);
  }

  status = search_html (&pg_data);

  free (pg_data.search_term);
  search_destroy (pg_data.search_info);

  return (status);
} /* }}} int action_search */

/* vim: set sw=2 sts=2 et fdm=marker : */
