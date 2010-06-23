#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "action_search_json.h"
#include "common.h"
#include "graph.h"
#include "graph_ident.h"
#include "graph_instance.h"
#include "graph_list.h"
#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

#define RESULT_LIMIT 10

struct callback_data_s
{
  graph_config_t *cfg;
  int limit;
  _Bool first;
};
typedef struct callback_data_s callback_data_t;

static int json_begin_graph (graph_config_t *cfg) /* {{{ */
{
  char desc[1024];

  if (cfg == NULL)
    return (EINVAL);

  graph_get_title (cfg, desc, sizeof (desc));

  printf ("{\"title\":\"%s\",\"instances\":[", desc);

  return (0);
} /* }}} int json_begin_graph */

static int json_end_graph (void) /* {{{ */
{
  printf ("]}");

  return (0);
} /* }}} int json_end_graph */

static int json_print_instance (graph_config_t *cfg, /* {{{ */
    graph_instance_t *inst)
{
  char params[1024];
  char desc[1024];

  if ((cfg == NULL) || (inst == NULL))
    return (EINVAL);

  memset (desc, 0, sizeof (desc));
  inst_describe (cfg, inst, desc, sizeof (desc));

  memset (params, 0, sizeof (params));
  inst_get_params (cfg, inst, params, sizeof (params));

  printf ("{\"description\":\"%s\",\"params\":\"%s\"}",
      desc, params);

  return (0);
} /* }}} int json_print_instance */

static int json_print_graph_instance (graph_config_t *cfg, /* {{{ */
    graph_instance_t *inst,
    void *user_data)
{
  callback_data_t *data = user_data;

  if (data->cfg != cfg)
  {
    if (!data->first)
    {
      json_end_graph ();
      printf (",\n");
    }
    json_begin_graph (cfg);

    data->cfg = cfg;
    data->first = 0;
  }
  else /* if (not first instance) */
  {
    printf (",\n");
  }

  json_print_instance (cfg, inst);

  if (data->limit > 0)
    data->limit--;

  if (data->limit == 0)
    return (1);

  return (0);
} /* }}} int json_print_graph_instance */

static int list_graphs_json (const char *term) /* {{{ */
{
  callback_data_t data;

  time_t now;
  char time_buffer[128];
  int status;

  printf ("Content-Type: application/json\n");

  now = time (NULL);
  status = time_to_rfc1123 (now + 300, time_buffer, sizeof (time_buffer));
  if (status == 0)
    printf ("Expires: %s\n"
        "Cache-Control: public\n",
        time_buffer);
  printf ("\n");

  data.cfg = NULL;
  data.limit = RESULT_LIMIT;
  data.first = 1;

  printf ("[\n");
  if (term == NULL)
    gl_instance_get_all (json_print_graph_instance, /* user_data = */ &data);
  else
    gl_search (term, json_print_graph_instance, /* user_data = */ &data);

  if (!data.first)
    json_end_graph ();

  printf ("\n]");

  return (0);
} /* }}} int list_graphs_json */

int action_search_json (void) /* {{{ */
{
  char *search;
  int status;

  gl_update ();

  search = strtolower_copy (param ("q"));

  status = list_graphs_json (search);

  free (search);

  return (status);
} /* }}} int action_search_json */

/* vim: set sw=2 sts=2 et fdm=marker : */
