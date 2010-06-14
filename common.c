#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <assert.h>
#include <math.h>

#include <rrd.h>

#include "common.h"
#include "graph_list.h"

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

int ds_list_from_rrd_file (char *file, /* {{{ */
    size_t *ret_dses_num, char ***ret_dses)
{
  char *rrd_argv[] = { "info", file, NULL };
  int rrd_argc = (sizeof (rrd_argv) / sizeof (rrd_argv[0])) - 1;

  rrd_info_t *info;
  rrd_info_t *ptr;

  char **dses = NULL;
  size_t dses_num = 0;

  info = rrd_info (rrd_argc, rrd_argv);
  if (info == NULL)
  {
    printf ("%s: rrd_info (%s) failed.\n", __func__, file);
    return (-1);
  }

  for (ptr = info; ptr != NULL; ptr = ptr->next)
  {
    size_t keylen;
    size_t dslen;
    char *ds;
    char **tmp;

    if (strncmp ("ds[", ptr->key, strlen ("ds[")) != 0)
      continue;

    keylen = strlen (ptr->key);
    if (keylen < strlen ("ds[?].index"))
      continue;

    dslen = keylen - strlen ("ds[].index");
    assert (dslen >= 1);

    if (strcmp ("].index", ptr->key + (strlen ("ds[") + dslen)) != 0)
      continue;

    ds = malloc (dslen + 1);
    if (ds == NULL)
      continue;

    memcpy (ds, ptr->key + strlen ("ds["), dslen);
    ds[dslen] = 0;

    tmp = realloc (dses, sizeof (*dses) * (dses_num + 1));
    if (tmp == NULL)
    {
      free (ds);
      continue;
    }
    dses = tmp;

    dses[dses_num] = ds;
    dses_num++;
  }

  rrd_info_free (info);

  if (dses_num < 1)
  {
    assert (dses == NULL);
    return (ENOENT);
  }

  *ret_dses_num = dses_num;
  *ret_dses = dses;

  return (0);
} /* }}} int ds_list_from_rrd_file */

static int hsv_to_rgb (double *hsv, double *rgb) /* {{{ */
{
  double c = hsv[2] * hsv[1];
  double h = hsv[0] / 60.0;
  double x = c * (1.0 - fabs (fmod (h, 2.0) - 1));
  double m = hsv[2] - c;

  rgb[0] = 0.0;
  rgb[1] = 0.0;
  rgb[2] = 0.0;

       if ((0.0 <= h) && (h < 1.0)) { rgb[0] = 1.0; rgb[1] = x; rgb[2] = 0.0; }
  else if ((1.0 <= h) && (h < 2.0)) { rgb[0] = x; rgb[1] = 1.0; rgb[2] = 0.0; }
  else if ((2.0 <= h) && (h < 3.0)) { rgb[0] = 0.0; rgb[1] = 1.0; rgb[2] = x; }
  else if ((3.0 <= h) && (h < 4.0)) { rgb[0] = 0.0; rgb[1] = x; rgb[2] = 1.0; }
  else if ((4.0 <= h) && (h < 5.0)) { rgb[0] = x; rgb[1] = 0.0; rgb[2] = 1.0; }
  else if ((5.0 <= h) && (h < 6.0)) { rgb[0] = 1.0; rgb[1] = 0.0; rgb[2] = x; }

  rgb[0] += m;
  rgb[1] += m;
  rgb[2] += m;

  return (0);
} /* }}} int hsv_to_rgb */

static uint32_t rgb_to_uint32 (double *rgb) /* {{{ */
{
  uint8_t r;
  uint8_t g;
  uint8_t b;

  r = (uint8_t) (255.0 * rgb[0]);
  g = (uint8_t) (255.0 * rgb[1]);
  b = (uint8_t) (255.0 * rgb[2]);

  return ((((uint32_t) r) << 16)
      | (((uint32_t) g) << 8)
      | ((uint32_t) b));
} /* }}} uint32_t rgb_to_uint32 */

uint32_t get_random_color (void) /* {{{ */
{
  double hsv[3] = { 0.0, 1.0, 1.0 };
  double rgb[3] = { 0.0, 0.0, 0.0 };

  hsv[0] = 360.0 * ((double) rand ()) / (((double) RAND_MAX) + 1.0);

  hsv_to_rgb (hsv, rgb);

  return (rgb_to_uint32 (rgb));
} /* }}} uint32_t get_random_color */

/* vim: set sw=2 sts=2 et fdm=marker : */
