
#include <stdio.h>
#include <dirent.h>


#include <sqlite3.h>

#include <taglib/tag_c.h>

#define BL_IMPL
#define BL_STRINGBUILDER_IMPL
#include "bl.h"

int subdirs(char *path, struct dirent **dirs);
static int select_flac(const struct dirent *dir);
static int select_dirs(const struct dirent *dir);
static int select_audio_file(const struct dirent *dir);

int rc;

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
   int i;
   for(i = 0; i<argc; i++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}

char* init_artists =
    "create table artists("
    "id   integer primary key autoincrement,"
    "name text unique not null"
    ");";

char *init_albums =
  "create table albums("
  "id        integer primary key autoincrement,"
  "title      text not null,"
  "artist_id integer," 
  "foreign key(artist_id) references artist(id)"
  ");";

char *init_songs = "create table songs("
  "id        integer primary key autoincrement,"
  "title     text not null,"
  "artist_id integer,"
  "album_id  integer,"
  "foreign key(artist_id) references artist(id),"
  "foreign key(album_id)  references album(id),"
  "UNIQUE(title, artist_id, album_id)"
  ");";

char *init_files = "create table files("
  "id        integer primary key autoincrement,"
  "path      text not null,"
  "extension text not null,"
  "song_id integer," 
  "foreign key(song_id) references songs(id)"
  ");";



char *zErrMsg = 0;
int rc;

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
  printf("inserting %s\n", artist);

  sqlite3_stmt *stmt;
  const char *sql_insert_artist = "INSERT INTO artists (name) VALUES (?);";
  
  rc = sqlite3_prepare_v2(db, sql_insert_artist, -1, &stmt, NULL);
  rc = sqlite3_bind_text(stmt, 1, artist, -1, SQLITE_TRANSIENT);
  rc = sqlite3_step(stmt); // Execute
  rc = sqlite3_finalize(stmt);

  sqlite3_int64 artist_id = sqlite3_last_insert_rowid(db);   

  return artist_id;
}

sqlite3_int64 insert_album(sqlite3 *db, sqlite3_int64 artist_id, char *album) {
  sqlite3_stmt *stmt;
  const char *sql_insert_album =
      "INSERT INTO albums (title, artist_id) VALUES (?, ?);";
  int rc;

  rc = sqlite3_prepare_v2(db, sql_insert_album, -1, &stmt, NULL);
  check_sqlite(db, rc, "prepare");
  rc = sqlite3_bind_text(stmt, 1, album, -1, SQLITE_TRANSIENT);
  check_sqlite(db, rc, "bind");
  rc = sqlite3_bind_int64(stmt, 2, artist_id);
  check_sqlite(db, rc, "bind");
  sqlite3_step(stmt); // Execute
  rc = sqlite3_finalize(stmt);
  check_sqlite(db, rc, "finalize");

  sqlite3_int64 album_id = sqlite3_last_insert_rowid(db);   
  return album_id;
}

sqlite3_int64 insert_or_get_song(sqlite3 *db, const char *title,
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
        "INSERT INTO songs (title, artist_id, album_id) VALUES (?, ?, ?);";
    rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL);
    check_sqlite(db, rc, "prepare insert");

    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, artist_id);
    sqlite3_bind_int64(stmt, 3, album_id);

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


char *get_filename_ext(const char *filename) {
  char *dot = strrchr(filename, '.');
  if (!dot || dot == filename) return "";
  return dot + 1;
}



