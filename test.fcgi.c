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

#include "action_graph.h"
#include "action_list_graphs.h"

struct action_s
{
  const char *name;
  int (*callback) (void);
};
typedef struct action_s action_t;

#if 0
struct str_array_s
{
  char **ptr;
  size_t size;
};
typedef struct str_array_s str_array_t;
#endif

static int action_usage (void);

static const action_t actions[] =
{
  { "graph",       action_graph },
  { "list_graphs", action_list_graphs },
  { "usage",       action_usage }
};
static const size_t actions_num = sizeof (actions) / sizeof (actions[0]);

#if 0
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
#endif

static int action_usage (void) /* {{{ */
{
  size_t i;

  printf ("Content-Type: text/plain\n\n");

  printf ("Usage:\n"
      "\n"
      "  Available actions:\n"
      "\n");

  for (i = 0; i < actions_num; i++)
    printf ("  * %s\n", actions[i].name);

  printf ("\n");

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
  else
  {
    size_t i;

    for (i = 0; i < actions_num; i++)
    {
      if (strcmp (action, actions[i].name) == 0)
        return ((*actions[i].callback) ());
    }

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
