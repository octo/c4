#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "utils_params.h"

struct parameter_s
{
  char *key;
  char *value;
};
typedef struct parameter_s parameter_t;

static parameter_t *parameters = NULL;
static size_t parameters_num = 0;
static _Bool parameters_init = 0;

static int parameter_add (const char *key, const char *value) /* {{{ */
{
  parameter_t *ptr;

  if (value == NULL)
    return (EINVAL);

  ptr = realloc (parameters, sizeof (*parameters) * (parameters_num + 1));
  if (ptr == NULL)
    return (ENOMEM);
  parameters = ptr;

  ptr = parameters + parameters_num;
  if (key == NULL)
  {
    ptr->key = NULL;
  }
  else
  {
    ptr->key = strdup (key);
    if (ptr->key == NULL)
      return (ENOMEM);
  }

  ptr->value = strdup (value);
  if (ptr->value == NULL)
  {
    free (ptr->key);
    return (ENOMEM);
  }

  parameters_num++;
  return (0);
} /* }}} int parameter_add */

static char *parameter_lookup (const char *key) /* {{{ */
{
  size_t i;

  for (i = 0; i < parameters_num; i++)
  {
    if ((key == NULL) && (parameters[i].key == NULL))
      return (parameters[i].value);
    else if ((key != NULL) && (parameters[i].key != NULL)
        && (strcmp (key, parameters[i].key) == 0))
      return (parameters[i].value);
  }

  return (NULL);
} /* }}} char *parameter_lookup */

static char *uri_unescape (char *string) /* {{{ */
{
  char *in;
  char *out;

  if (string == NULL)
    return (NULL);

  in = string;
  out = string;

  while (*in != 0)
  {
    if (*in == '+')
    {
      *out = ' ';
    }
    else if ((in[0] == '%')
        && isxdigit ((int) in[1]) && isxdigit ((int) in[2]))
    {
      char tmpstr[3];
      char *endptr;
      long value;

      tmpstr[0] = in[1];
      tmpstr[1] = in[2];
      tmpstr[2] = 0;

      errno = 0;
      endptr = NULL;
      value = strtol (tmpstr, &endptr, /* base = */ 16);
      if ((endptr == tmpstr) || (errno != 0))
      {
        *out = '?';
      }
      else
      {
        *out = (char) value;
      }

      in += 2;
    }
    else
    {
      *out = *in;
    }

    in++;
    out++;
  } /* while (*in != 0) */

  *out = 0;
  return (string);
} /* }}} char *uri_unescape */

static int parse_keyval (char *keyval) /* {{{ */
{
  char *key;
  char *val;

  val = strchr (keyval, '=');
  if (val == NULL)
  {
    key = NULL;
    val = keyval;
  }
  else
  {
    key = keyval;
    *val = 0;
    val++;
  }

  parameter_add (uri_unescape (key), uri_unescape (val));

  return (0);
} /* }}} int parse_keyval */

static int parse_query_string (char *query_string) /* {{{ */
{
  char *dummy;
  char *keyval;

  if (query_string == NULL)
    return (EINVAL);

  dummy = query_string;
  while ((keyval = strtok (dummy, ";&")) != NULL)
  {
    dummy = NULL;
    parse_keyval (keyval);
  }

  return (0);
} /* }}} int parse_query_string */

int param_init (void) /* {{{ */
{
  const char *query_string;
  char *copy;
  int status;

  if (parameters_init)
    return (0);

  query_string = getenv ("QUERY_STRING");
  if (query_string == NULL)
    return (ENOENT);

  copy = strdup (query_string);
  if (copy == NULL)
    return (ENOMEM);

  status = parse_query_string (copy);
  free (copy);

  parameters_init = 1;

  return (status);
} /* }}} int param_init */

void param_finish (void) /* {{{ */
{
  size_t i;

  if (!parameters_init)
    return;

  for (i = 0; i < parameters_num; i++)
  {
    free (parameters[i].key);
    free (parameters[i].value);
  }
  free (parameters);

  parameters = NULL;
  parameters_num = 0;
  parameters_init = 0;
} /* }}} void param_finish */

const char *param (const char *key) /* {{{ */
{
  param_init ();

  return (parameter_lookup (key));
} /* }}} const char *param */

/* vim: set sw=2 sts=2 et fdm=marker : */
