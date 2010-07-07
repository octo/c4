/**
 * collection4 - graph_ident.c
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
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <limits.h> /* PATH_MAX */
#include <sys/types.h>
#include <sys/stat.h>

#include "graph_ident.h"
#include "common.h"
#include "filesystem.h"
#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

/*
 * Data types
 */
struct graph_ident_s /* {{{ */
{
  char *host;
  char *plugin;
  char *plugin_instance;
  char *type;
  char *type_instance;
}; /* }}} struct graph_ident_s */

/*
 * Private functions
 */
static char *part_copy_with_selector (const char *selector, /* {{{ */
    const char *part, unsigned int flags)
{
  if ((selector == NULL) || (part == NULL))
    return (NULL);

  if ((flags & IDENT_FLAG_REPLACE_ANY) && IS_ANY (part))
    return (NULL);

  if ((flags & IDENT_FLAG_REPLACE_ALL) && IS_ALL (part))
    return (NULL);

  /* Replace the ANY and ALL flags if requested and if the selecter actually
   * *is* that flag. */
  if (IS_ANY (selector))
  {
    if (flags & IDENT_FLAG_REPLACE_ANY)
      return (strdup (part));
    else
      return (strdup (selector));
  }

  if (IS_ALL (selector))
  {
    if (flags & IDENT_FLAG_REPLACE_ALL)
      return (strdup (part));
    else
      return (strdup (selector));
  }

  if (strcmp (selector, part) != 0)
    return (NULL);

  /* Otherwise (no replacement), return a copy of the selector. */
  return (strdup (selector));
} /* }}} char *part_copy_with_selector */

static _Bool part_matches (const char *selector, /* {{{ */
    const char *part)
{
#if C4_DEBUG
  if ((selector == NULL) && (part == NULL))
    return (1);
#endif

  if (selector == NULL) /* && (part != NULL) */
    return (0);

  if (IS_ANY(selector) || IS_ALL(selector))
    return (1);

  if (part == NULL) /* && (selector != NULL) */
    return (0);

  if (strcmp (selector, part) == 0)
    return (1);

  return (0);
} /* }}} _Bool part_matches */

/*
 * Public functions
 */
graph_ident_t *ident_create (const char *host, /* {{{ */
    const char *plugin, const char *plugin_instance,
    const char *type, const char *type_instance)
{
  graph_ident_t *ret;

  if ((host == NULL)
      || (plugin == NULL) || (plugin_instance == NULL)
      || (type == NULL) || (type_instance == NULL))
    return (NULL);

  ret = malloc (sizeof (*ret));
  if (ret == NULL)
    return (NULL);
  memset (ret, 0, sizeof (*ret));

  ret->host = NULL;
  ret->host = NULL;
  ret->plugin = NULL;
  ret->plugin_instance = NULL;
  ret->type = NULL;
  ret->type_instance = NULL;

#define COPY_PART(p) do {        \
  ret->p = strdup (p);           \
  if (ret->p == NULL)            \
  {                              \
    free (ret->host);            \
    free (ret->plugin);          \
    free (ret->plugin_instance); \
    free (ret->type);            \
    free (ret->type_instance);   \
    free (ret);                  \
    return (NULL);               \
  }                              \
} while (0)

  COPY_PART(host);
  COPY_PART(plugin);
  COPY_PART(plugin_instance);
  COPY_PART(type);
  COPY_PART(type_instance);

#undef COPY_PART

  return (ret);
} /* }}} graph_ident_t *ident_create */

graph_ident_t *ident_clone (const graph_ident_t *ident) /* {{{ */
{
  return (ident_create (ident->host,
        ident->plugin, ident->plugin_instance,
        ident->type, ident->type_instance));
} /* }}} graph_ident_t *ident_clone */

graph_ident_t *ident_copy_with_selector (const graph_ident_t *selector, /* {{{ */
    const graph_ident_t *ident, unsigned int flags)
{
  graph_ident_t *ret;

  if ((selector == NULL) || (ident == NULL))
    return (NULL);

  ret = malloc (sizeof (*ret));
  if (ret == NULL)
    return (NULL);
  memset (ret, 0, sizeof (*ret));
  ret->host = NULL;
  ret->plugin = NULL;
  ret->plugin_instance = NULL;
  ret->type = NULL;
  ret->type_instance = NULL;

#define COPY_PART(p) do {                                  \
  ret->p = part_copy_with_selector (selector->p, ident->p, flags); \
  if (ret->p == NULL)                                      \
  {                                                        \
    free (ret->host);                                      \
    free (ret->plugin);                                    \
    free (ret->plugin_instance);                           \
    free (ret->type);                                      \
    free (ret->type_instance);                             \
    return (NULL);                                         \
  }                                                        \
} while (0)

  COPY_PART (host);
  COPY_PART (plugin);
  COPY_PART (plugin_instance);
  COPY_PART (type);
  COPY_PART (type_instance);

