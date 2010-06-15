#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "graph_config.h"
#include "graph_list.h"
#include "oconfig.h"
#include "common.h"

#define CONFIG_FILE "/usr/lib/cgi-bin/octo/collection.conf"

time_t last_read_mtime = 0;

static int dispatch_config (const oconfig_item_t *ci) /* {{{ */
{
  int i;

  for (i = 0; i < ci->children_num; i++)
  {
    oconfig_item_t *child;

    child = ci->children + i;
    if (strcasecmp ("Graph", child->key) == 0)
      graph_config_add (child);
    else
    {
      DEBUG ("Unknown config option: %s", child->key);
    }
  }

  return (0);
} /* }}} int dispatch_config */

static int internal_read_config (void) /* {{{ */
{
  oconfig_item_t *ci;

  ci = oconfig_parse_file (CONFIG_FILE);
  if (ci == NULL)
    return (-1);

  dispatch_config (ci);

  oconfig_free (ci);

  gl_config_submit ();

  return (0);
} /* }}} int internal_read_config */

static time_t get_config_mtime (void) /* {{{ */
{
  struct stat statbuf;
  int status;

  memset (&statbuf, 0, sizeof (statbuf));
  status = stat (CONFIG_FILE, &statbuf);
  if (status != 0)
    return (0);

  return (statbuf.st_mtime);
} /* }}} time_t get_config_mtime */

int graph_read_config (void) /* {{{ */
{
  time_t mtime;

  mtime = get_config_mtime ();

  if (mtime <= last_read_mtime)
    return (0);

  internal_read_config ();

  last_read_mtime = mtime;

  return (0);
} /* }}} int graph_read_config */

int graph_config_get_string (const oconfig_item_t *ci, /* {{{ */
    char **ret_str)
{
  char *tmp;

  if ((ci->values_num != 1) || (ci->values[0].type != OCONFIG_TYPE_STRING))
    return (EINVAL);

  tmp = strdup (ci->values[0].value.string);
  if (tmp == NULL)
    return (ENOMEM);

  free (*ret_str);
  *ret_str = tmp;

  return (0);
} /* }}} int graph_config_get_string */

int graph_config_get_bool (const oconfig_item_t *ci, /* {{{ */
    _Bool *ret_bool)
{
  if ((ci->values_num != 1) || (ci->values[0].type != OCONFIG_TYPE_BOOLEAN))
    return (EINVAL);

  if (ci->values[0].value.boolean)
    *ret_bool = 1;
  else
    *ret_bool = 0;

  return (0);
} /* }}} int graph_config_get_bool */

/* vim: set sw=2 sts=2 et fdm=marker : */
