/**
 * collection4 - action_instance_data_json.c
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

#include "action_instance_data_json.h"
#include "common.h"
#include "graph.h"
#include "graph_instance.h"
#include "graph_list.h"
#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

/* Expire data after one day. */
#define EXPIRES_SECS 86400

static void write_callback (__attribute__((unused)) void *ctx, /* {{{ */
    const char *str, unsigned int len)
{
  fwrite ((void *) str, /* size = */ len, /* nmemb = */ 1, stdout);
} /* }}} void write_callback */

static int param_get_resolution (dp_time_t *resolution) /* {{{ */
{
  const char *tmp;
  char *endptr;
  double value;

  tmp = param ("resolution");
  if (tmp == NULL)
    return (ENOENT);

  errno = 0;
  endptr = NULL;
  value = strtod (tmp, &endptr);
  if (errno != 0)
    return (errno);
  else if ((value <= 0.0) || (endptr == tmp))
    return (EINVAL);

  resolution->tv_sec = (time_t) value;
  resolution->tv_nsec = (long) ((value - ((double) resolution->tv_sec)) * 1000000000.0);
  return (0);
} /* }}} int param_get_resolution */

int action_instance_data_json (void) /* {{{ */
{
  graph_config_t *cfg;
  graph_instance_t *inst;

  time_t tt_begin = 0;
  time_t tt_end = 0;
  time_t tt_now = 0;

  dp_time_t dp_begin = { 0, 0 };
  dp_time_t dp_end = { 0, 0 };
  dp_time_t dp_resolution = { 0, 0 };

  yajl_gen_config handler_config;
  yajl_gen handler;

  time_t expires;
  char time_buffer[128];
  int status;

  cfg = gl_graph_get_selected ();
  if (cfg == NULL)
    return (ENOMEM);

  inst = inst_get_selected (cfg);
  if (inst == NULL)
    return (EINVAL);

  /* Get selected time(s) */
  tt_begin = tt_end = tt_now = 0;
  status = get_time_args (&tt_begin, &tt_end, &tt_now);
  if (status != 0)
    return (status);

  dp_begin.tv_sec = tt_begin;
  dp_begin.tv_nsec = 0;
  dp_end.tv_sec = tt_end;
  dp_end.tv_nsec = 0;

  dp_resolution.tv_sec = (tt_end - tt_begin) / 324;
  param_get_resolution (&dp_resolution);

  memset (&handler_config, 0, sizeof (handler_config));
  handler_config.beautify = 0;
  handler_config.indentString = "  ";

  handler = yajl_gen_alloc2 (write_callback,
      &handler_config,
      /* alloc functions = */ NULL,
      /* context = */ NULL);
  if (handler == NULL)
    return (-1);

  printf ("Content-Type: application/json\n");

  /* By default, permit caching until 1/1000th after the last data. If that
   * data is in the past, assume the entire data is in the past and allow
   * caching for one day. */
  expires = tt_end + ((tt_end - tt_begin) / 1000);
  if (expires < tt_now)
    expires = tt_now + 86400;

  status = time_to_rfc1123 (expires, time_buffer, sizeof (time_buffer));
  if (status == 0)
    printf ("Expires: %s\n"
        "Cache-Control: public\n",
        time_buffer);
  printf ("\n");

  status = inst_data_to_json (inst,
      dp_begin, dp_end, dp_resolution, handler);

  yajl_gen_free (handler);

  return (status);
} /* }}} int action_instance_data_json */

/* vim: set sw=2 sts=2 et fdm=marker : */
