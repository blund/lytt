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

char* after_swap = "<script type=\"text/javascript\" src=\"/static/reload.js\"></script>\n";
char* fixi =       "<script type=\"text/javascript\" src=\"/static/fixi.js\"></script>\n";

char *build_audio_player(char* path) {
  StringBuilder *sb = new_builder(1024);

  add_to(sb, "<audio id=\"audio-player\" controls>\n");
  add_to(sb, "<source id=\"player-source\">\n");
  add_to(sb, "your browser does not support the audio element.\n");
  add_to(sb, "</audio>\n");

  char *play = to_string(sb);
  free_builder(sb);
	 
  return play;
}
sqlite3_int64 get_song_id_by_title(sqlite3 *db, const char *title) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id FROM songs WHERE title = ? LIMIT 1;";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);

    sqlite3_int64 song_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        song_id = sqlite3_column_int64(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return song_id;
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

enum MHD_Result song_select(struct MHD_Connection *conn, sqlite3* db) {
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
    char response_buf[1024];
    snprintf(response_buf, sizeof(response_buf),
             "<source id=\"player-source\" src=\"/%s\" type=\"audio/ogg\">", filepath);
    free(filepath);

    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(response_buf), response_buf, MHD_RESPMEM_MUST_COPY);
    MHD_add_response_header(response, "Content-Type", "text/html");

    int ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
}

enum MHD_Result file_handler(struct MHD_Connection *conn, const char *url) {
    const char *filename = url + 8;  // Skip "/static/"

    // Only allow .js files for security
    if (!strstr(filename, ".js")) {
        return MHD_NO;
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "static/%s", filename);  // Adjust base dir

    printf("%s\n", filepath);

    FILE *fp = fopen(filepath, "rb");
    if (!fp) return MHD_NO;

    // Get file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char *buffer = malloc(size);
    if (!buffer) {
        fclose(fp);
        return MHD_NO;
    }

    fread(buffer, 1, size, fp);
    fclose(fp);

    struct MHD_Response *res = MHD_create_response_from_buffer(size, buffer, MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(res, "Content-Type", "application/javascript");

    int ret = MHD_queue_response(conn, MHD_HTTP_OK, res);
    MHD_destroy_response(res);
    return ret;
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

char *make_button(char *song_name, int id) {
  StringBuilder *sb = new_builder(128);
  add_to(sb, "<a href=# fx-trigger=\"click\" fx-action=\"/play?song_id=%d\" fx-target=\"#player-source\" fx-swap=\"outerHTML\">%s</a>", id, song_name);

  char* button = to_string(sb);
  free_builder(sb);
  return button;
}

enum MHD_Result handler(void *cls, struct MHD_Connection *conn, const char *url,
                        const char *method, const char *version,
                        const char *upload_data, size_t *upload_data_size,
                        void **con_cls) {
  sqlite3 *db = (sqlite3 *)cls;

  if (strncmp(url, "/static/", 8) == 0) {
    return file_handler(conn, url);
  }
  if (strncmp(url, "/musikk/", 8) == 0) {
    return music_handler(conn, url);
  }

  if (strcmp(url, "/play") == 0) {
    return song_select(conn, db);
  }
 
  char* t1 = "Holyfields,";
  sqlite3_int64 s1 = get_song_id_by_title(db, t1);
  char *p1 = get_file_path_by_song_id(db, s1);

  char* t2 = "1/21";
  sqlite3_int64 s2 = get_song_id_by_title(db, t2);
  char *p2 = get_file_path_by_song_id(db, s2);


  char *page = concat(make_button(t1,s1), make_button(t2, s2), build_audio_player(p2), fixi, after_swap);
  
  struct MHD_Response *response = MHD_create_response_from_buffer(
      strlen(page), (void *)page, MHD_RESPMEM_PERSISTENT);

  MHD_add_response_header(response, "Content-Type", "text/html");
  int ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return ret;
}

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

