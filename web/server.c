#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>

#include <microhttpd.h>
#include <sqlite3.h>

#define BL_IMPL
#define BL_STRINGBUILDER_IMPL
#include "../bl.h"


const int port = 8888;

enum MHD_Result handler(void *cls, struct MHD_Connection *conn, const char *url,
                        const char *method, const char *version,
                        const char *upload_data, size_t *upload_data_size,
                        void **con_cls);

int get_stylesheet(struct MHD_Connection *conn);
int get_layout(struct MHD_Connection *conn);

int running = 1;
void handle_signal(int sig) {
    running = 0;
}

int main() {
  sqlite3 *db;
  int result;

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  // Open the music database
  result = sqlite3_open("../musikk.sqlite", &db);
  assert(result == SQLITE_OK, sqlite3_errmsg(db));

  // Start the server
  struct MHD_Daemon *daemon;
  daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD, port,
			    NULL, NULL, &handler, db,
			    MHD_OPTION_END);
  assert(daemon != NULL, "failed to start http daemon");

  // Pause this thread until killed
  while (running) {
    pause();
  }

  // End daemon on end
  MHD_stop_daemon(daemon);
  return 0;
}



enum MHD_Result handler(void *cls, struct MHD_Connection *conn, const char *url,
                        const char *method, const char *version,
                        const char *upload_data, size_t *upload_data_size,
                        void **con_cls) {
  sqlite3 *db = (sqlite3 *)cls;

  if (strcmp(url, "/style.css") == 0) {
    return get_stylesheet(conn);
  }

  return get_layout(conn);
}

// Returns the stylesheet for the website
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

// Returns the main html file for the website
int get_layout(struct MHD_Connection *conn) {
  struct MHD_Response *response;
  int ret;

  char* page;
  int result = read_file("layout.html", &page);
  assert(result != 0, "could not read layout.html");
  response = MHD_create_response_from_buffer(
      strlen(page), (void *)page, MHD_RESPMEM_PERSISTENT);

  MHD_add_response_header(response, "Content-Type", "text/html");
  ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return ret;
}
