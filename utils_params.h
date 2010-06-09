#ifndef UTILS_PARAMS_H
#define UTILS_PARAMS_H 1

int param_init (void);
void param_finish (void);

const char *param (const char *key);

int uri_escape (char *dst, const char *src, size_t size);

#endif /* UTILS_PARAMS_H */
