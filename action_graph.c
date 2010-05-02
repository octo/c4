#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <fcgiapp.h>
#include <fcgi_stdio.h>

#include "action_graph.h"
#include "graph_list.h"
#include "utils_params.h"

int action_graph (void)
{
  printf ("Content-Type: text/plain\n\n"
      "Hello, this is %s\n", __func__);

  return (0);
} /* }}} int action_graph */

/* vim: set sw=2 sts=2 et fdm=marker : */
