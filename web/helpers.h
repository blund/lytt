#ifndef HELPERS_H
#define HELPERS_H

int starts_with(const char *string, const char *test);
char* has_extension(const char *ext, const char *filename);

int ok(char *content, char *mime, struct MHD_Connection *conn);

#endif
