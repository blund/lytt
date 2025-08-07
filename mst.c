
#include <stdio.h>
#include <dirent.h>

#include <sqlite3.h>

#include <taglib/tag_c.h>

#define BL_IMPL
#define BL_STRINGBUILDER_IMPL
#include "bl.h"

#include "db.h"
#include "files.h"

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

  // Iterate over artist folders
  forx(artist_idx, artist_count) {
    char* artist_name = artists[artist_idx]->d_name;
    printf("%s\n", artist_name);

    char* artist_dir = concat(root, "/", artist_name);
    sqlite3_int64 artist_id = insert_artist(db, artist_name);
    
    struct dirent **albums;
    int album_count = scandir(artist_dir, &albums, select_dirs, alphasort);

    // Iterate over album folders
    forx(album_idx, album_count) {
      char* album_title = albums[album_idx]->d_name;
      printf(" > %s\n", album_title);

      char *album_dir = concat(artist_dir, "/", album_title);
     sqlite3_int64 album_id = insert_album(db, artist_id, album_title);

      struct dirent **formats;
      int format_count = scandir(album_dir, &formats, select_dirs, alphasort);

      // Iterate over file format folders
      forx(format_idx, format_count) {
        char *format = formats[format_idx]->d_name;
	printf("  > %s\n", format);
        char *format_dir = concat(album_dir, "/", format);

        struct dirent **songs;
        int song_count =
            scandir(format_dir, &songs, select_audio_file, alphasort);

	// Iterate over song files
        forx(song_idx, song_count) {
	  char* song_filename = songs[song_idx]->d_name;
	  char *song_path = concat(format_dir, "/", song_filename);
	  printf("   > %s\n", song_filename);

	  TagLib_File *file = taglib_file_new(song_path);
	  if (!file) {
	    fprintf(stderr, "Failed to open file\n");
	    return 1;
	  }

	  TagLib_Tag *tag = taglib_file_tag(file);

	  sqlite3_int64 song_id =
	    insert_or_get_song(db, taglib_tag_title(tag), artist_id, album_id);

          sqlite3_int64 file_id = insert_file(db, song_path, format, song_id);

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

