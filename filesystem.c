#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "filesystem.h"

struct fs_scan_dir_data_s /* {{{ */
{
  fs_ident_cb_t callback;
  void *user_data;

  char *host;
  char *plugin;
  char *plugin_instance;
  char *type;
  char *type_instance;
}; /* }}} */
typedef struct fs_scan_dir_data_s fs_scan_dir_data_t;

typedef int (*callback_type_t)   (const char *type,   void *user_data);
typedef int (*callback_plugin_t) (const char *plugin, void *user_data);
typedef int (*callback_host_t)   (const char *host,   void *user_data);

/*
 * Directory and file walking functions
 */
static int foreach_rrd_file (const char *dir, /* {{{ */
    int (*callback) (const char *, void *),
    void *user_data)
{
  DIR *dh;
  struct dirent *entry;
  int status;

  if (callback == NULL)
    return (EINVAL);

  dh = opendir (dir);
  if (dh == NULL)
    return (errno);

  while ((entry = readdir (dh)) != NULL)
  {
    struct stat statbuf;
    char abspath[PATH_MAX + 1];
    size_t d_name_len;

    if (entry->d_name[0] == '.')
      continue;

    d_name_len = strlen (entry->d_name);
    if (d_name_len <= 4)
      continue;

    if (strcasecmp (".rrd", entry->d_name + (d_name_len - 4)) != 0)
      continue;

    snprintf (abspath, sizeof (abspath), "%s/%s", dir, entry->d_name);
    abspath[sizeof (abspath) - 1] = 0;

    memset (&statbuf, 0, sizeof (statbuf));

    status = stat (abspath, &statbuf);
    if (status != 0)
      continue;

    if (!S_ISREG (statbuf.st_mode))
      continue;

    entry->d_name[d_name_len - 4] = 0;

    status = (*callback) (entry->d_name, user_data);
    if (status != 0)
      break;
  } /* while (readdir) */

  closedir (dh);
  return (status);
} /* }}} int foreach_rrd_file */

static int foreach_dir (const char *dir, /* {{{ */
    int (*callback) (const char *, void *),
    void *user_data)
{
  DIR *dh;
  struct dirent *entry;
  int status = 0;

  if (callback == NULL)
    return (EINVAL);

  dh = opendir (dir);
  if (dh == NULL)
    return (errno);

  while ((entry = readdir (dh)) != NULL)
  {
    struct stat statbuf;
    char abspath[PATH_MAX + 1];

    if (entry->d_name[0] == '.')
      continue;

    snprintf (abspath, sizeof (abspath), "%s/%s", dir, entry->d_name);
    abspath[sizeof (abspath) - 1] = 0;

    memset (&statbuf, 0, sizeof (statbuf));

    status = stat (abspath, &statbuf);
    if (status != 0)
      continue;

    if (!S_ISDIR (statbuf.st_mode))
      continue;

    status = (*callback) (entry->d_name, user_data);
    if (status != 0)
      break;
  } /* while (readdir) */

  closedir (dh);
  return (status);
} /* }}} int foreach_dir */

static int foreach_type (const char *host, const char *plugin, /* {{{ */
    callback_type_t callback, void *user_data)
{
  char abspath[PATH_MAX + 1];

  if ((host == NULL) || (plugin == NULL))
    return (EINVAL);

  snprintf (abspath, sizeof (abspath), "%s/%s/%s", DATA_DIR, host, plugin);
  abspath[sizeof (abspath) - 1] = 0;

  return (foreach_rrd_file (abspath, callback, user_data));
} /* }}} int foreach_type */

static int foreach_plugin (const char *host, /* {{{ */
    callback_plugin_t callback,
    void *user_data)
{
  char abspath[PATH_MAX + 1];

  if (host == NULL)
    return (EINVAL);

  snprintf (abspath, sizeof (abspath), "%s/%s", DATA_DIR, host);
  abspath[sizeof (abspath) - 1] = 0;

  return (foreach_dir (abspath, callback, user_data));
} /* }}} int foreach_plugin */

static int foreach_host (callback_host_t callback, /* {{{ */
    void *user_data)
{
  return (foreach_dir (DATA_DIR, callback, user_data));
} /* }}} int foreach_host */

/*
 * Functions building "fs_scan_dir_data_t" and calling the user-supplied
 * callback eventually.
 */
static int scan_type (const char *type, void *user_data) /* {{{ */
{
  fs_scan_dir_data_t *data = user_data;
  graph_ident_t *ident;
  int status;

  if ((type == NULL) || (data == NULL))
    return (EINVAL);

  if ((data->type != NULL) || (data->type_instance != NULL))
    return (EINVAL);

  data->type = strdup (type);
  if (data->type == NULL)
    return (ENOMEM);

  data->type_instance = strchr (data->type, '-');
  if (data->type_instance != NULL)
  {
    *data->type_instance = 0;
    data->type_instance++;
  }
  else
  {
    data->type_instance = data->type + strlen (data->type);
  }

  ident = ident_create (data->host,
      data->plugin, data->plugin_instance,
      data->type, data->type_instance);
  if (ident == NULL)
  {
    status = -1;
  }
  else
  {
    status = (*data->callback) (ident, data->user_data);
    ident_destroy (ident);
  }

  free (data->type);
  data->type = NULL;
  data->type_instance = NULL;

  return (status);
} /* }}} int scan_type */

static int scan_plugin (const char *plugin, void *user_data) /* {{{ */
{
  fs_scan_dir_data_t *data = user_data;
  int status;

  if ((plugin == NULL) || (data == NULL))
    return (EINVAL);

  if ((data->plugin != NULL) || (data->plugin_instance != NULL))
    return (EINVAL);

  data->plugin = strdup (plugin);
  if (data->plugin == NULL)
    return (ENOMEM);

  data->plugin_instance = strchr (data->plugin, '-');
  if (data->plugin_instance != NULL)
  {
    *data->plugin_instance = 0;
    data->plugin_instance++;
  }
  else
  {
    data->plugin_instance = data->plugin + strlen (data->plugin);
  }

  status = foreach_type (data->host, plugin, scan_type, data);

  free (data->plugin);
  data->plugin = NULL;
  data->plugin_instance = NULL;

  return (status);
} /* }}} int scan_plugin */

static int scan_host (const char *host, void *user_data) /* {{{ */
{
  fs_scan_dir_data_t *data = user_data;
  int status;

  if ((host == NULL) || (data == NULL))
    return (EINVAL);

  if (data->host != NULL)
    return (EINVAL);

  data->host = strdup (host);
  if (data->host == NULL)
    return (ENOMEM);

  status =  foreach_plugin (host, scan_plugin, data);

  free (data->host);
  data->host = NULL;

  return (status);
} /* }}} int scan_host */

/*
 * Public function
 */
int fs_scan (fs_ident_cb_t callback, void *user_data) /* {{{ */
{
  fs_scan_dir_data_t data;

  memset (&data, 0, sizeof (data));
  data.callback = callback;
  data.user_data = user_data;

  data.host = NULL;
  data.plugin = NULL;
  data.plugin_instance = NULL;
  data.type = NULL;
  data.type_instance = NULL;

  foreach_host (scan_host, &data);

  return (0);
} /* }}} int fs_scan */

/* vim: set sw=2 sts=2 et fdm=marker : */
