#ifndef BL_H
#define BL_H

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>


#ifdef DEBUG
#define dprintf(fmt, args...) fprintf(stderr, fmt, ## args)
#else
#define dprintf(fmt, args...)
#endif

#define bl_assert(expr)                                                        \
  ((expr) ? ((void)0)                                                          \
          : _release_assert(#expr, __FILE__, __LINE__,                         \
                            __extension__ __PRETTY_FUNCTION__))
#undef assert
#define assert bl_assert

void _release_assert(const char *assertionExpr,
		     const char *assertionFile,
		     unsigned int assertionLine,
		     const char* assertionFunction);


typedef struct StringBuilder {
  char* buffer;
  int   capacity;
  int   index;
} StringBuilder;

// Forward declare functions
StringBuilder* new_builder(int capacity);
void add_to(StringBuilder* b, char* format, ...);
void reset(StringBuilder* b);
void free_builder(StringBuilder* b);
char* to_string(StringBuilder* b);


int read_file(const char* filename, char** data);
void _release_assert(const char *assertionExpr, const char *assertionFile,
                    unsigned int assertionLine, const char *assertionFunction);
float random_float(float min, float max);

#define array_size(x) sizeof((x))/sizeof((x)[0])
#define for_in(x, y) for (int x = 0; x < array_size(y); x++)
#define for_to(x, y) for (int x = 0; x < y; x++)

#define fori(x) for (int i = 0; i < x; i++)
#define forj(x) for (int j = 0; j < x; j++)
#define forx(name, x) for (int name = 0; name < x; name++)

#ifdef BL_IMPL

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>



void _release_assert(const char *assertionExpr,
                    const char *assertionFile,
                    unsigned int assertionLine,
                    const char* assertionFunction) {
  fprintf(stderr, "%s:%u: %s: Assertion `%s' failed.\n", assertionFile, assertionLine, assertionFunction, assertionExpr);
  abort();
}

int read_file(const char* filename, char** out) {
    size_t size;
    char* data = 0;

    struct stat st;
    if (stat(filename, &st) != 0)
    {
        fprintf(stderr, "Failed to access file '%s' for file size, errno = %d (%s)\n",
                filename, errno, strerror(errno));
        abort();
    }
    size = (size_t)(st.st_size);

    data = (char *)malloc(sizeof(char) * size + 1);

    // Make sure we terminate this scary C string :)
    data[size] = '\0';

    FILE* fp = fopen(filename, "r");
    if (fp == 0)
    {
        fprintf(stderr, "Failed to open file '%s' for read, errno = %d (%s)\n",
                filename, errno, strerror(errno));
        abort();
    }

    size_t bytesRead = fread(data, sizeof(char), size, fp);
    assert(bytesRead == size);

    {
        // just to sanity check we have reached eof
        uint8_t _c;
        assert(fread(&_c, sizeof(uint8_t), 1 /*numElements*/, fp) == 0);
        assert(feof(fp));
    }

    fclose(fp);

    *out = data;
    return size;
}

float random_float(float min, float max) {
    float scale = rand() / (float) RAND_MAX; /* [0, 1.0] */
    return min + scale * ( max - min );      /* [min, max] */
}

// Declare functions if ..._IMPLEMENTATION is defined in source file
#endif

#ifdef BL_STRINGBUILDER_IMPL
StringBuilder* new_builder(int capacity) {
  StringBuilder* b = (StringBuilder*)malloc(sizeof(StringBuilder));
  b->buffer   = (char*)malloc(capacity); b->capacity = capacity;
  b->index    = 0;

  return b;
}

void free_builder(StringBuilder* b) {
  //free(b->buffer);
  free(b);
}

void add_to(StringBuilder* b, char* format, ...) {
  // Try writing to the buffer
  // If expected written characters exceed the buffer capacity, grow it and try again
  for (;;) {
    char* position = b->buffer + b->index;
    int   remaining_capacity = b->capacity - b->index;

    va_list args;
    va_start(args, format);
    int written = vsnprintf(position, remaining_capacity, format, args);
    va_end(args);

    if (written >= remaining_capacity) {
      b->capacity *= 2;
      b->buffer = (char*)realloc(b->buffer, b->capacity);
    } else {
      b->index += written;
      break;
    }
  }
}

void reset(StringBuilder* b) {
  b->index = 0;
}

char* to_string(StringBuilder* b) {
  b->buffer[b->index] = 0;
  return b->buffer;
}

#define NUM_ARGS_IMPL(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N
#define NUM_ARGS(...) NUM_ARGS_IMPL(__VA_ARGS__,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)

#define concat(...) concat_multi(NUM_ARGS(__VA_ARGS__), __VA_ARGS__)
char *concat_multi(int count, ...) {
  StringBuilder *sb = new_builder(64);

  va_list args;
  va_start(args, count);
  for (int i = 0; i < count; ++i) {
    char* arg = va_arg(args, char*);
    add_to(sb, arg);
  }
  va_end(args);

  char *str = to_string(sb);
  free_builder(sb);
  return str;
}

#endif
#endif
