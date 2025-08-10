#ifndef HELPERS_H
#define HELPERS_H

#include <microhttpd.h>

#define db_iterate(stmt) while (((rc = sqlite3_step(stmt)) == SQLITE_ROW))

int starts_with(const char *string, const char *test);
char* has_extension(const char *ext, const char *filename);

typedef enum {
  PERSIST = MHD_RESPMEM_PERSISTENT,
  FREE = MHD_RESPMEM_MUST_FREE,
} response_mode;
int ok(char *content, int size, char *mime, struct MHD_Connection *conn,
       response_mode mode);

// Allocates result, must be freed
char *copy(const unsigned char *string);
#endif
