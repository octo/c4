/**
 * collection4 - action_list_graphs.c
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

#include "action_list_graphs.h"
#include "common.h"
#include "graph.h"
#include "graph_list.h"
#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

static int left_menu (__attribute__((unused)) void *user_data) /* {{{ */
{
  printf ("\n<ul class=\"menu left\">\n"
      "  <li><a href=\"%s?action=search\">Search</a></li>\n"
      "  <li><a href=\"%s?action=list_hosts\">All hosts</a></li>\n"
      "</ul>\n",
      script_name (), script_name ());

  return (0);
} /* }}} int left_menu */

static int print_one_graph (graph_config_t *cfg, /* {{{ */
    __attribute__((unused)) void *user_data)
{
  char params[1024];
  char title[1024];
  size_t num_instances;

  num_instances = graph_num_instances (cfg);
  if (num_instances < 1)
    return (0);

  memset (title, 0, sizeof (title));
  graph_get_title (cfg, title, sizeof (title));
  html_escape_buffer (title, sizeof (title));

  memset (params, 0, sizeof (params));
  graph_get_params (cfg, params, sizeof (params));
  html_escape_buffer (params, sizeof (params));

  printf ("      <li class=\"graph\"><a href=\"%s?action=show_graph;%s\">"
      "%s</a> <span class=\"num_instances\">(%lu %s)</span></li>\n",
      script_name (), params, title,
      (unsigned long) num_instances,
      (num_instances == 1) ? "instance" : "instances");

  return (0);
} /* }}} int print_one_graph */

static int print_all_graphs (__attribute__((unused)) void *user_data) /* {{{ */
{
  printf ("    <ul class=\"graph_list\">\n");
  gl_graph_get_all (print_one_graph, /* user_data = */ NULL);
  printf ("    </ul>\n");

  return (0);
} /* }}} int print_all_graphs */

int action_list_graphs (void) /* {{{ */
{
  page_callbacks_t pg_callbacks = PAGE_CALLBACKS_INIT;
  char title[512];

  strncpy (title, "List of all graphs", sizeof (title));
  title[sizeof (title) - 1] = 0;

  pg_callbacks.top_right = html_print_search_box;
  pg_callbacks.middle_left = left_menu;
  pg_callbacks.middle_center = print_all_graphs;

  html_print_page (title, &pg_callbacks, /* user data = */ NULL);

  return (0);
} /* }}} int action_list_graphs */

/* vim: set sw=2 sts=2 et fdm=marker : */
