#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "graph_def.h"
#include "common.h"

/*
 * Data structures
 */
struct graph_def_s
{
  graph_ident_t *select;

  graph_def_t *next;
};

/*
 * Private functions
 */

/*
 * Public functions
 */
graph_def_t *def_create (graph_config_t *cfg, graph_ident_t *ident) /* {{{ */
{
  graph_ident_t *selector;
  graph_def_t *ret;

  selector = gl_graph_get_selector (cfg);
  if (selector == NULL)
    return (NULL);

  ret = malloc (sizeof (*ret));
  if (ret == NULL)
  {
    ident_destroy (selector);
    return (NULL);
  }
  memset (ret, 0, sizeof (*ret));
  ret->next = NULL;

  ret->select = ident_copy_with_selector (selector, ident,
      IDENT_FLAG_REPLACE_ALL);
  if (ret->select == NULL)
  {
    ident_destroy (selector);
    free (ret);
    return (NULL);
  }

  ident_destroy (selector);
  return (ret);
}; /* }}} graph_def_t *def_create */

void def_destroy (graph_def_t *def) /* {{{ */
{
  graph_def_t *next;

  if (def == NULL)
    return;

  next = def->next;

  ident_destroy (def->select);

  free (def);

  def_destroy (next);
} /* }}} void def_destroy */

graph_def_t *def_search (graph_def_t *head, graph_ident_t *ident) /* {{{ */
{
  graph_def_t *ptr;

  if ((head == NULL) || (ident == NULL))
    return (NULL);

  for (ptr = head; ptr != NULL; ptr = ptr->next)
    if (ident_matches (ptr->select, ident))
      return (ptr);

  return (NULL);
} /* }}} graph_def_t *def_search */

int def_get_rrdargs (graph_def_t *def, graph_ident_t *ident, /* {{{ */
    str_array_t *args)
{
  char *file;
  char **dses = NULL;
  size_t dses_num = 0;
  int status;
  size_t i;

  if ((def == NULL) || (ident == NULL) || (args == NULL))
    return (EINVAL);

  file = ident_to_file (ident);
  if (file == NULL)
  {
    DEBUG ("gl_ident_get_rrdargs: ident_to_file returned NULL.\n");
    return (-1);
  }

  DEBUG ("gl_ident_get_rrdargs: file = %s;\n", file);

  status = ds_list_from_rrd_file (file, &dses_num, &dses);
  if (status != 0)
  {
    free (file);
    return (status);
  }

  for (i = 0; i < dses_num; i++)
  {
    int index;

    DEBUG ("gl_ident_get_rrdargs: ds[%lu] = %s;\n", (unsigned long) i, dses[i]);

    index = array_argc (args);

    /* CDEFs */
    array_append_format (args, "DEF:def_%04i_min=%s:%s:MIN",
        index, file, dses[i]);
    array_append_format (args, "DEF:def_%04i_avg=%s:%s:AVERAGE",
        index, file, dses[i]);
    array_append_format (args, "DEF:def_%04i_max=%s:%s:MAX",
        index, file, dses[i]);
    /* VDEFs */
    array_append_format (args, "VDEF:vdef_%04i_min=def_%04i_min,MINIMUM",
        index, index);
    array_append_format (args, "VDEF:vdef_%04i_avg=def_%04i_avg,AVERAGE",
        index, index);
    array_append_format (args, "VDEF:vdef_%04i_max=def_%04i_max,MAXIMUM",
        index, index);
    array_append_format (args, "VDEF:vdef_%04i_lst=def_%04i_avg,LAST",
        index, index);

    /* Graph part */
    array_append_format (args, "LINE1:def_%04i_avg#%06"PRIx32":%s",
        index, get_random_color (), dses[i]);
    array_append_format (args, "GPRINT:vdef_%04i_min:%%lg min,", index);
    array_append_format (args, "GPRINT:vdef_%04i_avg:%%lg avg,", index);
    array_append_format (args, "GPRINT:vdef_%04i_max:%%lg max,", index);
    array_append_format (args, "GPRINT:vdef_%04i_lst:%%lg last\\l", index);

    free (dses[i]);
  }

  free (dses);
  free (file);

  return (0);
} /* }}} int def_get_rrdargs */

/* vim: set sw=2 sts=2 et fdm=marker : */
