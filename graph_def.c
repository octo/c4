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

  char *ds_name;
  uint32_t color;

  graph_def_t *next;
};

/*
 * Private functions
 */

/*
 * Public functions
 */
graph_def_t *def_create (graph_config_t *cfg, graph_ident_t *ident, /* {{{ */
    const char *ds_name)
{
  graph_ident_t *selector;
  graph_def_t *ret;

  if ((cfg == NULL) || (ident == NULL) || (ds_name == NULL))
    return (NULL);

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

  ret->ds_name = strdup (ds_name);
  if (ret->ds_name == NULL)
  {
    ident_destroy (selector);
    free (ret);
    return (NULL);
  }

  ret->color = get_random_color ();
  ret->next = NULL;

  ret->select = ident_copy_with_selector (selector, ident,
      IDENT_FLAG_REPLACE_ALL);
  if (ret->select == NULL)
  {
    ident_destroy (selector);
    free (ret->ds_name);
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

  free (def->ds_name);

  free (def);

  def_destroy (next);
} /* }}} void def_destroy */

int def_append (graph_def_t *head, graph_def_t *def) /* {{{ */
{
  graph_def_t *ptr;

  if ((head == NULL) || (def == NULL))
    return (EINVAL);

  ptr = head;
  while (ptr->next != NULL)
    ptr = ptr->next;

  ptr->next = def;

  return (0);
} /* }}} int def_append */

graph_def_t *def_search (graph_def_t *head, graph_ident_t *ident, /* {{{ */
    const char *ds_name)
{
  graph_def_t *ptr;

  if ((head == NULL) || (ident == NULL) || (ds_name == NULL))
    return (NULL);

  for (ptr = head; ptr != NULL; ptr = ptr->next)
  {
    if (!ident_matches (ptr->select, ident))
      continue;

    if (strcmp (ptr->ds_name, ds_name) == 0)
      return (ptr);
  }

  return (NULL);
} /* }}} graph_def_t *def_search */

_Bool def_matches (graph_def_t *def, graph_ident_t *ident) /* {{{ */
{
  return (ident_matches (def->select, ident));
} /* }}} _Bool def_matches */

int def_foreach (graph_def_t *def, def_callback_t callback, /* {{{ */
    void *user_data)
{
  graph_def_t *ptr;

  if ((def == NULL) || (callback == NULL))
    return (EINVAL);

  for (ptr = def; ptr != NULL; ptr = ptr->next)
  {
    int status;

    status = (*callback) (ptr, user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int def_foreach */

int def_get_rrdargs (graph_def_t *def, graph_ident_t *ident, /* {{{ */
    str_array_t *args)
{
  char *file;
  int index;

  if ((def == NULL) || (ident == NULL) || (args == NULL))
    return (EINVAL);

  file = ident_to_file (ident);
  if (file == NULL)
  {
    DEBUG ("gl_ident_get_rrdargs: ident_to_file returned NULL.\n");
    return (-1);
  }

  DEBUG ("gl_ident_get_rrdargs: file = %s;\n", file);

  index = array_argc (args);

  /* CDEFs */
  array_append_format (args, "DEF:def_%04i_min=%s:%s:MIN",
      index, file, def->ds_name);
  array_append_format (args, "DEF:def_%04i_avg=%s:%s:AVERAGE",
      index, file, def->ds_name);
  array_append_format (args, "DEF:def_%04i_max=%s:%s:MAX",
      index, file, def->ds_name);
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
      index, def->color, def->ds_name);
  array_append_format (args, "GPRINT:vdef_%04i_min:%%lg min,", index);
  array_append_format (args, "GPRINT:vdef_%04i_avg:%%lg avg,", index);
  array_append_format (args, "GPRINT:vdef_%04i_max:%%lg max,", index);
  array_append_format (args, "GPRINT:vdef_%04i_lst:%%lg last\\l", index);

  free (file);

  return (0);
} /* }}} int def_get_rrdargs */

/* vim: set sw=2 sts=2 et fdm=marker : */
