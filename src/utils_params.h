#ifndef UTILS_PARAMS_H
#define UTILS_PARAMS_H 1

#include <time.h>

int param_init (void);
void param_finish (void);

const char *param (const char *key);

int uri_escape (char *dst, const char *src, size_t size);

const char *script_name (void);

int time_to_rfc1123 (time_t t, char *buffer, size_t buffer_size);

#endif /* UTILS_PARAMS_H */
