
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
  result = read_file("templates/template.layout.html", &template);
  assert(result, "could not read template.layout.html");

  char* artists = get_artist_list(db);
  result = string_replace(template, "$artists", artists, &template);
  assert(result, "could not patch artists in template");

  printf("%s\n", template);
  
  char* albums = get_album_list(db);
  result = string_replace(template, "$albums", albums, &template);
  assert(result, "could not patch albums in template");
  if (!result) return 0;

  printf("%s\n", template);

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

    char* artists_entry_template;
    int result = read_file("templates/template.artists_entry.html", &artists_entry_template);
    assert(result, "could not read template.artist_entry.html");

    StringBuilder *sb = new_builder(1024);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      sqlite3_int64 artist_id = sqlite3_column_int64(stmt, 0);
      const unsigned char *name = sqlite3_column_text(stmt, 1);
      add_to(sb, artists_entry_template, artist_id, name);
    }
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

    char* album_entry_template;
    int result =
        read_file("templates/template.albums_entry.html", &album_entry_template);
    assert(result, "could not read template.albums_entry.html");

    StringBuilder *sb = new_builder(1024);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      sqlite3_int64 album_id = sqlite3_column_int64(stmt, 0);
      const unsigned char *name = sqlite3_column_text(stmt, 1);
      add_to(sb, album_entry_template, album_id, name);
    }
    sqlite3_finalize(stmt);
    char *album_list = to_string(sb);

    free_builder(sb);
    
    return album_list;
}
