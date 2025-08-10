
#include <sqlite3.h>

#define BL_IMPL
#define BL_STRINGBUILDER_IMPL
#include "../bl.h"

#include "helpers.h"

char* get_artist_list(sqlite3 *db);
char* get_album_list(sqlite3 *db);
char* get_album_grid(sqlite3 *db);

int main() {
  int result;

  sqlite3 *db;
  result = sqlite3_open("../musikk.sqlite", &db);
  assert(!result, sqlite3_errmsg(db));

  char *template;
  result = read_file("templates/layout.html", &template);
  assert(result, "could not read template.layout.html");

  char* artists = get_artist_list(db);
  result = string_replace(template, "$artist_list", artists, &template);
  assert(result, "could not patch artists in template");

  char* albums = get_album_list(db);
  result = string_replace(template, "$album_list", albums, &template);
  assert(result, "could not patch albums in template");
  if (!result) return 0;

  char* album_grid = get_album_grid(db);
  result = string_replace(template, "$album_grid", album_grid, &template);
  assert(result, "could not patch album_grid in template");
  if (!result) return 0;


  template = concat("<!-- generated! do not modify -->\n\n", template);
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
    int result = read_file("templates/artists_entry.html", &artists_entry_template);
    assert(result, "could not read artist_entry.html");

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
        read_file("templates/albums_entry.html", &album_entry_template);
    assert(result, "could not read albums_entry.html");

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

char* get_album_grid(sqlite3 *db) {
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
