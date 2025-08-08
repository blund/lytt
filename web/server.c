#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>

#include <microhttpd.h>
#include <sqlite3.h>

#define BL_IMPL
#define BL_STRINGBUILDER_IMPL
#include "../bl.h"


#define PORT 8888

enum MHD_Result handler(void *cls, struct MHD_Connection *conn, const char *url,
                        const char *method, const char *version,
                        const char *upload_data, size_t *upload_data_size,
                        void **con_cls);


int get_stylesheet(struct MHD_Connection *conn);


int main() {
  sqlite3 *db;
  if (sqlite3_open("../musikk.sqlite", &db)) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  struct MHD_Daemon *daemon;
  daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD, PORT,
			    NULL, NULL, &handler, db,
			    MHD_OPTION_END);

  if (daemon == NULL)
    return 1;

  getchar(); // keep running until Enter is pressed
  MHD_stop_daemon(daemon);
  return 0;
}



enum MHD_Result handler(void *cls, struct MHD_Connection *conn, const char *url,
                        const char *method, const char *version,
                        const char *upload_data, size_t *upload_data_size,
                        void **con_cls) {
  sqlite3 *db = (sqlite3 *)cls;
  struct MHD_Response *response;
  int ret;

  char* page;
  int result = read_file("layout.html", &page);
  assert(result != 0, "could not read layout.html");

  if (strcmp(url, "/style.css") == 0) {
    return get_stylesheet(conn);
  }
  
  response = MHD_create_response_from_buffer(
      strlen(page), (void *)page, MHD_RESPMEM_PERSISTENT);

  MHD_add_response_header(response, "Content-Type", "text/html");
  ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return ret;
}


int get_stylesheet(struct MHD_Connection *conn) {
  struct MHD_Response *response;
  int ret;

  char* file;
  int result = read_file("style.css", &file);
  if (!result) {
    ret = MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, response);
    return ret;
  }
  int len = strlen(file);
  response = MHD_create_response_from_buffer(len, file, MHD_RESPMEM_MUST_FREE);
  MHD_add_response_header(response, "Content-Type", "text/css");
  ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}
