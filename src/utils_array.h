/**
 * collection4 - utils_array.h
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

#ifndef UTILS_ARRAY_H
#define UTILS_ARRAY_H 1

struct str_array_s;
typedef struct str_array_s str_array_t;

str_array_t *array_create (void);
void array_destroy (str_array_t *a);

/* Appends a string to the array. The string is duplicated, so the original
 * string may / must be freed. */
int array_append (str_array_t *a, const char *entry);
int array_append_format (str_array_t *a, const char *format, ...)
  __attribute__((format(printf,2,3)));

/* Prepends a string to the array. The string is duplicated, so the original
 * string may / must be freed. */
int array_prepend (str_array_t *a, const char *entry);
int array_prepend_format (str_array_t *a, const char *format, ...)
  __attribute__((format(printf,2,3)));

int array_sort (str_array_t *a);

int array_argc (str_array_t *);
char **array_argv (str_array_t *);

#endif /* UTILS_ARRAY_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
