#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "action_list_graphs.h"
#include "common.h"
#include "graph.h"
#include "graph_ident.h"
#include "graph_list.h"
#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

#define RESULT_LIMIT 50

struct callback_data_s
{
  graph_config_t *cfg;
  int graph_index;
  int graph_limit;
  _Bool graph_more;
  int inst_index;
  int inst_limit;
  _Bool inst_more;
};
typedef struct callback_data_s callback_data_t;

static int print_graph_inst_html (graph_config_t *cfg, /* {{{ */
    graph_instance_t *inst,
    void *user_data)
{
  callback_data_t *data = user_data;
  char params[1024];
  char desc[1024];

  if (data->cfg != cfg)
  {
    data->graph_index++;
    if (data->graph_index >= data->graph_limit)
    {
      data->graph_more = 1;
      return (1);
    }

    if (data->cfg != NULL)
      printf ("  </ul></li>\n");

    memset (desc, 0, sizeof (desc));
    graph_get_title (cfg, desc, sizeof (desc));
    html_escape_buffer (desc, sizeof (desc));

    printf ("  <li class=\"graph\">%s\n"
        "  <ul class=\"instance_list\">\n", desc);

    data->cfg = cfg;
    data->inst_index = -1;
    data->inst_more = 0;
  }

  data->inst_index++;
  if (data->inst_index >= data->inst_limit)
  {
    if (!data->inst_more)
    {
      printf ("    <li class=\"instance more\">More ...</li>\n");
      data->inst_more = 1;
    }
    return (0);
  }

  memset (params, 0, sizeof (params));
  inst_get_params (cfg, inst, params, sizeof (params));
  html_escape_buffer (params, sizeof (params));

  memset (desc, 0, sizeof (desc));
  inst_describe (cfg, inst, desc, sizeof (desc));
  html_escape_buffer (desc, sizeof (desc));

  printf ("    <li class=\"instance\"><a href=\"%s?action=show_graph;%s\">%s</a></li>\n",
      script_name (), params, desc);

  return (0);
} /* }}} int print_graph_inst_html */

struct page_data_s
{
  const char *search_term;
};
typedef struct page_data_s page_data_t;

static int print_search_result (void *user_data) /* {{{ */
{
  page_data_t *pg_data = user_data;
  callback_data_t cb_data = { /* cfg = */ NULL,
    /* graph_index = */ -1, /* graph_limit = */ 20, /* graph_more = */ 0,
    /* inst_index = */  -1, /* inst_limit = */   5, /* inst more = */  0 };

  if (pg_data->search_term != NULL)
  {
    char *search_term_html = html_escape (pg_data->search_term);
    printf ("    <h2>Search results for &quot;%s&quot;</h2>\n",
        search_term_html);
    free (search_term_html);
  }

  printf ("    <ul id=\"search-output\" class=\"graph_list\">\n");
  if (pg_data->search_term == NULL)
    gl_instance_get_all (print_graph_inst_html, /* user_data = */ &cb_data);
  else
  {
    char *term_lc = strtolower_copy (pg_data->search_term);
    gl_search (term_lc, print_graph_inst_html, /* user_data = */ &cb_data);
    free (term_lc);
  }

  if (cb_data.cfg != NULL)
    printf ("      </ul></li>\n");

  if (cb_data.graph_more)
  {
    printf ("    <li class=\"graph more\">More ...</li>\n");
  }

  printf ("    </ul>\n");

  return (0);
} /* }}} int print_search_result */

struct print_host_list_data_s
{
  str_array_t *array;
  char *last_host;
};
typedef struct print_host_list_data_s print_host_list_data_t;

static int print_host_list_callback (graph_config_t *cfg, /* {{{ */
    graph_instance_t *inst, void *user_data)
{
  print_host_list_data_t *data = user_data;
  graph_ident_t *ident;
  const char *host;

  /* make compiler happy */
  cfg = NULL;

  ident = inst_get_selector (inst);
  if (ident == NULL)
    return (-1);

  host = ident_get_host (ident);
  if (host == NULL)
  {
    ident_destroy (ident);
    return (-1);
  }

  if (IS_ALL (host))
    return (0);

  if ((data->last_host != NULL)
      && (strcmp (data->last_host, host) == 0))
  {
    ident_destroy (ident);
    return (0);
  }

  free (data->last_host);
  data->last_host = strdup (host);

  array_append (data->array, host);

  ident_destroy (ident);
  return (0);
} /* }}} int print_host_list_callback */

static int print_host_list (__attribute__((unused)) void *user_data) /* {{{ */
{
  print_host_list_data_t data;
  int hosts_argc;
  char **hosts_argv;
  int i;

  data.array = array_create ();
  data.last_host = NULL;

  gl_instance_get_all (print_host_list_callback, &data);

  free (data.last_host);
  data.last_host = NULL;

  array_sort (data.array);

  hosts_argc = array_argc (data.array);
  hosts_argv = array_argv (data.array);

  if (hosts_argc < 1)
  {
    array_destroy (data.array);
    return (0);
  }

  printf ("<div><h3>List of hosts</h3>\n"
      "<ul id=\"host-list\">\n");
  for (i = 0; i < hosts_argc; i++)
  {
    char *host = hosts_argv[i];
    char *host_html;

    if ((data.last_host != NULL) && (strcmp (data.last_host, host) == 0))
      continue;
    data.last_host = host;

    host_html = html_escape (host);

    printf ("  <li><a href=\"%s?action=list_graphs&q=%s\">%s</a></li>\n",
        script_name (), host_html, host_html);

    free (host_html);
  }
  printf ("</ul></div>\n");

  array_destroy (data.array);

  return (0);
} /* }}} int print_host_list */

static int list_graphs_html (const char *term) /* {{{ */
{
  page_data_t pg_data;
  page_callbacks_t pg_callbacks = PAGE_CALLBACKS_INIT;
  char title[512];

  if (term != NULL)
    snprintf (title, sizeof (title), "c4: Graphs matching \"%s\"", term);
  else
    strncpy (title, "c4: List of all graphs", sizeof (title));
  title[sizeof (title) - 1] = 0;

  memset (&pg_data, 0, sizeof (pg_data));
  pg_data.search_term = term;

  pg_callbacks.top_right = html_print_search_box;
  pg_callbacks.middle_left = print_host_list;
  pg_callbacks.middle_center = print_search_result;

  html_print_page (title, &pg_callbacks, &pg_data);

  return (0);
} /* }}} int list_graphs_html */

int action_list_graphs (void) /* {{{ */
{
  char *search;
  int status;

  gl_update ();

  search = strtolower_copy (param ("q"));
  status = list_graphs_html (search);
  free (search);

  return (status);
} /* }}} int action_list_graphs */

/* vim: set sw=2 sts=2 et fdm=marker : */
