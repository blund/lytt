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
int get_library_grid(struct MHD_Connection *conn, const char *url, sqlite3* db);
int get_artist(struct MHD_Connection *conn, const char *url, sqlite3* db);
int get_album(struct MHD_Connection *conn, const char *url, sqlite3* db);
int get_album_cover(struct MHD_Connection *conn, const char *url, sqlite3 *db);


enum MHD_Result play_song(struct MHD_Connection *conn, sqlite3 *db);
enum MHD_Result stream_handler(struct MHD_Connection *conn, const char *url, sqlite3* db);

char *build_song_list(int album_id, sqlite3* db);
char* build_library_grid(sqlite3 *db);

sqlite3_stmt* query_by_id(char *query, int id, sqlite3* db);

int running = 1;
void handle_signal(int sig) {
  running = 0;
}

int main(int argc, char *argv[]) {
  // Check for incorrect args
  if (argc != 2) return -1;
  char* db_path = argv[1];

  sqlite3 *db;
  int result;

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  // Open the music database
  result = sqlite3_open(db_path, &db);
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

  if (starts_with(url, "/library"))
    return get_library_grid(conn, url, db);

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

  if (starts_with(url, "/stream"))
    return stream_handler(conn, url, db);

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

// Returns the main html file for the website
int get_library_grid(struct MHD_Connection *conn, const char *url, sqlite3* db) {
  struct MHD_Response *response;
  int ret;

  char* library_grid = build_library_grid(db);
  return ok(library_grid, strlen(library_grid), "text/html", conn, FREE);
}

char* get_album_grid(int artist_id, sqlite3 *db) {
  sqlite3_stmt *stmt;
  int rc;

  char* album_grid;
  int size =
    read_file("templates/album_grid.html", &album_grid);
  assert(size, "could not read album_grid.html");

  char* album_grid_entry;
  size =
    read_file("templates/album_grid_entry.html", &album_grid_entry);
  assert(size, "could not read album_grid_entry.html");

  stmt = query_by_id("SELECT albums.id, albums.title, artists.name "
		     "FROM albums "
		     "JOIN artists ON albums.artist_id = artists.id "
		     "WHERE albums.artist_id = ? "
		     "ORDER BY albums.title", artist_id, db);

  StringBuilder *sb = new_builder(1024);
  do {
    sqlite3_int64 album_id = sqlite3_column_int64(stmt, 0);
    const char *album_title = (char*)sqlite3_column_text(stmt, 1);
    const char *artist_name = (char*)sqlite3_column_text(stmt, 2);

    char *entry = copy(album_grid_entry);

    char album_id_str[8];
    snprintf(album_id_str, 8, "%lld", album_id);

    string_replace(entry, "$album_id", album_id_str,  &entry);
    string_replace(entry, "$album_title", album_title, &entry);
    string_replace(entry, "$artist_name", artist_name, &entry);

    add_to(sb, entry);
  }
  db_iterate(stmt);

  sqlite3_finalize(stmt);
  char *album_grid_entries = to_string(sb);

  string_replace(album_grid, "$album_grid_entries", album_grid_entries, &album_grid);
  
  free_builder(sb);
  return album_grid;
}

int get_artist(struct MHD_Connection *conn, const char *url, sqlite3 *db) {
  const char *id =
    MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "id");
  if (!id) return MHD_NO;


  int artist_id = atoi(id);
  char* album_grid = get_album_grid(artist_id, db);
  
  return ok(album_grid, strlen(album_grid), "text/html", conn, FREE);
}



int get_album(struct MHD_Connection *conn, const char *url, sqlite3 *db) {
  const char *id =
    MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "id");
  if (!id) return MHD_NO;

  sqlite3_stmt *stmt;
  int rc;

  int album_id = atoi(id);
  stmt = query_by_id("SELECT albums.title, artists.name "
                     "FROM albums "
                     "JOIN artists ON albums.artist_id = artists.id "
                     "WHERE albums.id = ?;",
                     album_id, db);

  char* album_title = copy(sqlite3_column_text(stmt, 0));
  char* artist = copy(sqlite3_column_text(stmt, 1));
  sqlite3_finalize(stmt);

  char *album_overview;
  int size =
    read_file("templates/album_overview.html", &album_overview);
  if (size < 0) return MHD_NO;

  char* song_list = build_song_list(album_id, db);
  string_replace(album_overview, "$album_id", id, &album_overview);
  string_replace(album_overview, "$artist", artist, &album_overview);
  string_replace(album_overview, "$album_title", album_title, &album_overview);
  string_replace(album_overview, "$song_list", song_list, &album_overview);

  free(album_title);
  free(artist);
  free(song_list);

  return ok(album_overview, strlen(album_overview), "text/html", conn, FREE);
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
  int size = read_file(path, &file);
  if (!size) return MHD_NO;

  char *mime = "image/jpeg";
  if (has_extension(".png", path)) {
    mime = "image/png";
  }

  return ok(file, size, mime, conn, FREE);
}

