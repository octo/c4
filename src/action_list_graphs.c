#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "action_list_graphs.h"
#include "common.h"
#include "graph.h"
#include "graph_ident.h"
#include "graph_instance.h"
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

  printf ("    <li class=\"instance\"><a href=\"%s?action=show_instance;%s\">%s</a></li>\n",
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

    if (strncmp ("host:", term_lc, strlen ("host:")) == 0)
      gl_search_field (GIF_HOST, term_lc + strlen ("host:"),
          print_graph_inst_html, /* user_data = */ &cb_data);
    else if (strncmp ("plugin:", term_lc, strlen ("plugin:")) == 0)
      gl_search_field (GIF_PLUGIN, term_lc + strlen ("plugin:"),
          print_graph_inst_html, /* user_data = */ &cb_data);
    else if (strncmp ("plugin_instance:", term_lc, strlen ("plugin_instance:")) == 0)
      gl_search_field (GIF_PLUGIN_INSTANCE, term_lc + strlen ("plugin_instance:"),
          print_graph_inst_html, /* user_data = */ &cb_data);
    else if (strncmp ("type:", term_lc, strlen ("type:")) == 0)
      gl_search_field (GIF_TYPE, term_lc + strlen ("type:"),
          print_graph_inst_html, /* user_data = */ &cb_data);
    else if (strncmp ("type_instance:", term_lc, strlen ("type_instance:")) == 0)
      gl_search_field (GIF_TYPE_INSTANCE, term_lc + strlen ("type_instance:"),
          print_graph_inst_html, /* user_data = */ &cb_data);
    else
      gl_search (term_lc,
          print_graph_inst_html, /* user_data = */ &cb_data);

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

static int print_host_list_callback (const char *host, void *user_data) /* {{{ */
{
  char *host_html;

  /* Make compiler happy */
  user_data = NULL;

  if (host == NULL)
    return (EINVAL);
  
  host_html = html_escape (host);
  if (host_html == NULL)
    return (ENOMEM);

  printf ("  <li class=\"host\"><a href=\"%s?action=list_graphs;q=host:%s\">"
      "%s</a></li>\n",
      script_name (), host_html, host_html);

  return (0);
} /* }}} int print_host_list_callback */

static int print_host_list (__attribute__((unused)) void *user_data) /* {{{ */
{
  printf ("<div><h3>List of hosts</h3>\n"
      "<ul id=\"host-list\">\n");
  gl_foreach_host (print_host_list_callback, /* user data = */ NULL);
  printf ("</ul></div>\n");

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
