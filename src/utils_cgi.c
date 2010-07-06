/**
 * collection4 - utils_cgi.c
 * Copyright (C) 2010  Florian octo Forster
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors:
 *   Florian octo Forster <ff at octo.it>
 **/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include "utils_cgi.h"
#include "common.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

struct parameter_s
{
  char *key;
  char *value;
};
typedef struct parameter_s parameter_t;

struct param_list_s
{
  parameter_t *parameters;
  size_t parameters_num;
};

static param_list_t *pl_global = NULL;

static char *uri_unescape_copy (char *dest, const char *src, size_t n) /* {{{ */
{
  const char *src_ptr;
  char *dest_ptr;

  if ((dest == NULL) || (src == NULL) || (n < 1))
    return (NULL);

  src_ptr = src;
  dest_ptr = dest;

  *dest_ptr = 0;

  while (*src_ptr != 0)
  {
    if (n < 2)
      break;

    if (*src_ptr == '+')
    {
      *dest_ptr = ' ';
    }
    else if ((src_ptr[0] == '%')
        && isxdigit ((int) src_ptr[1]) && isxdigit ((int) src_ptr[2]))
    {
      char tmpstr[3];
      char *endptr;
      long value;

      tmpstr[0] = src_ptr[1];
      tmpstr[1] = src_ptr[2];
      tmpstr[2] = 0;

      errno = 0;
      endptr = NULL;
      value = strtol (tmpstr, &endptr, /* base = */ 16);
      if ((endptr == tmpstr) || (errno != 0))
      {
        *dest_ptr = '?';
      }
      else
      {
        *dest_ptr = (char) value;
      }

      src_ptr += 2;
    }
    else
    {
      *dest_ptr = *src_ptr;
    }

    src_ptr++;
    dest_ptr++;
    *dest_ptr = 0;
  } /* while (*src_ptr != 0) */

  assert (*dest_ptr == 0);
  return (dest);
} /* }}} char *uri_unescape */

static char *uri_unescape (const char *string) /* {{{ */
{
  char buffer[4096];

  if (string == NULL)
    return (NULL);

  uri_unescape_copy (buffer, string, sizeof (buffer));
  
  return (strdup (buffer));
} /* }}} char *uri_unescape */

static int param_parse_keyval (param_list_t *pl, char *keyval) /* {{{ */
{
  char *key_raw;
  char *value_raw;
  char *key;
  char *value;

  key_raw = keyval;
  value_raw = strchr (key_raw, '=');
  if (value_raw == NULL)
    return (EINVAL);
  *value_raw = 0;
  value_raw++;

  key = uri_unescape (key_raw);
  if (key == NULL)
    return (ENOMEM);

  value = uri_unescape (value_raw);
  if (value == NULL)
  {
    free (key);
    return (ENOMEM);
  }
  
  param_set (pl, key, value);

  free (key);
  free (value);

  return (0);
} /* }}} int param_parse_keyval */

static int parse_query_string (param_list_t *pl, /* {{{ */
    char *query_string)
{
  char *dummy;
  char *keyval;

  if ((pl == NULL) || (query_string == NULL))
    return (EINVAL);

  dummy = query_string;
  while ((keyval = strtok (dummy, ";&")) != NULL)
  {
    dummy = NULL;
    param_parse_keyval (pl, keyval);
  }

  return (0);
} /* }}} int parse_query_string */

int param_init (void) /* {{{ */
{
  if (pl_global != NULL)
    return (0);

  pl_global = param_create (/* query string = */ NULL);
  if (pl_global == NULL)
    return (ENOMEM);

  return (0);
} /* }}} int param_init */

void param_finish (void) /* {{{ */
{
  param_destroy (pl_global);
  pl_global = NULL;
} /* }}} void param_finish */

const char *param (const char *key) /* {{{ */
{
  param_init ();

  return (param_get (pl_global, key));
} /* }}} const char *param */