#undef COPY_PART

  return (ret);
} /* }}} graph_ident_t *ident_copy_with_selector */

void ident_destroy (graph_ident_t *ident) /* {{{ */
{
  if (ident == NULL)
    return;

  free (ident->host);
  free (ident->plugin);
  free (ident->plugin_instance);
  free (ident->type);
  free (ident->type_instance);

  free (ident);
} /* }}} void ident_destroy */

/* ident_get_* methods {{{ */
const char *ident_get_host (const graph_ident_t *ident) /* {{{ */
{
  if (ident == NULL)
    return (NULL);

  return (ident->host);
} /* }}} char *ident_get_host */

const char *ident_get_plugin (const graph_ident_t *ident) /* {{{ */
{
  if (ident == NULL)
    return (NULL);

  return (ident->plugin);
} /* }}} char *ident_get_plugin */

const char *ident_get_plugin_instance (const graph_ident_t *ident) /* {{{ */
{
  if (ident == NULL)
    return (NULL);

  return (ident->plugin_instance);
} /* }}} char *ident_get_plugin_instance */

const char *ident_get_type (const graph_ident_t *ident) /* {{{ */
{
  if (ident == NULL)
    return (NULL);

  return (ident->type);
} /* }}} char *ident_get_type */

const char *ident_get_type_instance (const graph_ident_t *ident) /* {{{ */
{
  if (ident == NULL)
    return (NULL);

  return (ident->type_instance);
} /* }}} char *ident_get_type_instance */

const char *ident_get_field (const graph_ident_t *ident, /* {{{ */
    graph_ident_field_t field)
{
  if ((ident == NULL) || (field >= _GIF_LAST))
    return (NULL);

  if (field == GIF_HOST)
    return (ident->host);
  else if (field == GIF_PLUGIN)
    return (ident->plugin);
  else if (field == GIF_PLUGIN_INSTANCE)
    return (ident->plugin_instance);
  else if (field == GIF_TYPE)
    return (ident->type);
  else if (field == GIF_TYPE_INSTANCE)
    return (ident->type_instance);
  else
    return (NULL); /* never reached */
} /* }}} const char *ident_get_field */
/* }}} ident_get_* methods */

/* ident_set_* methods {{{ */
int ident_set_host (graph_ident_t *ident, const char *host) /* {{{ */
{
  char *tmp;

  if ((ident == NULL) || (host == NULL))
    return (EINVAL);

  tmp = strdup (host);
  if (tmp == NULL)
    return (ENOMEM);

  free (ident->host);
  ident->host = tmp;

  return (0);
} /* }}} int ident_set_host */

int ident_set_plugin (graph_ident_t *ident, const char *plugin) /* {{{ */
{
  char *tmp;

  if ((ident == NULL) || (plugin == NULL))
    return (EINVAL);

  tmp = strdup (plugin);
  if (tmp == NULL)
    return (ENOMEM);

  free (ident->plugin);
  ident->plugin = tmp;

  return (0);
} /* }}} int ident_set_plugin */

int ident_set_plugin_instance (graph_ident_t *ident, const char *plugin_instance) /* {{{ */
{
  char *tmp;

  if ((ident == NULL) || (plugin_instance == NULL))
    return (EINVAL);

  tmp = strdup (plugin_instance);
  if (tmp == NULL)
    return (ENOMEM);

  free (ident->plugin_instance);
  ident->plugin_instance = tmp;

  return (0);
} /* }}} int ident_set_plugin_instance */

int ident_set_type (graph_ident_t *ident, const char *type) /* {{{ */
{
  char *tmp;

  if ((ident == NULL) || (type == NULL))
    return (EINVAL);

  tmp = strdup (type);
  if (tmp == NULL)
    return (ENOMEM);

  free (ident->type);
  ident->type = tmp;

  return (0);
} /* }}} int ident_set_type */

int ident_set_type_instance (graph_ident_t *ident, const char *type_instance) /* {{{ */
{
  char *tmp;

  if ((ident == NULL) || (type_instance == NULL))
    return (EINVAL);

  tmp = strdup (type_instance);
  if (tmp == NULL)
    return (ENOMEM);

  free (ident->type_instance);
  ident->type_instance = tmp;

  return (0);
} /* }}} int ident_set_type_instance */

/* }}} ident_set_* methods */

int ident_compare (const graph_ident_t *i0, /* {{{ */
    const graph_ident_t *i1)
{
  int status;

#define COMPARE_PART(p) do {       \
  status = strcmp (i0->p, i1->p);  \
  if (status != 0)                 \
    return (status);               \
} while (0)

  COMPARE_PART (host);
  COMPARE_PART (plugin);
  COMPARE_PART (plugin_instance);
  COMPARE_PART (type);
  COMPARE_PART (type_instance);

