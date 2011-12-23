#ifndef __BUFFER_H

#define __BUFFER_H

#include <stddef.h>

struct _buffer;

typedef struct _buffer buffer_t;

typedef void (*consumer_func)(void *data, size_t len, void *args);

buffer_t    *buffer_create(size_t size);
int          buffer_destroy(buffer_t *buf);
int          buffer_write(buffer_t *buf, void *data, size_t len);
size_t       buffer_write_from_fd(buffer_t *buf, int fd, size_t len);
size_t       buffer_read_to(buffer_t *buf, size_t len, void *target, size_t capacity);
size_t       buffer_read_to_fd(buffer_t *buf, size_t len, int to_fd);
size_t       buffer_consume(buffer_t *buf, size_t len, consumer_func cb, void *args);

#endif /* end of include guard: __BUFFER_H */
