
#include <sqlite3.h>

#define BL_IMPL
#define BL_STRINGBUILDER_IMPL
#include "../bl.h"

char* get_artist_list(sqlite3 *db);
char* get_album_list(sqlite3 *db);

int main() {
  int result;

  sqlite3 *db;
  result = sqlite3_open("../musikk.sqlite", &db);
  assert(!result, sqlite3_errmsg(db));

  char *template;
  result = read_file("template.layout.html", &template);
  assert(result, "could not read layout.html");

  char* artists = get_artist_list(db);
  result = string_replace(&template, "$artists", artists);
  assert(result, "could not patch artists in template");

  char* albums = get_album_list(db);
  result = string_replace(&template, "$albums", albums);
  assert(result, "could not patch albums in template");
  if (!result) return 0;

  template = concat("<!-- generated! do not modify -->", template);
  write_file("layout.html", template);
}

char* get_artist_list(sqlite3 *db) {
    const char *sql = "SELECT id, name FROM artists ORDER BY name;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLite prepare failed: %s\n", sqlite3_errmsg(db));
        return (char*)-1;
    }

    StringBuilder *sb = new_builder(1024);
    add_to(sb, "<ul>\n");
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      sqlite3_int64 artist_id = sqlite3_column_int64(stmt, 0);
      const unsigned char *name = sqlite3_column_text(stmt, 1);
      add_to(sb, "<li><a href=\"/artist?id=%lld\">%s</a></li>\n", artist_id, name);

    }
    add_to(sb, "</ul>\n");
    sqlite3_finalize(stmt);
    char *artist_list = to_string(sb);

    free_builder(sb);
    
    return artist_list;
}


char* get_album_list(sqlite3 *db) {
    const char *sql = "SELECT id, title FROM albums ORDER BY title;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLite prepare failed: %s\n", sqlite3_errmsg(db));
        return (char*)-1;
    }

    StringBuilder *sb = new_builder(1024);

    add_to(sb, "<ul>\n");
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      sqlite3_int64 album_id = sqlite3_column_int64(stmt, 0);
      const unsigned char *name = sqlite3_column_text(stmt, 1);
      add_to(sb, "<li><a href=\"/artist?id=%lld\">%s</a></li>\n", album_id, name);
    }
    add_to(sb, "</ul>\n");
    sqlite3_finalize(stmt);
    char *album_list = to_string(sb);

    free_builder(sb);
    
    return album_list;
}
