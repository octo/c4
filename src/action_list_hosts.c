/**
 * collection4 - action_list_hosts.c
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

#include "action_list_hosts.h"
#include "common.h"
#include "graph.h"
#include "graph_list.h"
#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

static int print_one_host (const char *host, /* {{{ */
    __attribute__((unused)) void *user_data)
{
  char host_html[128];

  strncpy (host_html, host, sizeof (host_html));
  host_html[sizeof (host_html) - 1] = 0;
  html_escape_buffer (host_html, sizeof (host_html));

  printf ("      <li class=\"host\"><a href=\"%s?action=search;q=host:%s\">"
      "%s</a></li>\n",
      script_name (), host_html, host_html);

  return (0);
} /* }}} int print_one_host */

static int print_all_hosts (__attribute__((unused)) void *user_data) /* {{{ */
{
  printf ("    <ul class=\"host_list\">\n");
  gl_foreach_host (print_one_host, /* user_data = */ NULL);
  printf ("    </ul>\n");

  return (0);
} /* }}} int print_all_hosts */

int action_list_hosts (void) /* {{{ */
{
  gl_update ();

  page_callbacks_t pg_callbacks = PAGE_CALLBACKS_INIT;
  char title[512];

  strncpy (title, "List of all hosts", sizeof (title));
  title[sizeof (title) - 1] = 0;

  pg_callbacks.top_right = html_print_search_box;
  pg_callbacks.middle_center = print_all_hosts;

  html_print_page (title, &pg_callbacks, /* user data = */ NULL);

  return (0);
} /* }}} int action_list_hosts */

/* vim: set sw=2 sts=2 et fdm=marker : */
