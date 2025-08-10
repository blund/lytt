#include <string.h>
#include <malloc.h>

#include <microhttpd.h>

int starts_with(const char *string, const char *test) {
  return strncmp(string, test, strlen(test)) == 0;
}

char* has_extension(const char *ext, const char *filename) {
  return strstr(filename, ext);
}

int ok(char *content, int size, char *mime, struct MHD_Connection *conn) {
  struct MHD_Response *response = MHD_create_response_from_buffer(
      size, content, MHD_RESPMEM_MUST_COPY);

  MHD_add_response_header(response, "Content-Type", mime);
  int ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return ret;
}

char *copy(const unsigned char *string) {
  char* dest = malloc(strlen((char*)string) + 1);
  strcpy(dest, (char*)string);

  return dest;
}