char *make_song(const unsigned char *song_name, int id) {
  static char *song_template;

  int size = read_file("templates/song.html", &song_template);
  assert(size, "failed to read song.html");

  char id_str[32];
  snprintf(id_str, 32, "%d", id);
  string_replace(song_template, "$song_id", id_str, &song_template);
  string_replace(song_template, "$song_name", (char *)song_name, &song_template);

  return song_template;
}

char *build_song_list(int album_id, sqlite3* db) {
  sqlite3_stmt *stmt;
  int rc;

  stmt = query_by_id("SELECT id, title "
		     "FROM songs "
		     "WHERE album_id = ? "
		     "ORDER BY track_number",
		     album_id, db);

  if (!stmt) return "";

  StringBuilder *sb = new_builder(1024);
  do {
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *name = sqlite3_column_text(stmt, 1);
    add_to(sb, "%s\n", make_song(name, id));
  } db_iterate(stmt);
  sqlite3_finalize(stmt);

  char *song_list = to_string(sb);

  free_builder(sb);
  return song_list;
}

enum MHD_Result play_song(struct MHD_Connection *conn, sqlite3* db) {
  const char *song_id_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "song_id");
  if (!song_id_str) return MHD_NO;

  sqlite3_int64 song_id = strtoll(song_id_str, NULL, 10);

  sqlite3_stmt *stmt =
      query_by_id("SELECT files.id, songs.title, artists.name, albums.id "
                  "FROM songs "
                  "JOIN albums ON songs.album_id = albums.id "
                  "JOIN artists ON songs.artist_id = artists.id "
                  "JOIN files ON songs.id = files.song_id "
                  "WHERE songs.id = ?"
		  "AND files.extension = 'ogg'",
		song_id, db);

  int   file_id  = sqlite3_column_int64(stmt, 0);
  char* song_title    = copy(sqlite3_column_text(stmt, 1));
  char* artist_name   = copy(sqlite3_column_text(stmt, 2));
  int   album_id = sqlite3_column_int64(stmt, 3);
  sqlite3_finalize(stmt);

  char* song_template;
  int result =
    read_file("templates/song_source.html", &song_template);
  assert(result, "could not read song_source.html");

  char album_id_str[8];
  snprintf(album_id_str, 8, "%d", album_id);

  char file_id_str[8];
  snprintf(file_id_str, 8, "%d", file_id);

  string_replace(song_template, "$album_id", album_id_str, &song_template);
  string_replace(song_template, "$artist_name", artist_name, &song_template);
  string_replace(song_template, "$song_title", song_title, &song_template);
  string_replace(song_template, "$file_id", file_id_str, &song_template);
  
  free(song_title);
  free(artist_name);

  return ok(song_template, strlen(song_template), "text/html", conn, FREE);
}

enum MHD_Result stream_handler(struct MHD_Connection *conn, const char *url, sqlite3* db) {
  const char *file_id_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "id");
  if (!file_id_str) return MHD_NO;

  sqlite3_stmt *stmt;
  stmt = query_by_id("SELECT path FROM files WHERE id = ?;",atoi(file_id_str), db);
  char* file_path    = copy(sqlite3_column_text(stmt, 0));
  sqlite3_finalize(stmt);
  
  int fd = open(file_path, O_RDONLY);
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

char* build_library_grid(sqlite3 *db) {
  const char *sql = "SELECT albums.id, albums.title, artists.name "
                    "FROM albums "
                    "JOIN artists ON albums.artist_id = artists.id "
                    "ORDER BY albums.title ";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLite prepare failed: %s\n", sqlite3_errmsg(db));
        return (char*)-1;
    }

    char* album_grid;
    int size =
      read_file("templates/album_grid.html", &album_grid);
    assert(size, "could not read album_grid.html");

    char* album_grid_entry;
    size =
        read_file("templates/album_grid_entry.html", &album_grid_entry);
    assert(size, "could not read album_grid_entry.html");

    StringBuilder *sb = new_builder(1024);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      sqlite3_int64 album_id = sqlite3_column_int64(stmt, 0);
      const char *album_title = (char*)sqlite3_column_text(stmt, 1);
      const char *artist_name = (char*)sqlite3_column_text(stmt, 2);

      char *entry = copy(album_grid_entry);

      char album_id_str[8];
      snprintf(album_id_str, 8, "%lld", album_id);

      string_replace(entry, "$album_id", album_id_str,  &entry);
      string_replace(entry, "$album_title", album_title, &entry);
      string_replace(entry, "$artist_name", artist_name, &entry);

      add_to(sb, entry);
    }
    sqlite3_finalize(stmt);
    char *album_grid_entries = to_string(sb);

    string_replace(album_grid, "$album_grid_entries", album_grid_entries, &album_grid);
    free_builder(sb);
    
    return album_grid;
}
