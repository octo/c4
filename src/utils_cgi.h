#ifndef UTILS_CGI_H
#define UTILS_CGI_H 1

#include <time.h>

int param_init (void);
void param_finish (void);

const char *param (const char *key);

int uri_escape (char *dst, const char *src, size_t size);

const char *script_name (void);

int time_to_rfc1123 (time_t t, char *buffer, size_t buffer_size);

char *html_escape (const char *string);
char *html_escape_buffer (char *buffer, size_t buffer_size);
char *html_escape_copy (char *dest, const char *src, size_t n);

/* vim: set sw=2 sts=2 et fdm=marker : */
#endif /* UTILS_CGI_H */
