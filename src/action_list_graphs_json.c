/**
 * collection4 - action_list_graphs_json.c
 * Copyright (C) 2010,2011  Florian octo Forster
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

#include "action_list_graphs_json.h"
#include "common.h"
#include "graph.h"
#include "graph_list.h"
#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

static void write_callback (__attribute__((unused)) void *ctx, /* {{{ */
    const char *str, unsigned int len)
{
  fwrite ((void *) str, /* size = */ len, /* nmemb = */ 1, stdout);
} /* }}} void write_callback */

static int print_one_graph (graph_config_t *cfg, /* {{{ */
    void *user_data)
{
  char title[1024];
  size_t num_instances;
  graph_ident_t *selector;

  yajl_gen handler = user_data;

  num_instances = graph_num_instances (cfg);
  if (num_instances < 1)
    return (0);

  selector = graph_get_selector (cfg);
  if (selector == NULL)
  {
    /* TODO: Print error. */
    return (0);
  }

  yajl_gen_map_open (handler);

  memset (title, 0, sizeof (title));
  graph_get_title (cfg, title, sizeof (title));

  yajl_gen_string (handler,
      (unsigned char *) "title",
      (unsigned int) strlen ("title"));
  yajl_gen_string (handler,
      (unsigned char *) title,
      (unsigned int) strlen (title));

  yajl_gen_string (handler,
      (unsigned char *) "selector",
      (unsigned int) strlen ("selector"));
  ident_to_json (selector, handler);

  yajl_gen_string (handler,
      (unsigned char *) "num_instances",
      (unsigned int) strlen ("num_instances"));
  yajl_gen_integer (handler, (long int) num_instances);

  yajl_gen_map_close (handler);

  ident_destroy (selector);

  return (0);
} /* }}} int print_one_graph */

static int print_all_graphs (yajl_gen handler) /* {{{ */
{
  const char *dynamic;
  _Bool include_dynamic = 0;

  dynamic = param ("dynamic");
  if ((dynamic != NULL)
      && (strcmp ("true", dynamic) == 0))
    include_dynamic = 1;

  yajl_gen_array_open (handler);

  gl_graph_get_all (include_dynamic, print_one_graph,
      /* user_data = */ handler);

  yajl_gen_array_close (handler);

  return (0);
} /* }}} int print_all_graphs */

int action_list_graphs_json (void) /* {{{ */
{
  graph_config_t *cfg;

  yajl_gen_config handler_config;
  yajl_gen handler;

  time_t now;
  char time_buffer[128];
  int status;

  memset (&handler_config, 0, sizeof (handler_config));
  handler_config.beautify = 1;
  handler_config.indentString = "  ";

  handler = yajl_gen_alloc2 (write_callback,
      &handler_config,
      /* alloc functions = */ NULL,
      /* context = */ NULL);
  if (handler == NULL)
    return (-1);

  printf ("Content-Type: application/json\n");

  now = time (NULL);
  status = time_to_rfc1123 (now + 300, time_buffer, sizeof (time_buffer));
  if (status == 0)
    printf ("Expires: %s\n"
        "Cache-Control: public\n",
        time_buffer);
  printf ("\n");

  print_all_graphs (handler);

  yajl_gen_free (handler);

  return (status);
} /* }}} int action_list_graphs_json */

/* vim: set sw=2 sts=2 et fdm=marker : */
