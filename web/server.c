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

#include "helpers.h"

const int port = 8888;

enum MHD_Result handler(void *cls, struct MHD_Connection *conn, const char *url,
                        const char *method, const char *version,
                        const char *upload_data, size_t *upload_data_size,
                        void **con_cls);

int get_static_file(struct MHD_Connection *conn, const char* url);
int get_layout(struct MHD_Connection *conn);
int get_artist(struct MHD_Connection *conn, const char *url, sqlite3* db);

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

  if (starts_with(url, "/static")) {
    return get_static_file(conn, url);
  }

  if (starts_with(url, "/artist")) {
    puts("bam");
    return get_artist(conn, url, db);
  }

  return get_layout(conn);
}

int get_static_file(struct MHD_Connection *conn, const char *url) {
  const char* filename = url + 1;

  struct MHD_Response *response;
  int ret;

  char* file;
  int result = read_file(filename, &file);
  if (!result) {
    ret = MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, response);
    return ret;
  }

  int len = strlen(file);
  response = MHD_create_response_from_buffer(len, file, MHD_RESPMEM_MUST_FREE);

  char *mime = "text/plain";
  if (has_extension(".css", filename)) {
    mime = "text/css";
  }
  else if (has_extension(".js", filename)) {
    mime = "text/javascript";
  }
  else {
    assert(0, filename);
  }

  MHD_add_response_header(response, "Content-Type", mime);
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

int get_artist(struct MHD_Connection *conn, const char *url, sqlite3 *db) {
  const char *id =
      MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "id");
  if (!id) return MHD_NO;

  char* response_text = concat("<p>", id, "</p>");
  struct MHD_Response *response = MHD_create_response_from_buffer(
      strlen(response_text), response_text, MHD_RESPMEM_MUST_COPY);

  MHD_add_response_header(response, "Content-Type", "text/html");

  int ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return ret;
}
