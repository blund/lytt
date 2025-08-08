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
int get_album(struct MHD_Connection *conn, const char *url, sqlite3* db);


enum MHD_Result play_song(struct MHD_Connection *conn, sqlite3 *db);
enum MHD_Result music_handler(struct MHD_Connection *conn, const char *url);

char *build_songs(int album_id, sqlite3* db);

sqlite3_stmt* query_by_id(char *query, int id, sqlite3* db);

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

  if (starts_with(url, "/static"))
    return get_static_file(conn, url);

  if (starts_with(url, "/album"))
    return get_album(conn, url, db);

  if (starts_with(url, "/artist"))
    return get_artist(conn, url, db);

  if (starts_with(url, "/play")) {
    return play_song(conn, db);
  }

  if (starts_with(url, "/musikk/")) {
    return music_handler(conn, url);
  }





  return get_layout(conn);
}

int get_static_file(struct MHD_Connection *conn, const char *url) {
  const char* filename = url + 1;

  char* file;
  int result = read_file(filename, &file);
  if (!result) {
    struct MHD_Response *response;
    int ret = MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);
    return ret;
  }

  char *mime = "text/plain";
  if      (has_extension(".css", filename)) mime = "text/css";
  else if (has_extension(".js", filename))  mime = "text/javascript";
  else assert(0, filename);

  return ok(file, mime, conn);
}

// Returns the main html file for the website
int get_layout(struct MHD_Connection *conn) {
  struct MHD_Response *response;
  int ret;

  char* page;
  int result = read_file("layout.html", &page);
  assert(result != 0, "could not read layout.html");

  return ok(page, "text/html", conn);
}

int get_artist(struct MHD_Connection *conn, const char *url, sqlite3 *db) {
  const char *id =
      MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "id");
  if (!id) return MHD_NO;

  char *content = concat("<p>", id, "</p>");
  return ok(content, "text/html", conn);
}

int get_album(struct MHD_Connection *conn, const char *url, sqlite3 *db) {
  const char *id =
      MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "id");
  if (!id) return MHD_NO;

  char *content = build_songs(atoi(id), db);
  return ok(content, "text/html", conn);
}

char *make_song(const unsigned char *song_name, int id) {
  StringBuilder *sb = new_builder(128);
  add_to(sb, "<a href=# fx-trigger=\"click\" fx-action=\"/play?song_id=%d\" fx-target=\"#player-source\" fx-swap=\"outerHTML\" >%s</a>", id, song_name);

  char* button = to_string(sb);
  free_builder(sb);
  return button;
}

char *build_songs(int album_id, sqlite3* db) {
    sqlite3_stmt *stmt;
    int rc;

    stmt = query_by_id("SELECT title FROM albums WHERE id = ?;", album_id, db);
    char* title = copy(sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);

    stmt = query_by_id("SELECT artists.name "
                  "FROM albums "
                  "JOIN artists ON albums.artist_id = artists.id "
                  "WHERE albums.id = ?;", album_id, db);
    char* artist = copy(sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);

    
    const char *sql =
        "SELECT id, title FROM songs WHERE album_id = ? ORDER BY title;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLite prepare failed: %s\n", sqlite3_errmsg(db));
        return (char*)-1;
    }

    // Bind the artist_id to the first placeholder
    rc = sqlite3_bind_int64(stmt, 1, album_id);
    if (rc != SQLITE_OK) {
      fprintf(stderr, "SQLite bind failed: %s\n", sqlite3_errmsg(db));
      sqlite3_finalize(stmt);
      return (char*)-1;
    }

    StringBuilder *sb = new_builder(1024);
    add_to(sb, "<div>\n");
    add_to(sb, "<h3>%s</h2>\n", artist);
    add_to(sb, "<h2>%s</h2>\n", title);
    add_to(sb, "</div>\n");
    add_to(sb, "<ul>\n");

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *name = sqlite3_column_text(stmt, 1);
        add_to(sb, "<li>%s</li>\n", make_song(name, id));
    }

    add_to(sb, "</ul>\n");
    sqlite3_finalize(stmt);

    char *album_list = to_string(sb);
    free_builder(sb);

    return album_list;
}

char *get_file_path_by_song_id(sqlite3 *db, sqlite3_int64 song_id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT path FROM files WHERE song_id = ? LIMIT 1;";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "prepare failed: %s\n", sqlite3_errmsg(db));
        return NULL;
    }

    sqlite3_bind_int64(stmt, 1, song_id);

    char *path = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *raw_path = sqlite3_column_text(stmt, 0);
        path = strdup((const char *)raw_path); // make a copy you can return
    }

    sqlite3_finalize(stmt);
    return path; // caller must free(path)
}

enum MHD_Result play_song(struct MHD_Connection *conn, sqlite3* db) {
// Check for /play
    const char *song_id_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "song_id");
    if (!song_id_str) {
      return MHD_NO;
    }

    sqlite3_int64 song_id = strtoll(song_id_str, NULL, 10);

    // Lookup file path by song_id
    char *filepath = get_file_path_by_song_id(db, song_id); // You already have this function
    if (!filepath) {
      return MHD_NO;
    }

    // Create HTML <source> tag to return
    char content[1024];
    snprintf(content, sizeof(content),
             "<source id=\"player-source\" src=\"/%s\" type=\"audio/ogg\">", filepath);
    free(filepath);

    return ok(content, "text/html", conn);
}

enum MHD_Result music_handler(struct MHD_Connection *conn, const char *url) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "../.%s", url);  // Note: still unsafe path handling

    int fd = open(filepath, O_RDONLY);
    if (fd == -1) return MHD_NO;

    struct stat st;
    if (fstat(fd, &st) == -1) {
      close(fd);
      return MHD_NO;
    }

    // Check for Range request
    const char *range_header = MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "Range");
    struct MHD_Response *response;
    int ret;

    if (range_header && strncmp(range_header, "bytes=", 6) == 0) {
      off_t start = atoll(range_header + 6);
      off_t length = st.st_size - start;

      response = MHD_create_response_from_fd_at_offset64(length, fd, start);
      MHD_add_response_header(response, "Content-Type", "audio/ogg");
      MHD_add_response_header(response, "Accept-Ranges", "bytes");

      char content_range[128];
      snprintf(content_range, sizeof(content_range),
	       "bytes %lld-%lld/%lld",
	       (long long)start,
	       (long long)(st.st_size - 1),
	       (long long)st.st_size);
      MHD_add_response_header(response, "Content-Range", content_range);

      ret = MHD_queue_response(conn, MHD_HTTP_PARTIAL_CONTENT, response); // 206
    } else {
      response = MHD_create_response_from_fd_at_offset64(st.st_size, fd, 0);
      MHD_add_response_header(response, "Content-Type", "audio/ogg");
      MHD_add_response_header(response, "Accept-Ranges", "bytes");
      ret = MHD_queue_response(conn, MHD_HTTP_OK, response); // 200
    }

    MHD_destroy_response(response);
    return ret;
}

sqlite3_stmt* query_by_id(char *query, int id, sqlite3* db) {
  int rc;

  sqlite3_stmt *stmt;
  rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQLite prepare failed: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  rc = sqlite3_bind_int64(stmt, 1, id);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQLite bind failed: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return 0;
  }

  return stmt;
}
