#ifndef UTILS_CGI_H
#define UTILS_CGI_H 1

#include <time.h>

typedef int (*page_callback_t) (void *user_data);

struct page_callbacks_s
{
  page_callback_t top_left;
  page_callback_t top_center;
  page_callback_t top_right;
  page_callback_t middle_left;
  page_callback_t middle_center;
  page_callback_t middle_right;
  page_callback_t bottom_left;
  page_callback_t bottom_center;
  page_callback_t bottom_right;
};
typedef struct page_callbacks_s page_callbacks_t;

struct param_list_s;
typedef struct param_list_s param_list_t;

#define PAGE_CALLBACKS_INIT \
{ NULL, NULL, NULL, \
  NULL, NULL, NULL, \
  NULL, NULL, NULL }

int param_init (void);
void param_finish (void);

const char *param (const char *key);

/* Create a new parameter list from "query_string". If "query_string" is NULL,
 * the "QUERY_STRING" will be used. */
param_list_t *param_create (const char *query_string);
param_list_t *param_clone (param_list_t *pl);
void param_destroy (param_list_t *pl);
const char *param_get (param_list_t *pl, const char *name);
int param_set (param_list_t *pl,
    const char *name, const char *value);
char *param_as_string (param_list_t *pl);
int param_print_hidden (param_list_t *pl);

char *uri_escape (const char *string);
char *uri_escape_buffer (char *buffer, size_t buffer_size);
char *uri_escape_copy (char *dest, const char *src, size_t n);

const char *script_name (void);

int time_to_rfc1123 (time_t t, char *buffer, size_t buffer_size);

char *html_escape (const char *string);
char *html_escape_buffer (char *buffer, size_t buffer_size);
char *html_escape_copy (char *dest, const char *src, size_t n);

int html_print_page (const char *title,
    const page_callbacks_t *cb, void *user_data);

int html_print_logo (void *user_data);

int html_print_search_box (void *user_data);

/* vim: set sw=2 sts=2 et fdm=marker : */
#endif /* UTILS_CGI_H */
