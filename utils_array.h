#ifndef UTILS_ARRAY_H
#define UTILS_ARRAY_H 1

struct str_array_s;
typedef struct str_array_s str_array_t;

str_array_t *array_create (void);
void array_destroy (str_array_t *a);
int array_append (str_array_t *a, const char *entry);
int array_append_format (str_array_t *a, const char *format, ...)
  __attribute__((format(printf,2,3)));

int array_argc (str_array_t *);
char **array_argv (str_array_t *);

#endif /* UTILS_ARRAY_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
