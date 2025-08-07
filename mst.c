
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
  "extension text not null,"
  "file_name text not null,"
  "artist_id integer," 
  "album_id  integer," 
  "foreign key(artist_id) references artist(id),"
  "foreign key(album_id)  references album(id)"
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

sqlite3_int64 insert_song(sqlite3 *db, char *title, char *extension,
                          char *file_name, sqlite3_int64 artist_id,
                          sqlite3_int64 album_id) {

  sqlite3_stmt *stmt;
  const char *sql_insert_album =
      "INSERT INTO songs (title, extension, file_name, artist_id, album_id) VALUES (?, ?, ?, ?, ?);";
  int rc;

  rc = sqlite3_prepare_v2(db, sql_insert_album, -1, &stmt, NULL);
  check_sqlite(db, rc, "prepare");

  rc = sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
  check_sqlite(db, rc, "bind");
  rc = sqlite3_bind_text(stmt, 2, file_name, -1, SQLITE_TRANSIENT);
  check_sqlite(db, rc, "bind");
  rc = sqlite3_bind_text(stmt, 3, extension, -1, SQLITE_TRANSIENT);
  check_sqlite(db, rc, "bind");
  rc = sqlite3_bind_int64(stmt, 4, artist_id);
  check_sqlite(db, rc, "bind");
  rc = sqlite3_bind_int64(stmt, 5, album_id);
  check_sqlite(db, rc, "bind");

  sqlite3_step(stmt); // Execute
  rc = sqlite3_finalize(stmt);
  check_sqlite(db, rc, "finalize");

  sqlite3_int64 song_id = sqlite3_last_insert_rowid(db);   
  return song_id;
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
    printf("%s\n", artists[artist_idx]->d_name);
    char* artist_dir = concat(root, "/", artists[artist_idx]->d_name);

    sqlite3_int64 artist_id = insert_artist(db, artists[artist_idx]->d_name);
    
    struct dirent **albums;
    int album_count = scandir(artist_dir, &albums, select_dirs, alphasort);

    forx(album_idx, album_count) {
      printf(" > %s\n", albums[album_idx]->d_name);
      char *album_dir = concat(root, "/", artists[artist_idx]->d_name, "/", albums[album_idx]->d_name, "/");

      sqlite3_int64 album_id = insert_album(db, artist_id, albums[album_idx]->d_name);
      
      struct dirent **songs;
      int song_count = scandir(album_dir, &songs, select_audio_file, alphasort);

      forx(song_idx, song_count) {
        char *song_filename = songs[song_idx]->d_name;
	char *song_path = concat(root, "/", artists[artist_idx]->d_name, "/", albums[album_idx]->d_name, "/", song_filename);

	char* ext = get_filename_ext(song_filename);

	TagLib_File *file = taglib_file_new(song_path);
	if (!file) {
	  fprintf(stderr, "Failed to open file\n");
	  return 1;
	}

	TagLib_Tag *tag = taglib_file_tag(file);

        sqlite3_int64 song_id =
	  insert_song(db, "", ext, taglib_tag_title(tag), artist_id, album_id);

	taglib_file_free(file);
	free(songs[song_idx]);
      }
      free(songs);
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

