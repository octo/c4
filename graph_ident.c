#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h> /* PATH_MAX */

#include "graph_ident.h"
#include "common.h"

#define ANY_TOKEN "/any/"
#define ALL_TOKEN "/all/"

#define IS_ANY(str) (((str) != NULL) && (strcasecmp (ANY_TOKEN, (str)) == 0))
#define IS_ALL(str) (((str) != NULL) && (strcasecmp (ALL_TOKEN, (str)) == 0))

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
    const char *part, _Bool keep_all_selector)
{
  if ((selector == NULL) || (part == NULL))
    return (NULL);

  if (IS_ANY (part))
    return (NULL);

  if (!keep_all_selector && IS_ALL (part))
    return (NULL);

  /* ANY in the graph selection => concrete value in the instance. */
  if (IS_ANY (selector))
    return (strdup (part));

  if (IS_ALL (selector))
  {
    if (keep_all_selector)
      return (strdup (ALL_TOKEN));
    else
      return (strdup (part));
  }

  if (strcmp (selector, part) != 0)
    return (NULL);

  return (strdup (selector));
} /* }}} char *part_copy_with_selector */

static _Bool part_matches (const char *selector, /* {{{ */
    const char *part)
{
  if ((selector == NULL) && (part == NULL))
    return (1);

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

graph_ident_t *ident_clone (const graph_ident_t *ident)
{
  return (ident_create (ident->host,
        ident->plugin, ident->plugin_instance,
        ident->type, ident->type_instance));
} /* }}} graph_ident_t *ident_clone */

graph_ident_t *ident_copy_with_selector (const graph_ident_t *selector, /* {{{ */
    const graph_ident_t *ident, _Bool keep_all_selector)
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
  ret->p = part_copy_with_selector (selector->p, ident->p, \
      keep_all_selector);                                  \
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

const char *ident_get_host (graph_ident_t *ident) /* {{{ */
{
  if (ident == NULL)
    return (NULL);

  return (ident->host);
} /* }}} char *ident_get_host */

const char *ident_get_plugin (graph_ident_t *ident) /* {{{ */
{
  if (ident == NULL)
    return (NULL);

  return (ident->plugin);
} /* }}} char *ident_get_plugin */

const char *ident_get_plugin_instance (graph_ident_t *ident) /* {{{ */
{
  if (ident == NULL)
    return (NULL);

  return (ident->plugin_instance);
} /* }}} char *ident_get_plugin_instance */

const char *ident_get_type (graph_ident_t *ident) /* {{{ */
{
  if (ident == NULL)
    return (NULL);

  return (ident->type);
} /* }}} char *ident_get_type */

const char *ident_get_type_instance (graph_ident_t *ident) /* {{{ */
{
  if (ident == NULL)
    return (NULL);

  return (ident->type_instance);
} /* }}} char *ident_get_type_instance */

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
  if ((selector == NULL) && (ident == NULL))
    return (0);
  else if (selector == NULL)
    return (-1);
  else if (ident == NULL)
    return (1);

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

char *ident_to_json (const graph_ident_t *ident) /* {{{ */
{
  char buffer[4096];

  buffer[0] = 0;

  strlcat (buffer, "{\"host\":\"", sizeof (buffer));
  strlcat (buffer, ident->host, sizeof (buffer));
  strlcat (buffer, "\",\"plugin\":\"", sizeof (buffer));
  strlcat (buffer, ident->plugin, sizeof (buffer));
  strlcat (buffer, "\",\"plugin_instance\":\"", sizeof (buffer));
  strlcat (buffer, ident->plugin_instance, sizeof (buffer));
  strlcat (buffer, "\",\"type\":\"", sizeof (buffer));
  strlcat (buffer, ident->type, sizeof (buffer));
  strlcat (buffer, "\",\"type_instance\":\"", sizeof (buffer));
  strlcat (buffer, ident->type_instance, sizeof (buffer));
  strlcat (buffer, "\"}", sizeof (buffer));

  return (strdup (buffer));
} /* }}} char *ident_to_json */

/* vim: set sw=2 sts=2 et fdm=marker : */

