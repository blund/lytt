#include <string.h>
#include <stdlib.h>
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
int get_album_cover(struct MHD_Connection *conn, const char *url, sqlite3 *db);


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

  if (starts_with(url, "/album-cover"))
    return get_album_cover(conn, url, db);

  if (starts_with(url, "/album"))
    return get_album(conn, url, db);

  if (starts_with(url, "/artist"))
    return get_artist(conn, url, db);

  if (starts_with(url, "/play"))
    return play_song(conn, db);

  if (starts_with(url, "/musikk/"))
    return music_handler(conn, url);

  return get_layout(conn);
}

int get_static_file(struct MHD_Connection *conn, const char *url) {
  const char* filename = url + 1;

  char* file;
  int size = read_file(filename, &file);
  if (!size) {
    struct MHD_Response *response;
    int ret = MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);
    return ret;
  }

  char *mime = "text/plain";
  if      (has_extension(".css", filename)) mime = "text/css";
  else if (has_extension(".js", filename))  mime = "text/javascript";
  else assert(0, filename);

  return ok(file, size, mime, conn, PERSIST);
}

// Returns the main html file for the website
int get_layout(struct MHD_Connection *conn) {
  struct MHD_Response *response;
  int ret;

  char* page;
  int size = read_file("layout.html", &page);
  assert(size != 0, "could not read layout.html");

  return ok(page, size, "text/html", conn, FREE);
}

int get_artist(struct MHD_Connection *conn, const char *url, sqlite3 *db) {
  const char *id =
      MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "id");
  if (!id) return MHD_NO;

  char *content = concat("<p>", id, "</p>");
  return ok(content, strlen(content), "text/html", conn, FREE);
}



int get_album(struct MHD_Connection *conn, const char *url, sqlite3 *db) {
  const char *id =
      MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "id");
  if (!id) return MHD_NO;

  char *content = build_songs(atoi(id), db);
  return ok(content, strlen(content), "text/html", conn, FREE);
}

int get_album_cover(struct MHD_Connection *conn, const char *url, sqlite3 *db) {
  const char *id_str =
      MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "id");
  if (!id_str) return MHD_NO;

  int id = atoi(id_str);
  
  sqlite3_stmt *stmt =
      query_by_id("SELECT cover FROM albums WHERE id = ?", id, db);
  char *path = copy(sqlite3_column_text(stmt, 0));
  sqlite3_finalize(stmt);

  char *file;
  int size = read_file(concat("../", path), &file);
  if (!size)
    return MHD_NO;

  char *mime = "image/jpeg";
  if (has_extension(".png", path)) {
    mime = "image/png";
  }

  return ok(file, size, mime, conn, FREE);
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

    stmt = query_by_id("SELECT albums.title, artists.name "
                       "FROM albums "
		       "JOIN artists ON albums.artist_id = artists.id "
                       "WHERE albums.id = ?;", album_id, db);
    char* album_title = copy(sqlite3_column_text(stmt, 0));
    char* artist = copy(sqlite3_column_text(stmt, 1));
    sqlite3_finalize(stmt);
   
    StringBuilder *sb = new_builder(1024);
    add_to(sb, "<div class=\"album-info\">\n");
    add_to(sb, "<img src=\"/album-cover?id=%d\" alt=\"Album Cover\">", album_id);
    add_to(sb, "<div><h3>%s</h2>\n", artist);
    add_to(sb, "<h2>%s</h2></div>\n", album_title);
    add_to(sb, "</div>\n");
    add_to(sb, "<ul>\n");

    stmt = query_by_id(
        "SELECT id, title FROM songs WHERE album_id = ? ORDER BY track_number",
        album_id, db);
    if (!stmt) return "";

    do {
      int id = sqlite3_column_int(stmt, 0);
      const unsigned char *name = sqlite3_column_text(stmt, 1);
      add_to(sb, "<li>%s</li>\n", make_song(name, id));
    } db_iterate(stmt);

    add_to(sb, "</ul>\n");
    sqlite3_finalize(stmt);

    char *album_list = to_string(sb);

    free(album_title);
    free(artist);

    free_builder(sb);
    return album_list;
}

enum MHD_Result play_song(struct MHD_Connection *conn, sqlite3* db) {
    const char *song_id_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "song_id");
    if (!song_id_str) return MHD_NO;

    sqlite3_int64 song_id = strtoll(song_id_str, NULL, 10);

    sqlite3_stmt *stmt =
        query_by_id("SELECT files.path, songs.title, artists.name, albums.id "
                    "FROM songs "
                    "JOIN albums ON songs.album_id = albums.id "
                    "JOIN artists ON songs.artist_id = artists.id "
                    "JOIN files ON songs.id = files.song_id "
                    "WHERE songs.id = ?",
                    song_id, db);

    char* path   = copy(sqlite3_column_text(stmt, 0));
    char* title  = copy(sqlite3_column_text(stmt, 1));
    char* artist = copy(sqlite3_column_text(stmt, 2));
    int cover    = sqlite3_column_int64(stmt, 3);
    sqlite3_finalize(stmt);

    char* song_template;
    int result =
        read_file("templates/template.song_entry.html", &song_template);
    assert(result, "could not read template.albums_entry.html");

    // Create HTML <source> tag to return
    char content[1024];
    snprintf(content, sizeof(content),
	       song_template, cover, artist, title, path);
    
    free(song_template);
    free(path);
    free(title);
    free(artist);

    return ok(content, strlen(content), "text/html", conn, PERSIST);
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
