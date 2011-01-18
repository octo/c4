/**
 * collection4 - main.c
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
#include <strings.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "common.h"
#include "graph_list.h"
#include "utils_cgi.h"

#include "action_graph.h"
#include "action_instance_data_json.h"
#include "action_graph_def_json.h"
#include "action_list_graphs.h"
#include "action_list_hosts.h"
#include "action_search.h"
#include "action_search_json.h"
#include "action_show_graph.h"
#include "action_show_graph_json.h"
#include "action_show_instance.h"

/* Include this last, so the macro magic of <fcgi_stdio.h> doesn't interfere
 * with our own header files. */
#include <fcgiapp.h>
#include <fcgi_stdio.h>

struct action_s
{
  const char *name;
  int (*callback) (void);
};
typedef struct action_s action_t;

static int action_usage (void);

static const action_t actions[] =
{
  { "graph",       action_graph },
  { "instance_data_json", action_instance_data_json },
  { "graph_def_json", action_graph_def_json },
  { "list_graphs", action_list_graphs },
  { "list_hosts",  action_list_hosts },
  { "search",      action_search },
  { "search_json", action_search_json },
  { "show_graph",  action_show_graph },
  { "show_graph_json",  action_show_graph_json },
  { "show_instance", action_show_instance },
  { "usage",       action_usage }
};
static const size_t actions_num = sizeof (actions) / sizeof (actions[0]);


static int action_usage (void) /* {{{ */
{
  size_t i;

  printf ("Content-Type: text/plain\n\n");

  printf ("Usage:\n"
      "\n"
      "  Available actions:\n"
      "\n");

  for (i = 0; i < actions_num; i++)
    printf ("  * %s\n", actions[i].name);

  printf ("\n");

  return (0);
} /* }}} int action_usage */

static int handle_request (void) /* {{{ */
{
  const char *action;

  param_init ();

  action = param ("action");
  if (action == NULL)
  {
    return (action_list_graphs ());
  }
  else
  {
    size_t i;
    int status = ENOENT;

    gl_update (/* request_served = */ 0);

    for (i = 0; i < actions_num; i++)
    {
      if (strcmp (action, actions[i].name) == 0)
      {
        status = (*actions[i].callback) ();
        break;
      }
    }

    if (i >= actions_num)
      status = action_usage ();

    /* Call finish before updating the graph list, so clients don't wait for
     * the update to finish. */
    FCGI_Finish ();

    gl_update (/* request_served = */ 1);

    return (status);
  }
} /* }}} int handle_request */

static int run (void) /* {{{ */
{
  while (FCGI_Accept() >= 0)
  {
    handle_request ();
    param_finish ();
  }

  return (0);
} /* }}} int run */

int main (int argc, char **argv) /* {{{ */
{
  int status;

  argc = 0;
  argv = NULL;

  if (FCGX_IsCGI ())
    status = handle_request ();
  else
    status = run ();

  exit ((status == 0) ? EXIT_SUCCESS : EXIT_FAILURE);
} /* }}} int main */

/* vim: set sw=2 sts=2 et fdm=marker : */
