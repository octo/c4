/**
 * collection4 - common.h
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

#ifndef COMMON_H
#define COMMON_H 1

#include <stdint.h>
#include <inttypes.h>

int print_debug (const char *format, ...)
  __attribute__((format(printf,1,2)));
#if 0
# define DEBUG(...) print_debug (__VA_ARGS__)
#else
# define DEBUG(...) /**/
#endif

size_t c_strlcat (char *dst, const char *src, size_t size);
#define strlcat c_strlcat

int ds_list_from_rrd_file (char *file,
    size_t *ret_dses_num, char ***ret_dses);

uint32_t get_random_color (void);
uint32_t fade_color (uint32_t color);

char *strtolower (char *str);
char *strtolower_copy (const char *str);

#endif /* COMMON_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
