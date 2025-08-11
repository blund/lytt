
#include <stdio.h>
#include <dirent.h>

#include <sqlite3.h>

#include <taglib/tag_c.h>

#define BL_IMPL
#define BL_STRINGBUILDER_IMPL
#include "../bl.h"

#include "db.h"
#include "files.h"

char* make_small_cover(char *orig_path, char *album_path);

int main(int argc, char* argv[]) {

  // Check for incorrect args
  if (argc != 3) return -1;
  char* root = argv[1];
  char* db_path = argv[2];

  // Build artist dirs
  struct dirent **artists;
  int artist_count = scandir(root, &artists, select_dirs, alphasort);

  sqlite3 *db;
  sqlite3_open(db_path, &db);
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

      struct dirent **covers;
      int cover_count = scandir(album_dir, &covers, select_image_file, NULL);

      char *cover = "";

      // small cover not generated yet
      if (cover_count == 2) cover = concat(album_dir, "/small_cover.jpg");
      if (cover_count == 1) {
	cover = make_small_cover(covers[0]->d_name, album_dir);
      }

      sqlite3_int64 album_id = insert_album(db, artist_id, album_title, cover);

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
	    insert_or_get_song(db, taglib_tag_title(tag), taglib_tag_track(tag), artist_id, album_id);

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

char *make_small_cover(char *orig_file, char *album_dir) {
  char *orig_cover_path = concat(album_dir, "/", orig_file);
  char *small_cover_path = concat(album_dir, "/small_cover.jpg");

  // Build command string
  char cmd[512];
  snprintf(cmd, sizeof(cmd),
	   "ffmpeg -y -i \"%s\" -vf scale=300:300 \"%s\" > /dev/null 2>&1",
	   orig_cover_path, small_cover_path);

  // Run conversion command
  int ret = system(cmd);
  if (ret != 0) {
    fprintf(stderr, "Failed to generate small cover for %s\n", orig_cover_path);
    // fallback to original or handle error
  }


  char *new_cover = concat(album_dir, "/small_cover.jpg");
  printf("Generating new cover: %s\n", new_cover);
  return new_cover;
}
