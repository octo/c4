#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

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

int uri_escape (char *dst, const char *src, size_t size) /* {{{ */
{
  size_t in;
  size_t out;

  in = 0;
  out = 0;
  while (42)
  {
    if (src[in] == 0)
    {
      dst[out] = 0;
      return (0);
    }
    else if ((src[in] < 32)
        || (src[in] == '&')
        || (src[in] == ';')
        || (((unsigned char) src[in]) >= 128))
    {
      char esc[4];

      if ((size - out) < 4)
        break;
      
      snprintf (esc, sizeof (esc), "%%%02x", (unsigned int) src[in]);
      dst[out] = esc[0];
      dst[out+1] = esc[1];
      dst[out+2] = esc[2];

      out += 3;
      in++;
    }
    else
    {
      dst[out] = src[in];
      out++;
      in++;
    }
  } /* while (42) */

  return (0);
} /* }}} int uri_escape */

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

  printf ("Content-Type: text/html\n\n");

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

int html_print_search_box (__attribute__((unused)) void *user_data) /* {{{ */
{
  char *term_html;

  term_html = html_escape (param ("search"));

  printf ("<form action=\"%s\" method=\"get\">\n"
      "  <input type=\"hidden\" name=\"action\" value=\"list_graphs\" />\n"
      "  <input type=\"text\" name=\"search\" value=\"%s\" id=\"search-input\" />\n"
      "  <input type=\"submit\" name=\"button\" value=\"Search\" />\n"
      "</form>\n",
      script_name (),
      (term_html != NULL) ? term_html : "");

  free (term_html);

  return (0);
} /* }}} int html_print_search_box */

/* vim: set sw=2 sts=2 et fdm=marker : */