int main(int argc, char* argv[]) {
  char* root;

  // Check for incorrect args
  if (argc != 2) return -1;
  root = argv[1];

  // Build artist dirs
  struct dirent **artists;
  int artist_count = scandir(root, &artists, select_dirs, alphasort);

  sqlite3 *db;
  sqlite3_open("musikk.sqlite", &db);
  init_db(db);

  if( rc ) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    return (0);
  }

  // Iterate over albums for each artists
  forx(artist_idx, artist_count) {
    char* artist_name = artists[artist_idx]->d_name;
    printf("%s\n", artist_name);

    char* artist_dir = concat(root, "/", artist_name);
    sqlite3_int64 artist_id = insert_artist(db, artist_name);
    
    struct dirent **albums;
    int album_count = scandir(artist_dir, &albums, select_dirs, alphasort);

    forx(album_idx, album_count) {
      char* album_title = albums[album_idx]->d_name;
      printf(" > %s\n", album_title);

      char *album_dir = concat(artist_dir, "/", album_title);
      sqlite3_int64 album_id = insert_album(db, artist_id, album_title);

      struct dirent **formats;
      int format_count = scandir(album_dir, &formats, select_dirs, alphasort);

      
      forx(format_idx, format_count) {
        char *format = formats[format_idx]->d_name;
	printf("  > %s\n", format);
        char *format_dir = concat(album_dir, "/", format);

	printf("%s\n", format_dir);
	
        struct dirent **songs;
        int song_count =
            scandir(format_dir, &songs, select_audio_file, alphasort);

	printf("songs: %d\n", song_count);
	
        forx(song_idx, song_count) {
	  char* song_filename = songs[song_idx]->d_name;
	  char *song_path = concat(format_dir, "/", song_filename);
	  printf("  > %s\n", song_filename);

	  char* song_ext = get_filename_ext(song_filename);

	  TagLib_File *file = taglib_file_new(song_path);
	  if (!file) {
	    fprintf(stderr, "Failed to open file\n");
	    return 1;
	  }

	  TagLib_Tag *tag = taglib_file_tag(file);

	  sqlite3_int64 song_id =
	    insert_or_get_song(db, taglib_tag_title(tag), artist_id, album_id);

	  sqlite3_int64 file_id =
	    insert_file(db, song_path, song_ext, song_id);
	  taglib_file_free(file);
	  free(songs[song_idx]);
	}
	free(songs);
      }
    }
  }

  sqlite3_close(db);

  return 0;
}

/* when return 1, scandir will put this dirent to the list */
static int select_dirs(const struct dirent *dir) {
  if(!dir) return 0;

  if (dir->d_type == DT_DIR) { /* only deal with regular file */
    if (strcmp(dir->d_name, ".") == 0) return 0;
    if (strcmp(dir->d_name, "..") == 0) return 0;
    return 1;
  }

  return 0;
}

/* when return 1, scandir will put this dirent to the list */
static int select_flac(const struct dirent *dir) {
  if (!dir) return 0;

  if (dir->d_type == DT_REG) { /* only deal with regular file */
    const char *ext = strrchr(dir->d_name,'.');
    if ((!ext) || (ext == dir->d_name)) return 0;
    else {
      if (strcmp(ext, ".flac") == 0) return 1;
    }
  }

  return 0;
}

/* when return 1, scandir will put this dirent to the list */
static int select_audio_file(const struct dirent *dir) {
  if (!dir) return 0;

  if (dir->d_type == DT_REG) { /* only deal with regular file */
    const char *ext = strrchr(dir->d_name,'.');
    if ((!ext) || (ext == dir->d_name)) return 0;
    else {
      if (strcmp(ext, ".mp3") == 0)  return 1;
      if (strcmp(ext, ".flac") == 0) return 1;
      if (strcmp(ext, ".ogg") == 0)  return 1;
    }
  }

  return 0;
}



int subdirs(char* path, struct dirent** dirs) {
  DIR *dr = opendir(path);

  if (dr == NULL) {
    printf("Could not open current directory \"%s\"", path);
    return 0;
  }

  // We currently handle this with a dynamic array
  int allocated_size = 8;
  *dirs = malloc(sizeof(struct dirent)*allocated_size);
  
  int dir_count = 0;
  struct dirent *de;  // Pointer for directory entry
  for (;;) {
    if (dir_count == allocated_size) {
      allocated_size *= 2;
      *dirs = realloc(*dirs, sizeof(struct dirent)*allocated_size);
    }

    de = readdir(dr);
    if (de == NULL)
      break;

    (*dirs)[dir_count] = *de;
    dir_count++;
  }

  closedir(dr);

  return dir_count;
}

