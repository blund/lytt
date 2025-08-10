#ifndef HELPERS_H
#define HELPERS_H

#define db_iterate(stmt) while (((rc = sqlite3_step(stmt)) == SQLITE_ROW))

int starts_with(const char *string, const char *test);
char* has_extension(const char *ext, const char *filename);

int ok(char *content, int size, char *mime, struct MHD_Connection *conn);

// Allocates result, must be freed
char *copy(const unsigned char *string);
#endif
