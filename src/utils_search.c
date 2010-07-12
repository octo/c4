/**
 * collection4 - utils_search.c
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
#include <ctype.h>
#include <errno.h>

#include "utils_search.h"
#include "graph_instance.h"
#include "utils_array.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

struct search_info_s
{
  char *host;
  char *plugin;
  char *plugin_instance;
  char *type;
  char *type_instance;

  str_array_t *terms;
};

/*
 * Private functions
 */
static char *read_quoted_string (const char **buffer) /* {{{ */
{
  const char *ptr = *buffer;
  char *ret;
  size_t ret_len;

  if (ptr[0] != '"')
    return (NULL);
  ptr++;

  ret_len = 0;
  while ((*ptr != '"') && (*ptr != 0))
  {
    ret_len++;

    if (*ptr == '\\')
      ptr += 2;
    else
      ptr++;
  }

  if ((ret_len < 1) || (*ptr != '"'))
    return (NULL);

  ret = malloc (ret_len + 1);
  if (ret == NULL)
    return (NULL);

  ptr = *buffer + 1;
  ret_len = 0;
  while ((*ptr != '"') && (*ptr != 0))
  {
    if (*ptr == '"')
      break;

    if (*ptr == '\\')
      ptr++;

    ret[ret_len] = *ptr;

    ptr++;
    ret_len++;
  }

  /* terminate string */
  ret[ret_len] = 0;

  /* "ptr" points to the '"' sign, so advance one more */
  ptr++;
  *buffer = ptr;

  return (ret);
} /* }}} char *read_quoted_string */

static char *read_unquoted_word (const char **buffer) /* {{{ */
{
  const char *ptr = *buffer;
  char *ret;
  size_t ret_len;

  ret_len = 0;
  while (!isspace ((int) ptr[ret_len]) && (ptr[ret_len] != 0))
    ret_len++;

  if (ret_len < 1)
    return (NULL);

  ret = malloc (ret_len + 1);
  if (ret == NULL)
    return (NULL);

  memcpy (ret, ptr, ret_len);
  ret[ret_len] = 0;

  ptr += ret_len;
  *buffer = ptr;

  return (ret);
} /* }}} char *read_unquoted_word */

static char *next_token (const char **buffer) /* {{{ */
{
  const char *ptr = *buffer;
  char *ret;

  while (isspace ((int) (*ptr)))
    ptr++;

  if (ptr[0] == 0)
    return (NULL);
  else if (ptr[0] == '"')
  {
    ret = read_quoted_string (&ptr);
    if (ret != NULL)
    {
      *buffer = ptr;
      return (ret);
    }
  }

  ret = read_unquoted_word (&ptr);
  if (ret != NULL)
    *buffer = ptr;

  return (ret);
} /* }}} char *next_token */

static int store_token_field (char **field, const char *token) /* {{{ */
{
  char *copy;

  if ((field == NULL) || (token == NULL))
    return (EINVAL);

  copy = strdup (token);
  if (copy == NULL)
    return (ENOMEM);

  free (*field);
  *field = copy;

  return (0);
} /* }}} int store_token_field */

static int store_token (search_info_t *si, const char *token) /* {{{ */
{
  if (strncmp ("host:", token, strlen ("host:")) == 0)
    return (store_token_field (&si->host, token + strlen ("host:")));
  else if (strncmp ("plugin:", token, strlen ("plugin:")) == 0)
    return (store_token_field (&si->plugin, token + strlen ("plugin:")));
  else if (strncmp ("plugin_instance:", token, strlen ("plugin_instance:")) == 0)
    return (store_token_field (&si->plugin_instance, token + strlen ("plugin_instance:")));
  else if (strncmp ("type:", token, strlen ("type:")) == 0)
    return (store_token_field (&si->type, token + strlen ("type:")));
  else if (strncmp ("type_instance:", token, strlen ("type_instance:")) == 0)
    return (store_token_field (&si->type_instance, token + strlen ("type_instance:")));

  return (array_append (si->terms, token));
} /* }}} int store_token */

/*
 * Public functions
 */
search_info_t *search_parse (const char *search) /* {{{ */
{
  const char *ptr;
  char *token;
  search_info_t *si;

  si = malloc (sizeof (*si));
  if (si == NULL)
    return (NULL);
  memset (si, 0, sizeof (*si));

  si->terms = array_create ();
  if (si->terms == NULL)
  {
    free (si);
    return (NULL);
  }

  ptr = search;

  while ((token = next_token (&ptr)) != NULL)
  {
    store_token (si, token);
    free (token);
  }

  return (si);
} /* }}} search_info_t *search_parse */

void search_destroy (search_info_t *si) /* {{{ */
{
  if (si == NULL)
    return;

  free (si->host);
  free (si->plugin);
  free (si->plugin_instance);
  free (si->type);
  free (si->type_instance);

  array_destroy (si->terms);
} /* }}} void search_destroy */

graph_ident_t *search_to_ident (search_info_t *si) /* {{{ */
{
  if (si == NULL)
    return (NULL);

  return (ident_create ((si->host == NULL) ? ANY_TOKEN : si->host,
        (si->plugin == NULL) ? ANY_TOKEN : si->plugin,
        (si->plugin_instance == NULL) ? ANY_TOKEN : si->plugin_instance,
        (si->type == NULL) ? ANY_TOKEN : si->type,
        (si->type_instance == NULL) ? ANY_TOKEN : si->type_instance));
} /* }}} graph_ident_t *search_to_ident */

_Bool search_graph_title_matches (search_info_t *si, /* {{{ */
    const char *title)
{
  char **argv;
  int argc;
  int i;

  if ((si == NULL) || (title == NULL))
    return (0);

  if (si->terms == NULL)
    return (1);

  argc = array_argc (si->terms);
  argv = array_argv (si->terms);
  for (i = 0; i < argc; i++)
    if (strstr (title, argv[i]) == NULL)
      return (0);

  return (1);
} /* }}} _Bool search_graph_title_matches */

_Bool search_graph_inst_matches (search_info_t *si, /* {{{ */
    graph_config_t *cfg, graph_instance_t *inst,
    const char *title)
{
  char **argv;
  int argc;
  int i;

  if ((si == NULL) || (cfg == NULL) || (inst == NULL))
    return (0);

  if ((si->host != NULL)
      && !inst_matches_field (inst, GIF_HOST, si->host))
    return (0);
  else if ((si->plugin != NULL)
      && !inst_matches_field (inst, GIF_PLUGIN, si->plugin))
    return (0);
  else if ((si->plugin_instance != NULL)
      && !inst_matches_field (inst, GIF_PLUGIN_INSTANCE, si->plugin_instance))
    return (0);
  else if ((si->type != NULL)
      && !inst_matches_field (inst, GIF_TYPE, si->type))
    return (0);
  else if ((si->type_instance != NULL)
      && !inst_matches_field (inst, GIF_TYPE_INSTANCE, si->type_instance))
    return (0);

  if (si->terms == NULL)
    return (1);

  argc = array_argc (si->terms);
  argv = array_argv (si->terms);
  for (i = 0; i < argc; i++)
  {
    if (inst_matches_string (cfg, inst, argv[i]))
      continue;

    if ((title != NULL) && (strstr (title, argv[i]) != NULL))
      continue;

    return (0);
  }

  return (1);
} /* }}} _Bool search_graph_inst_matches */

/* vim: set sw=2 sts=2 et fdm=marker : */
