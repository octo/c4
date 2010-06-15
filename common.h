#ifndef COMMON_H
#define COMMON_H 1

#include <stdint.h>
#include <inttypes.h>

int print_debug (const char *format, ...)
  __attribute__((format(printf,1,2)));
#if 0
# define DEBUG(...) print_debug (__VA_ARGS__)
#else
# define DEBUG(...) /**/
#endif

size_t c_strlcat (char *dst, const char *src, size_t size);
#define strlcat c_strlcat

int ds_list_from_rrd_file (char *file,
    size_t *ret_dses_num, char ***ret_dses);

uint32_t get_random_color (void);

#endif /* COMMON_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