param_list_t *param_create (const char *query_string) /* {{{ */
{
  char *tmp;
  param_list_t *pl;

  if (query_string == NULL)
    query_string = getenv ("QUERY_STRING");

  if (query_string == NULL)
    return (NULL);

  tmp = strdup (query_string);
  if (tmp == NULL)
    return (NULL);
  
  pl = malloc (sizeof (*pl));
  if (pl == NULL)
  {
    free (tmp);
    return (NULL);
  }
  memset (pl, 0, sizeof (*pl));

  parse_query_string (pl, tmp);

  free (tmp);
  return (pl);
} /* }}} param_list_t *param_create */

  param_list_t *param_clone (__attribute__((unused)) param_list_t *pl) /* {{{ */
{
  /* FIXME: To be implemented. */
  assert (23 == 42);
  return (NULL);
} /* }}} param_list_t *param_clone */

void param_destroy (param_list_t *pl) /* {{{ */
{
  size_t i;

  if (pl == NULL)
    return;

  for (i = 0; i < pl->parameters_num; i++)
  {
    free (pl->parameters[i].key);
    free (pl->parameters[i].value);
  }
  free (pl->parameters);
  free (pl);
} /* }}} void param_destroy */

const char *param_get (param_list_t *pl, const char *name) /* {{{ */
{
  size_t i;

  if ((pl == NULL) || (name == NULL))
    return (NULL);

  for (i = 0; i < pl->parameters_num; i++)
  {
    if ((name == NULL) && (pl->parameters[i].key == NULL))
      return (pl->parameters[i].value);
    else if ((name != NULL) && (pl->parameters[i].key != NULL)
        && (strcmp (name, pl->parameters[i].key) == 0))
      return (pl->parameters[i].value);
  }

  return (NULL);
} /* }}} char *param_get */

static int param_add (param_list_t *pl, /* {{{ */
    const char *key, const char *value)
{
  parameter_t *tmp;

  tmp = realloc (pl->parameters,
      sizeof (*pl->parameters) * (pl->parameters_num + 1));
  if (tmp == NULL)
    return (ENOMEM);
  pl->parameters = tmp;
  tmp = pl->parameters + pl->parameters_num;

  memset (tmp, 0, sizeof (*tmp));
  tmp->key = strdup (key);
  if (tmp->key == NULL)
    return (ENOMEM);

  tmp->value = strdup (value);
  if (tmp->value == NULL)
  {
    free (tmp->key);
    return (ENOMEM);
  }

  pl->parameters_num++;

  return (0);
} /* }}} int param_add */

static int param_delete (param_list_t *pl, /* {{{ */
    const char *name)
{
  size_t i;

  if ((pl == NULL) || (name == NULL))
    return (EINVAL);

  for (i = 0; i < pl->parameters_num; i++)
    if (strcasecmp (pl->parameters[i].key, name) == 0)
      break;

  if (i >= pl->parameters_num)
    return (ENOENT);

  if (i < (pl->parameters_num - 1))
  {
    parameter_t p;

    p = pl->parameters[i];
    pl->parameters[i] = pl->parameters[pl->parameters_num - 1];
    pl->parameters[pl->parameters_num - 1] = p;
  }

  pl->parameters_num--;
  free (pl->parameters[pl->parameters_num].key);
  free (pl->parameters[pl->parameters_num].value);

  return (0);
} /* }}} int param_delete */

int param_set (param_list_t *pl, const char *name, /* {{{ */
    const char *value)
{
  parameter_t *p;
  char *value_copy;
  size_t i;

  if ((pl == NULL) || (name == NULL))
    return (EINVAL);

  if (value == NULL)
    return (param_delete (pl, name));

  p = NULL;
  for (i = 0; i < pl->parameters_num; i++)
  {
    if (strcasecmp (pl->parameters[i].key, name) == 0)
    {
      p = pl->parameters + i;
      break;
    }
  }

  if (p == NULL)
    return (param_add (pl, name, value));

  value_copy = strdup (value);
  if (value_copy == NULL)
    return (ENOMEM);

  free (p->value);
  p->value = value_copy;

  return (0);
} /* }}} int param_set */

char *param_as_string (param_list_t *pl) /* {{{ */
{
  char buffer[4096];
  char key[2048];
  char value[2048];
  size_t i;

  if (pl == NULL)
    return (NULL);

  buffer[0] = 0;
  for (i = 0; i < pl->parameters_num; i++)
  {
    uri_escape_copy (key, pl->parameters[i].key, sizeof (key));
    uri_escape_copy (value, pl->parameters[i].value, sizeof (value));

    if (i != 0)
      strlcat (buffer, ";", sizeof (buffer));
    strlcat (buffer, key, sizeof (buffer));
    strlcat (buffer, "=", sizeof (buffer));
    strlcat (buffer, value, sizeof (buffer));
  }

  return (strdup (buffer));
} /* }}} char *param_as_string */

