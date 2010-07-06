/**
 * collection4 - aux_types.h
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

#ifndef AUX_TYPES_H
#define AUX_TYPES_H 1

struct statement_list_s
{
	oconfig_item_t *statement;
	int             statement_num;
};
typedef struct statement_list_s statement_list_t;

struct argument_list_s
{
	oconfig_value_t *argument;
	int              argument_num;
};
typedef struct argument_list_s argument_list_t;

#endif /* AUX_TYPES_H */
