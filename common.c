#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <assert.h>

#include "common.h"

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

int foreach_type (const char *host, const char *plugin, /* {{{ */
    callback_type_t callback, void *user_data)
{
  char abspath[PATH_MAX + 1];

  if ((host == NULL) || (plugin == NULL))
    return (EINVAL);

  snprintf (abspath, sizeof (abspath), "%s/%s/%s", DATA_DIR, host, plugin);
  abspath[sizeof (abspath) - 1] = 0;

  return (foreach_rrd_file (abspath, callback, user_data));
} /* }}} int foreach_type */

int foreach_plugin (const char *host, /* {{{ */
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

int foreach_host (callback_host_t callback, /* {{{ */
    void *user_data)
{
  return (foreach_dir (DATA_DIR, callback, user_data));
} /* }}} int foreach_host */

size_t c_strlcat (char *dst, const char *src, size_t size) /* {{{ */
{
  size_t retval;
  size_t dst_len;
  size_t src_len;

  dst_len = strlen (dst);
  src_len = strlen (src);
  retval = dst_len + src_len;

  if ((dst_len + 1) >= size)
    return (retval);

  dst  += dst_len;
  size -= dst_len;
  assert (size >= 2);

  /* Result will be truncated. */
  if (src_len >= size)
    src_len = size - 1;

  memcpy (dst, src, src_len);
  dst[src_len] = 0;

  return (retval);
} /* }}} size_t c_strlcat */



/* vim: set sw=2 sts=2 et fdm=marker : */
