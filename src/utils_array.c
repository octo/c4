#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "utils_array.h"

struct str_array_s
{
  char **ptr;
  size_t size;
};

str_array_t *array_create (void) /* {{{ */
{
  str_array_t *a;

  a = malloc (sizeof (*a));
  if (a == NULL)
    return (NULL);

  memset (a, 0, sizeof (*a));
  a->ptr = NULL;
  a->size = 0;

  return (a);
} /* }}} str_array_t *array_create */

void array_destroy (str_array_t *a) /* {{{ */
{
  if (a == NULL)
    return;

  free (a->ptr);
  a->ptr = NULL;
  a->size = 0;

  free (a);
} /* }}} void array_destroy */

int array_append (str_array_t *a, const char *entry) /* {{{ */
{
  char **ptr;

  if ((entry == NULL) || (a == NULL))
    return (EINVAL);

  ptr = realloc (a->ptr, sizeof (*a->ptr) * (a->size + 1));
  if (ptr == NULL)
    return (ENOMEM);
  a->ptr = ptr;
  ptr = a->ptr + a->size;

  *ptr = strdup (entry);
  if (*ptr == NULL)
    return (ENOMEM);

  a->size++;
  return (0);
} /* }}} int array_append */

int array_append_format (str_array_t *a, const char *format, ...) /* {{{ */
{
  char buffer[1024];
  va_list ap;
  int status;

  va_start (ap, format);
  status = vsnprintf (buffer, sizeof (buffer), format, ap);
  va_end(ap);

  if ((status < 0) || (((size_t) status) >= sizeof (buffer)))
    return (ENOMEM);

  return (array_append (a, buffer));
} /* }}} int array_append_format */

int array_argc (str_array_t *a) /* {{{ */
{
  if (a == NULL)
    return (-1);

  return ((int) a->size);
} /* }}} int array_argc */

char **array_argv (str_array_t *a) /* {{{ */
{
  if ((a == NULL) || (a->size == 0))
    return (NULL);

  return (a->ptr);
} /* }}} char **array_argv */

/* vim: set sw=2 sts=2 et fdm=marker : */