int param_print_hidden (param_list_t *pl) /* {{{ */
{
  char key[2048];
  char value[2048];
  size_t i;

  if (pl == NULL)
    return (EINVAL);

  for (i = 0; i < pl->parameters_num; i++)
  {
    html_escape_copy (key, pl->parameters[i].key, sizeof (key));
    html_escape_copy (value, pl->parameters[i].value, sizeof (value));

    printf ("  <input type=\"hidden\" name=\"%s\" value=\"%s\" />\n",
        key, value);
  }

  return (0);
} /* }}} int param_print_hidden */

char *uri_escape_copy (char *dest, const char *src, size_t n) /* {{{ */
{
  size_t in;
  size_t out;

  in = 0;
  out = 0;
  while (42)
  {
    if (src[in] == 0)
    {
      dest[out] = 0;
      return (dest);
    }
    else if ((((unsigned char) src[in]) < 32)
        || (src[in] == '&')
        || (src[in] == ';')
        || (src[in] == '?')
        || (src[in] == '/')
        || (((unsigned char) src[in]) >= 128))
    {
      char esc[4];

      if ((n - out) < 4)
        break;
      
      snprintf (esc, sizeof (esc), "%%%02x", (unsigned int) src[in]);
      dest[out] = esc[0];
      dest[out+1] = esc[1];
      dest[out+2] = esc[2];

      out += 3;
      in++;
    }
    else
    {
      dest[out] = src[in];
      out++;
      in++;
    }
  } /* while (42) */

  return (dest);
} /* }}} char *uri_escape_copy */

char *uri_escape (const char *string) /* {{{ */
{
  char buffer[4096];

  if (string == NULL)
    return (NULL);

  uri_escape_copy (buffer, string, sizeof (buffer));

  return (strdup (buffer));
} /* }}} char *uri_escape */

const char *script_name (void) /* {{{ */
{
  char *ret;

  ret = getenv ("SCRIPT_NAME");
  if (ret == NULL)
    ret = "collection4.fcgi";

  return (ret);
} /* }}} char *script_name */

int time_to_rfc1123 (time_t t, char *buffer, size_t buffer_size) /* {{{ */
{
  struct tm tm_tmp;
  size_t status;

  /* RFC 1123 *requires* the time to be GMT and the "GMT" timezone string.
   * Apache will ignore the timezone if "localtime_r" and "%z" is used,
   * resulting in weird behavior. */
  if (gmtime_r (&t, &tm_tmp) == NULL)
    return (errno);

  status = strftime (buffer, buffer_size, "%a, %d %b %Y %T GMT", &tm_tmp);
  if (status == 0)
    return (errno);

  return (0);
} /* }}} int time_to_rfc1123 */

#define COPY_ENTITY(e) do {    \
  size_t len = strlen (e);     \
  if (dest_size < (len + 1)) \
    break;                     \
  strcpy (dest_ptr, (e));    \
  dest_ptr += len;           \
  dest_size -= len;          \
} while (0)

char *html_escape_copy (char *dest, const char *src, size_t n) /* {{{ */
{
  char *dest_ptr;
  size_t dest_size;
  size_t pos;

  dest[0] = 0;
  dest_ptr = dest;
  dest_size = n;
  for (pos = 0; src[pos] != 0; pos++)
  {
    if (src[pos] == '"')
      COPY_ENTITY ("&quot;");
    else if (src[pos] == '<')
      COPY_ENTITY ("&lt;");
    else if (src[pos] == '>')
      COPY_ENTITY ("&gt;");
    else if (src[pos] == '&')
      COPY_ENTITY ("&amp;");
    else
    {
      *dest_ptr = src[pos];
      dest_ptr++;
      dest_size--;
      *dest_ptr = 0;
    }

    if (dest_size <= 1)
      break;
  }

  return (dest);
} /* }}} char *html_escape_copy */

#undef COPY_ENTITY

char *html_escape_buffer (char *buffer, size_t buffer_size) /* {{{ */
{
  char tmp[buffer_size];

  html_escape_copy (tmp, buffer, sizeof (tmp));
  memcpy (buffer, tmp, buffer_size);

  return (buffer);
} /* }}} char *html_escape_buffer */

