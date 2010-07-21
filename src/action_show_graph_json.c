/**
 * collection4 - action_show_graph_json.c
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

#include "action_show_graph_json.h"
#include "common.h"
#include "graph.h"
#include "graph_ident.h"
#include "graph_instance.h"
#include "graph_list.h"
#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

static void write_callback (__attribute__((unused)) void *ctx, /* {{{ */
    const char *str, unsigned int len)
{
  fwrite ((void *) str, /* size = */ len, /* nmemb = */ 1, stdout);
} /* }}} void write_callback */

int action_show_graph_json (void) /* {{{ */
{
  graph_config_t *cfg;

  yajl_gen_config handler_config;
  yajl_gen handler;

  time_t now;
  char time_buffer[128];
  int status;

  cfg = gl_graph_get_selected ();
  if (cfg == NULL)
    return (ENOMEM);

  memset (&handler_config, 0, sizeof (handler_config));
  handler_config.beautify = 1;
  handler_config.indentString = "  ";

  handler = yajl_gen_alloc2 (write_callback,
      &handler_config,
      /* alloc functions = */ NULL,
      /* context = */ NULL);
  if (handler == NULL)
  {
    graph_destroy (cfg);
    return (-1);
  }

  printf ("Content-Type: application/json\n");

  now = time (NULL);
  status = time_to_rfc1123 (now + 300, time_buffer, sizeof (time_buffer));
  if (status == 0)
    printf ("Expires: %s\n"
        "Cache-Control: public\n",
        time_buffer);
  printf ("\n");

  status = graph_to_json (cfg, handler);

  graph_destroy (cfg);
  yajl_gen_free (handler);

  return (status);
} /* }}} int action_show_graph_json */

/* vim: set sw=2 sts=2 et fdm=marker : */
