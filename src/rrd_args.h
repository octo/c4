/**
 * collection4 - rrd_args.h
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

#ifndef RRD_ARGS_H
#define RRD_ARGS_H

#include "utils_array.h"

struct rrd_args_s
{
  str_array_t *options;
  str_array_t *data;
  str_array_t *calc;
  str_array_t *areas;
  str_array_t *lines;

  int index;
  char last_stack_cdef[64];
};
typedef struct rrd_args_s rrd_args_t;

rrd_args_t *ra_create (void);
void ra_destroy (rrd_args_t *ra);

int ra_argc (rrd_args_t *ra);
char **ra_argv (rrd_args_t *ra);
void ra_argv_free (char **argv);

#endif /* RRD_ARGS_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
