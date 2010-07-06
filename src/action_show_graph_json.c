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

struct show_graph_data_s
{
  graph_config_t *cfg;
  _Bool first;
};
typedef struct show_graph_data_s show_graph_data_t;

static int show_instance_cb (graph_instance_t *inst, /* {{{ */
    void *user_data)
{
  show_graph_data_t *data = user_data;
  graph_ident_t *ident;
  char *ident_json;

  ident = inst_get_selector (inst);
  if (ident == NULL)
    return (ENOMEM);

  ident_json = ident_to_json (ident);
  if (ident_json == NULL)
  {
    ident_destroy (ident);
    return (ENOMEM);
  }

  if (!data->first)
    printf (",\n");
  data->first = 0;

  printf ("    %s", ident_json);

  free (ident_json);
  ident_destroy (ident);
  return (0);
} /* }}} int show_instance_cb */

int action_show_graph_json (void) /* {{{ */
{
  show_graph_data_t data;

  time_t now;
  char time_buffer[128];
  int status;

  char title[1024];
  graph_ident_t *ident;
  char *ident_json;

  memset (&data, 0, sizeof (data));
  data.first = 1;
  data.cfg = gl_graph_get_selected ();
  if (data.cfg == NULL)
    return (ENOMEM);

  ident = graph_get_selector (data.cfg);
  if (ident == NULL)
    return (ENOMEM);

  ident_json = ident_to_json (ident);
  if (ident_json == NULL)
  {
    ident_destroy (ident);
    return (ENOMEM);
  }

  printf ("Content-Type: text/plain\n");

  now = time (NULL);
  status = time_to_rfc1123 (now + 300, time_buffer, sizeof (time_buffer));
  if (status == 0)
    printf ("Expires: %s\n"
        "Cache-Control: public\n",
        time_buffer);
  printf ("\n");

  memset (title, 0, sizeof (title));
  graph_get_title (data.cfg, title, sizeof (title));
  json_escape_buffer (title, sizeof (title));

  printf ("{\n");
  printf ("  \"title\": \"%s\",\n", title);
  printf ("  \"selector\": %s,\n", ident_json);
  printf ("  \"instances\":\n");
  printf ("  [\n");
  graph_inst_foreach (data.cfg, show_instance_cb, &data);
  printf ("\n  ]\n}\n");

  free (ident_json);
  ident_destroy (ident);
  return (0);
} /* }}} int action_show_graph_json */

/* vim: set sw=2 sts=2 et fdm=marker : */