char *html_escape (const char *string) /* {{{ */
{
  char buffer[4096];

  if (string == NULL)
    return (NULL);

  html_escape_copy (buffer, string, sizeof (buffer));

  return (strdup (buffer));
} /* }}} char *html_escape */

int html_print_page (const char *title, /* {{{ */
    const page_callbacks_t *cb, void *user_data)
{
  char *title_html;

  printf ("Content-Type: text/html\n"
      "X-Generator: "PACKAGE_STRING"\n"
      "\n\n");

  if (title == NULL)
    title = "c4: collection4 graph interface";

  title_html = html_escape (title);

  printf ("<html>\n"
      "  <head>\n"
      "    <title>%s</title>\n"
      "    <link rel=\"stylesheet\" type=\"text/css\" href=\"../share/style.css\" />\n"
      "    <script type=\"text/javascript\" src=\"../share/jquery-1.4.2.min.js\">\n"
      "    </script>\n"
      "    <script type=\"text/javascript\" src=\"../share/collection.js\">\n"
      "    </script>\n"
      "  </head>\n",
      title_html);

  printf ("  <body>\n"
      "    <table id=\"layout-table\">\n"
      "      <tr id=\"layout-top\">\n"
      "        <td id=\"layout-top-left\">");
  if (cb->top_left != NULL)
    (*cb->top_left) (user_data);
  else
    html_print_logo (NULL);
  printf ("</td>\n"
      "        <td id=\"layout-top-center\">");
  if (cb->top_center != NULL)
    (*cb->top_center) (user_data);
  else
    printf ("<h1>%s</h1>", title_html);
  printf ("</td>\n"
      "        <td id=\"layout-top-right\">");
  if (cb->top_right != NULL)
    (*cb->top_right) (user_data);
  printf ("</td>\n"
      "      </tr>\n"
      "      <tr id=\"layout-middle\">\n"
      "        <td id=\"layout-middle-left\">");
  if (cb->middle_left != NULL)
    (*cb->middle_left) (user_data);
  printf ("</td>\n"
      "        <td id=\"layout-middle-center\">");
  if (cb->middle_center != NULL)
    (*cb->middle_center) (user_data);
  printf ("</td>\n"
      "        <td id=\"layout-middle-right\">");
  if (cb->middle_right != NULL)
    (*cb->middle_right) (user_data);
  printf ("</td>\n"
      "      </tr>\n"
      "      <tr id=\"layout-bottom\">\n"
      "        <td id=\"layout-bottom-left\">");
  if (cb->bottom_left != NULL)
    (*cb->bottom_left) (user_data);
  printf ("</td>\n"
      "        <td id=\"layout-bottom-center\">");
  if (cb->bottom_center != NULL)
    (*cb->bottom_center) (user_data);
  printf ("</td>\n"
      "        <td id=\"layout-bottom-right\">");
  if (cb->bottom_right != NULL)
    (*cb->bottom_right) (user_data);
  printf ("</td>\n"
      "      </tr>\n"
      "    </table>\n"
      "  </body>\n"
      "</html>\n");

  free (title_html);
  return (0);
} /* }}} int html_print_page */

int html_print_logo (__attribute__((unused)) void *user_data) /* {{{ */
{
  printf ("<a href=\"%s?action=list_graphs\" id=\"logo-canvas\">\n"
      "  <h1>c<sup>4</sup></h1>\n"
      "  <div id=\"logo-subscript\">collection&nbsp;4</div>\n"
      "</a>\n");

  return (0);
} /* }}} int html_print_search_box */

int html_print_search_box (__attribute__((unused)) void *user_data) /* {{{ */
{
  char *term_html;

  term_html = html_escape (param ("q"));

  printf ("<form action=\"%s\" method=\"get\" id=\"search-form\">\n"
      "  <input type=\"hidden\" name=\"action\" value=\"list_graphs\" />\n"
      "  <input type=\"text\" name=\"q\" value=\"%s\" id=\"search-input\" />\n"
      "  <input type=\"submit\" name=\"button\" value=\"Search\" />\n"
      "</form>\n",
      script_name (),
      (term_html != NULL) ? term_html : "");

  free (term_html);

  return (0);
} /* }}} int html_print_search_box */

/* vim: set sw=2 sts=2 et fdm=marker : */
