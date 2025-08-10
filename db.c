
#include <stdlib.h>
#include <stdio.h>

#include <sqlite3.h>


char *zErrMsg = 0;
int rc;

char* init_artists =
    "create table artists("
    "id   integer primary key,"
    "name text unique not null"
    ");";

char *init_albums =
  "create table albums("
  "id          integer primary key,"
  "title       text not null,"
  "cover       text not null,"
  "artist_id   integer," 
  "foreign key(artist_id) references artist(id)"
  ");";

char *init_songs = "create table songs("
  "id            integer primary key,"
  "title         text not null,"
  "track_number  integer,"
  "artist_id     integer,"
  "album_id      integer,"
  "foreign key(artist_id) references artist(id),"
  "foreign key(album_id)  references album(id),"
  "UNIQUE(title, artist_id, album_id)"
  ");";

char *init_files = "create table files("
  "id        integer primary key,"
  "path      text not null,"
  "extension text not null,"
  "song_id integer," 
  "foreign key(song_id) references songs(id)"
  ");";

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
   int i;
   for(i = 0; i<argc; i++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}

void init_db(sqlite3* db) {
  // sqlite3_exec(db, "PRAGMA foreign_keys = ON;", 0, 0, &zErrMsg);

  rc = sqlite3_exec(db, init_artists, callback, 0, &zErrMsg);
  if (rc) {
    fprintf(stderr, "Failed to add artist: %s\n", sqlite3_errmsg(db));
    return;
  }
  rc = sqlite3_exec(db, init_albums, callback, 0, &zErrMsg);
  if (rc) {
    fprintf(stderr, "Failed to add artist: %s\n", sqlite3_errmsg(db));
    return;
  }
  rc = sqlite3_exec(db, init_songs, callback, 0, &zErrMsg);
  if (rc) {
    fprintf(stderr, "Failed to add artist: %s\n", sqlite3_errmsg(db));
    return;
  }
  rc = sqlite3_exec(db, init_files, callback, 0, &zErrMsg);
  if (rc) {
    fprintf(stderr, "Failed to add artist: %s\n", sqlite3_errmsg(db));
    return;
  }
}

void check_sqlite(sqlite3* db, int rc, char* error) {
  if (rc != SQLITE_OK) {
    fprintf(stderr, "%s: %s\n", error, sqlite3_errmsg(db));
    exit(-1);
  }
}

sqlite3_int64 insert_artist(sqlite3 *db, char *artist) {
  sqlite3_stmt *stmt;
  const char *sql_insert_artist = "INSERT INTO artists (name) VALUES (?);";
  
  rc = sqlite3_prepare_v2(db, sql_insert_artist, -1, &stmt, NULL);
  rc = sqlite3_bind_text(stmt, 1, artist, -1, SQLITE_TRANSIENT);
  rc = sqlite3_step(stmt); // Execute
  rc = sqlite3_finalize(stmt);

  sqlite3_int64 artist_id = sqlite3_last_insert_rowid(db);   

  return artist_id;
}

sqlite3_int64 insert_album(sqlite3 *db, sqlite3_int64 artist_id, char *album, char* cover) {
  sqlite3_stmt *stmt;
  const char *sql_insert_album =
      "INSERT INTO albums (title, artist_id, cover) VALUES (?, ?, ?);";
  int rc;

  rc = sqlite3_prepare_v2(db, sql_insert_album, -1, &stmt, NULL);
  check_sqlite(db, rc, "prepare");
  rc = sqlite3_bind_text(stmt, 1, album, -1, SQLITE_TRANSIENT);
  check_sqlite(db, rc, "bind");
  rc = sqlite3_bind_int64(stmt, 2, artist_id);
  check_sqlite(db, rc, "bind");
  rc = sqlite3_bind_text(stmt, 3, cover, -1, SQLITE_TRANSIENT);
  check_sqlite(db, rc, "bind");
  sqlite3_step(stmt); // Execute
  rc = sqlite3_finalize(stmt);
  check_sqlite(db, rc, "finalize");

  sqlite3_int64 album_id = sqlite3_last_insert_rowid(db);   
  return album_id;
}

sqlite3_int64 insert_or_get_song(sqlite3 *db, const char *title, int track_number,
                                 sqlite3_int64 artist_id,
                                 sqlite3_int64 album_id) {
    sqlite3_stmt *stmt;
    int rc;
    sqlite3_int64 song_id = -1;

    // First: try to find the song
    const char *sql_select =
        "SELECT id FROM songs WHERE title = ? AND artist_id = ? AND album_id = ?;";
    rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, NULL);
    check_sqlite(db, rc, "prepare select");

    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, artist_id);
    sqlite3_bind_int64(stmt, 3, album_id);

    rc = sqlite3_step(stmt);

    // Return if it exists
    if (rc == SQLITE_ROW) {
        song_id = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
        return song_id; 
    }
    sqlite3_finalize(stmt);

    // Insert if not found
    const char *sql_insert =
        "INSERT INTO songs (title, track_number, artist_id, album_id) VALUES (?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL);
    check_sqlite(db, rc, "prepare insert");

    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, track_number);
    sqlite3_bind_int64(stmt, 3, artist_id);
    sqlite3_bind_int64(stmt, 4, album_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Insert failed: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    song_id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return song_id;
}

sqlite3_int64 insert_file(sqlite3 *db, char *path, char* extension, sqlite3_int64 song_id) {
  sqlite3_stmt *stmt;
  const char *sql_insert_album =
      "INSERT INTO files (path, extension, song_id) VALUES (?, ?, ?);";
  int rc;

  rc = sqlite3_prepare_v2(db, sql_insert_album, -1, &stmt, NULL);
  check_sqlite(db, rc, "prepare");

  rc = sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
  check_sqlite(db, rc, "bind");
  rc = sqlite3_bind_text(stmt, 2, extension, -1, SQLITE_TRANSIENT);
  check_sqlite(db, rc, "bind");
  rc = sqlite3_bind_int64(stmt, 3, song_id);
  check_sqlite(db, rc, "bind");

  sqlite3_step(stmt); // Execute
  rc = sqlite3_finalize(stmt);
  check_sqlite(db, rc, "finalize");

  sqlite3_int64 file_id = sqlite3_last_insert_rowid(db);   
  return file_id;
}

