#include <string.h>

int starts_with(const char *string, const char *test) {
  return strncmp(string, test, strlen(test)) == 0;
}

char* has_extension(const char *ext, const char *filename) {
  return strstr(filename, ext);
}
