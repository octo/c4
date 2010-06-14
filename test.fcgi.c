#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "common.h"
#include "graph_list.h"
#include "utils_params.h"

#include "action_graph.h"
#include "action_list_graphs.h"

/* Include this last, so the macro magic of <fcgi_stdio.h> doesn't interfere
 * with our own header files. */
#include <fcgiapp.h>
#include <fcgi_stdio.h>

struct action_s
{
  const char *name;
  int (*callback) (void);
};
typedef struct action_s action_t;

static int action_usage (void);

static const action_t actions[] =
{
  { "graph",       action_graph },
  { "list_graphs", action_list_graphs },
  { "usage",       action_usage }
};
static const size_t actions_num = sizeof (actions) / sizeof (actions[0]);


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
