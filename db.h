
#include <stdio.h>
#include <sqlite3.h>


void init_db(sqlite3 *db);
sqlite3_int64 insert_artist(sqlite3 *db, char *artist);
sqlite3_int64 insert_album(sqlite3 *db, sqlite3_int64 artist_id, char *album);
sqlite3_int64 insert_or_get_song(sqlite3 *db, const char *title,
                                 sqlite3_int64 artist_id,
                                 sqlite3_int64 album_id);
sqlite3_int64 insert_file(sqlite3 *db, char *path, char* extension, sqlite3_int64 song_id);
