#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include "graph_list.h"
#include "common.h"

#define UPDATE_INTERVAL 10

static graph_list_t *graph_list = NULL;
static size_t graph_list_length = 0;
static time_t gl_last_update = 0;

/* "Safe" version of strcmp(3): Either or both pointers may be NULL. */
static int strcmp_s (const char *s1, const char *s2) /* {{{ */
{
  if ((s1 == NULL) && (s2 == NULL))
    return (0);
  else if (s1 == NULL)
    return (1);
  else if (s2 == NULL)
    return (-1);
  assert ((s1 != NULL) && (s2 != NULL));

  return (strcmp (s1, s2));
} /* }}} int strcmp_s */

static int gl_compare (const void *p0, const void *p1) /* {{{ */
{
  const graph_list_t *gl0 = p0;
  const graph_list_t *gl1 = p1;
  int status;

  status = strcmp (gl0->host, gl1->host);
  if (status != 0)
    return (status);

  status = strcmp (gl0->plugin, gl1->plugin);
  if (status != 0)
    return (status);

  status = strcmp_s (gl0->plugin_instance, gl1->plugin_instance);
  if (status != 0)
    return (status);

  status = strcmp (gl0->type, gl1->type);
  if (status != 0)
    return (status);

  return (strcmp_s (gl0->type_instance, gl1->type_instance));
} /* }}} int gl_compare */

static void gl_clear_entry (graph_list_t *gl) /* {{{ */
{
  if (gl == NULL)
    return;

  free (gl->host);
  free (gl->plugin);
  free (gl->plugin_instance);
  free (gl->type);
  free (gl->type_instance);

  gl->host = NULL;
  gl->plugin = NULL;
  gl->plugin_instance = NULL;
  gl->type = NULL;
  gl->type_instance = NULL;
} /* }}} void gl_clear_entry */

static void gl_clear (void) /* {{{ */
{
  size_t i;

  for (i = 0; i < graph_list_length; i++)
    gl_clear_entry (graph_list + i);

  free (graph_list);
  graph_list = NULL;
  graph_list_length = 0;
  gl_last_update = 0;
} /* }}} void gl_clear */

static int gl_add_copy (graph_list_t *gl) /* {{{ */
{
  graph_list_t *ptr;
  int status;

  if (gl == NULL)
    return (EINVAL);

  ptr = realloc (graph_list, sizeof (*graph_list) * (graph_list_length + 1));
  if (ptr == NULL)
    return (ENOMEM);
  graph_list = ptr;

  ptr = graph_list + graph_list_length;
  memset (ptr, 0, sizeof (*ptr));
  ptr->host = NULL;
  ptr->plugin = NULL;
  ptr->plugin_instance = NULL;
  ptr->type = NULL;
  ptr->type_instance = NULL;

#define DUP_OR_BREAK(member) do {                    \
  ptr->member = NULL;                                \
  if (gl->member != NULL)                            \
  {                                                  \
    ptr->member = strdup (gl->member);               \
    if (ptr->member == NULL)                         \
      break;                                         \
  }                                                  \
} while (0)

  status = ENOMEM;
  do
  {
    DUP_OR_BREAK(host);
    DUP_OR_BREAK(plugin);
    DUP_OR_BREAK(plugin_instance);
    DUP_OR_BREAK(type);
    DUP_OR_BREAK(type_instance);

    status = 0;
  } while (0);

#undef DUP_OR_BREAK

  if (status != 0)
  {
    free (ptr->host);
    free (ptr->plugin);
    free (ptr->plugin_instance);
    free (ptr->type);
    free (ptr->type_instance);
    return (status);
  }

  graph_list_length++;
  return (0);
} /* }}} int gl_add_copy */

static int callback_type (const char *type, void *user_data) /* {{{ */
{
  graph_list_t *gl;
  int status;

  if ((type == NULL) || (user_data == NULL))
    return (EINVAL);

  gl = user_data;
  if ((gl->type != NULL) || (gl->type_instance != NULL))
    return (EINVAL);

  gl->type = strdup (type);
  if (gl->type == NULL)
    return (ENOMEM);

  gl->type_instance = strchr (gl->type, '-');
  if (gl->type_instance != NULL)
  {
    *gl->type_instance = 0;
    gl->type_instance++;
  }

  status = gl_add_copy (gl);

  free (gl->type);
  gl->type = NULL;
  gl->type_instance = NULL;

  return (status);
} /* }}} int callback_type */

static int callback_plugin (const char *plugin, void *user_data) /* {{{ */
{
  graph_list_t *gl;
  int status;

  if ((plugin == NULL) || (user_data == NULL))
    return (EINVAL);

  gl = user_data;
  if ((gl->plugin != NULL) || (gl->plugin_instance != NULL))
    return (EINVAL);

  gl->plugin = strdup (plugin);
  if (gl->plugin == NULL)
    return (ENOMEM);

  gl->plugin_instance = strchr (gl->plugin, '-');
  if (gl->plugin_instance != NULL)
  {
    *gl->plugin_instance = 0;
    gl->plugin_instance++;
  }

  status = foreach_type (gl->host, plugin, callback_type, gl);

  free (gl->plugin);
  gl->plugin = NULL;
  gl->plugin_instance = NULL;

  return (status);
} /* }}} int callback_plugin */

static int callback_host (const char *host, void *user_data) /* {{{ */
{
  graph_list_t *gl;
  int status;

  if ((host == NULL) || (user_data == NULL))
    return (EINVAL);

  gl = user_data;
  if (gl->host != NULL)
    return (EINVAL);

  gl->host = strdup (host);
  if (gl->host == NULL)
    return (ENOMEM);

  status =  foreach_plugin (host, callback_plugin, gl);

  free (gl->host);
  gl->host = NULL;

  return (status);
} /* }}} int callback_host */

int gl_update (void) /* {{{ */
{
  time_t now;
  graph_list_t gl;
  int status;

  now = time (NULL);

  if ((gl_last_update + UPDATE_INTERVAL) >= now)
    return (0);

  gl_clear ();

  memset (&gl, 0, sizeof (gl));
  gl.host = NULL;
  gl.plugin = NULL;
  gl.plugin_instance = NULL;
  gl.type = NULL;
  gl.type_instance = NULL;

  status = foreach_host (callback_host, &gl);

  if (graph_list_length > 1)
    qsort (graph_list, graph_list_length, sizeof (*graph_list), gl_compare);

  return (status);
} /* }}} int gl_update */

int gl_foreach (gl_callback callback, void *user_data) /* {{{ */
{
  size_t i;

  for (i = 0; i < graph_list_length; i++)
  {
    int status;

    status = (*callback) (graph_list + i, user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int gl_foreach */

/* vim: set sw=2 sts=2 et fdm=marker : */
