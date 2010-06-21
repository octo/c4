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

static int print_graph_inst_json (graph_config_t *cfg, /* {{{ */
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
} /* }}} int print_graph_inst_json */

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
    gl_instance_get_all (print_graph_inst_json, /* user_data = */ &data);
  else
    gl_search (term, print_graph_inst_json, /* user_data = */ &data);

  if (!data.first)
    json_end_graph ();

  printf ("\n]");

  return (0);
} /* }}} int list_graphs_json */

static int print_graph_inst_html (graph_config_t *cfg, /* {{{ */
    graph_instance_t *inst,
    void *user_data)
{
  callback_data_t *data = user_data;
  char params[1024];
  char desc[1024];

  if (data->cfg != cfg)
  {
    if (data->cfg != NULL)
      printf ("  </ul></li>\n");

    memset (desc, 0, sizeof (desc));
    graph_get_title (cfg, desc, sizeof (desc));
    html_escape_buffer (desc, sizeof (desc));

    printf ("  <li class=\"graph\">%s\n"
        "  <ul class=\"instance_list\">\n", desc);

    data->cfg = cfg;
  }

  memset (params, 0, sizeof (params));
  inst_get_params (cfg, inst, params, sizeof (params));
  html_escape_buffer (params, sizeof (params));

  memset (desc, 0, sizeof (desc));
  inst_describe (cfg, inst, desc, sizeof (desc));
  html_escape_buffer (desc, sizeof (desc));

  printf ("    <li class=\"instance\"><a href=\"%s?action=graph;%s\">%s</a></li>\n",
      script_name (), params, desc);

  if (data->limit > 0)
    data->limit--;

  /* Abort scan if limit is reached. */
  if (data->limit == 0)
    return (1);

  return (0);
} /* }}} int print_graph_inst_html */

struct page_data_s
{
  const char *search_term;
};
typedef struct page_data_s page_data_t;

static int print_search_box (void *user_data) /* {{{ */
{
  page_data_t *data = user_data;
  char *term_html;

  if (data == NULL)
  {
    fprintf (stderr, "print_search_box: data == NULL\n");
    return (EINVAL);
  }

  term_html = html_escape (data->search_term);

  printf ("<form action=\"%s\" method=\"get\">\n"
      "  <input type=\"hidden\" name=\"action\" value=\"list_graphs\" />\n"
      "  <input type=\"text\" name=\"search\" value=\"%s\" id=\"search-input\" />\n"
      "  <input type=\"submit\" name=\"button\" value=\"Search\" />\n"
      "</form>\n",
      script_name (),
      (term_html != NULL) ? term_html : "");

  free (term_html);

  return (0);
} /* }}} int print_search_box */

static int print_search_result (void *user_data) /* {{{ */
{
  page_data_t *pg_data = user_data;
  callback_data_t cb_data = { NULL, /* limit = */ RESULT_LIMIT, /* first = */ 1 };

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

  printf ("<ul id=\"host-list\">\n");
  for (i = 0; i < hosts_argc; i++)
  {
    char *host = hosts_argv[i];
    char *host_html;

    if ((data.last_host != NULL) && (strcmp (data.last_host, host) == 0))
      continue;
    data.last_host = host;

    host_html = html_escape (host);

    printf ("  <li><a href=\"%s?action=list_graphs&search=%s\">%s</a></li>\n",
        script_name (), host_html, host_html);

    free (host_html);
  }
  printf ("</ul>\n");

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

  pg_callbacks.top_right = print_search_box;
  pg_callbacks.middle_left = print_host_list;
  pg_callbacks.middle_center = print_search_result;

  html_print_page (title, &pg_callbacks, &pg_data);

  return (0);
} /* }}} int list_graphs_html */

int action_list_graphs (void) /* {{{ */
{
  const char *format;
  char *search;
  int status;

  gl_update ();

  search = strtolower_copy (param ("search"));

  format = param ("format");
  if (format == NULL)
    format = "html";

  if (strcmp ("json", format) == 0)
    status = list_graphs_json (search);
  else
    status = list_graphs_html (search);

  free (search);

  return (status);
} /* }}} int action_list_graphs */

/* vim: set sw=2 sts=2 et fdm=marker : */
