#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <fcgiapp.h>
#include <fcgi_stdio.h>

#include "common.h"
#include "graph_list.h"
#include "utils_params.h"

#include "action_list_graphs.h"

struct str_array_s
{
  char **ptr;
  size_t size;
};
typedef struct str_array_s str_array_t;

static str_array_t *array_alloc (void) /* {{{ */
{
  str_array_t *a;

  a = malloc (sizeof (*a));
  if (a == NULL)
    return (NULL);

  memset (a, 0, sizeof (*a));
  a->ptr = NULL;
  a->size = 0;

  return (a);
} /* }}} str_array_t *array_alloc */

static void array_free (str_array_t *a) /* {{{ */
{
  if (a == NULL)
    return;

  free (a->ptr);
  a->ptr = NULL;
  a->size = 0;

  free (a);
} /* }}} void array_free */

static int array_add (const char *entry, void *user_data) /* {{{ */
{
  str_array_t *a = user_data;
  char **ptr;

  if ((entry == NULL) || (a == NULL))
    return (EINVAL);

  ptr = realloc (a->ptr, sizeof (*a->ptr) * (a->size + 1));
  if (ptr == NULL)
    return (ENOMEM);
  a->ptr = ptr;
  ptr = a->ptr + a->size;

  *ptr = strdup (entry);
  if (*ptr == NULL)
    return (ENOMEM);

  a->size++;
  return (0);
} /* }}} int array_add */

static int print_graph (const graph_list_t *gl, void *user_data) /* {{{ */
{
  if (gl == NULL)
    return (EINVAL);

  printf ("host = %s; plugin = %s;", gl->host, gl->plugin);
  if (gl->plugin_instance != NULL)
    printf (" plugin_instance = %s;", gl->plugin_instance);
  printf (" type = %s;", gl->type);
  if (gl->type_instance != NULL)
    printf (" type_instance = %s;", gl->type_instance);
  printf ("\n");

  return (0);
} /* }}} int print_graph */

static int get_graphs_list (char ***ret_graphs, /* {{{ */
    size_t *ret_graphs_num)
{
  gl_update ();
  gl_foreach (print_graph, /* user_data = */ NULL);

  return (0);
} /* }}} int get_graphs_list */

static int action_hello (void) /* {{{ */
{
  printf ("Content-Type: text/plain\n\n");

  get_graphs_list (NULL, NULL);

  return (0);
} /* }}} int action_hello */

static int action_usage (void) /* {{{ */
{
  printf ("Content-Type: text/plain\n\n");

  fputs ("Usage:\n"
      "\n"
      "  Available actions:\n"
      "\n"
      "    * hello\n"
      "\n", stdout);

  return (0);
} /* }}} int action_usage */

static int handle_request (void) /* {{{ */
{
  const char *action;

  param_init ();

  action = param ("action");
  if (action == NULL)
  {
    return (action_usage ());
  }
  else if (strcmp ("list_graphs", action) == 0)
  {
    return (action_list_graphs ());
  }
  else if (strcmp ("hello", action) == 0)
  {
    return (action_hello ());
  }
  else
  {
    return (action_usage ());
  }
} /* }}} int handle_request */

static int run (void) /* {{{ */
{
  while (FCGI_Accept() >= 0)
  {
    handle_request ();
    param_finish ();
  }

  return (0);
} /* }}} int run */

int main (int argc, char **argv) /* {{{ */
{
  int status;

  argc = 0;
  argv = NULL;

  if (FCGX_IsCGI ())
    status = handle_request ();
  else
    status = run ();

  exit ((status == 0) ? EXIT_SUCCESS : EXIT_FAILURE);
} /* }}} int main */

/* vim: set sw=2 sts=2 et fdm=marker : */
