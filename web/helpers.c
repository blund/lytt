#include <string.h>

#include <microhttpd.h>

int starts_with(const char *string, const char *test) {
  return strncmp(string, test, strlen(test)) == 0;
}

char* has_extension(const char *ext, const char *filename) {
  return strstr(filename, ext);
}

int ok(char *content, char *mime, struct MHD_Connection *conn) {
  struct MHD_Response *response = MHD_create_response_from_buffer(
      strlen(content), content, MHD_RESPMEM_MUST_COPY);

  MHD_add_response_header(response, "Content-Type", mime);
  int ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return ret;

}