#undef COMPARE_PART

  return (0);
} /* }}} int ident_compare */

_Bool ident_matches (const graph_ident_t *selector, /* {{{ */
    const graph_ident_t *ident)
{
#if C4_DEBUG
  if ((selector == NULL) || (ident == NULL))
    return (0);
#endif

  if (!part_matches (selector->host, ident->host))
    return (0);

  if (!part_matches (selector->plugin, ident->plugin))
    return (0);

  if (!part_matches (selector->plugin_instance, ident->plugin_instance))
    return (0);

  if (!part_matches (selector->type, ident->type))
    return (0);

  if (!part_matches (selector->type_instance, ident->type_instance))
    return (0);

  return (1);
} /* }}} _Bool ident_matches */

char *ident_to_string (const graph_ident_t *ident) /* {{{ */
{
  char buffer[PATH_MAX];

  buffer[0] = 0;

  strlcat (buffer, ident->host, sizeof (buffer));
  strlcat (buffer, "/", sizeof (buffer));
  strlcat (buffer, ident->plugin, sizeof (buffer));
  if (ident->plugin_instance[0] != 0)
  {
    strlcat (buffer, "-", sizeof (buffer));
    strlcat (buffer, ident->plugin_instance, sizeof (buffer));
  }
  strlcat (buffer, "/", sizeof (buffer));
  strlcat (buffer, ident->type, sizeof (buffer));
  if (ident->type_instance[0] != 0)
  {
    strlcat (buffer, "-", sizeof (buffer));
    strlcat (buffer, ident->type_instance, sizeof (buffer));
  }

  return (strdup (buffer));
} /* }}} char *ident_to_string */

char *ident_to_file (const graph_ident_t *ident) /* {{{ */
{
  char buffer[PATH_MAX];

  buffer[0] = 0;

  strlcat (buffer, DATA_DIR, sizeof (buffer));
  strlcat (buffer, "/", sizeof (buffer));

  strlcat (buffer, ident->host, sizeof (buffer));
  strlcat (buffer, "/", sizeof (buffer));
  strlcat (buffer, ident->plugin, sizeof (buffer));
  if (ident->plugin_instance[0] != 0)
  {
    strlcat (buffer, "-", sizeof (buffer));
    strlcat (buffer, ident->plugin_instance, sizeof (buffer));
  }
  strlcat (buffer, "/", sizeof (buffer));
  strlcat (buffer, ident->type, sizeof (buffer));
  if (ident->type_instance[0] != 0)
  {
    strlcat (buffer, "-", sizeof (buffer));
    strlcat (buffer, ident->type_instance, sizeof (buffer));
  }

  strlcat (buffer, ".rrd", sizeof (buffer));

  return (strdup (buffer));
} /* }}} char *ident_to_file */

#define ADD_FIELD(field) do {                              \
  char json[1024];                                         \
  json_escape_copy (json, ident->field, sizeof (json));    \
  strlcat (buffer, json, sizeof (buffer));                 \
} while (0)

char *ident_to_json (const graph_ident_t *ident) /* {{{ */
{
  char buffer[4096];

  buffer[0] = 0;

  strlcat (buffer, "{\"host\":\"", sizeof (buffer));
  ADD_FIELD (host);
  strlcat (buffer, "\",\"plugin\":\"", sizeof (buffer));
  ADD_FIELD (plugin);
  strlcat (buffer, "\",\"plugin_instance\":\"", sizeof (buffer));
  ADD_FIELD (plugin_instance);
  strlcat (buffer, "\",\"type\":\"", sizeof (buffer));
  ADD_FIELD (type);
  strlcat (buffer, "\",\"type_instance\":\"", sizeof (buffer));
  ADD_FIELD (type_instance);
  strlcat (buffer, "\"}", sizeof (buffer));

  return (strdup (buffer));
} /* }}} char *ident_to_json */

#undef ADD_FIELD

time_t ident_get_mtime (const graph_ident_t *ident) /* {{{ */
{
  char *file;
  struct stat statbuf;
  int status;

  if (ident == NULL)
    return (0);

  file = ident_to_file (ident);
  if (file == NULL)
    return (0);

  memset (&statbuf, 0, sizeof (statbuf));
  status = stat (file, &statbuf);
  if (status != 0)
  {
    fprintf (stderr, "ident_get_mtime: stat'ing file \"%s\" failed: %s\n",
        file, strerror (errno));
    return (0);
  }

  free (file);
  return (statbuf.st_mtime);
} /* }}} time_t ident_get_mtime */

/* vim: set sw=2 sts=2 et fdm=marker : */

