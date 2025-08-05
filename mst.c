
#include <stdio.h>
#include <dirent.h>

#include "stb_vorbis.c"

#define BL_IMPL
#define BL_STRINGBUILDER_IMPL
#include "bl.h"

int subdirs(char *path, struct dirent **dirs);
static int select_flac(const struct dirent *dir);
static int select_dirs(const struct dirent *dir);
static int select_audio_file(const struct dirent *dir);

int main(int argc, char* argv[]) {
  char* root;

  // Check for incorrect args
  if (argc != 2) return -1;

  // Set root
  root = argv[1];

  // Build artist dirs
  struct dirent **artists;
  int artist_count = scandir(root, &artists, select_dirs, alphasort);

  // Iterate over albums for each artists
  forx(artist_idx, artist_count) {
    printf("%s\n", artists[artist_idx]->d_name);
    char* artist_dir = concat(root, "/", artists[artist_idx]->d_name);
    
    struct dirent **albums;
    int album_count = scandir(artist_dir, &albums, select_dirs, alphasort);

    forx(album_idx, album_count) {
      printf(" > %s\n", albums[album_idx]->d_name);
      char *album_dir =
        concat(root, "/", artists[artist_idx]->d_name, "/", albums[album_idx]->d_name, "/");

      struct dirent **songs;
       int song_count = scandir(album_dir, &songs, select_audio_file, alphasort);
       if (song_count < 0) {
           perror("scandir");
           return 1;
       }

      forx(song_idx, song_count) {
        printf("  > %s\n", songs[song_idx]->d_name);
	free(songs[song_idx]);
      }
      free(songs);
    }
  }

  return 0;
}

/* when return 1, scandir will put this dirent to the list */
static int select_dirs(const struct dirent *dir)
{
  if(!dir)
    return 0;

  if (dir->d_type == DT_DIR) { /* only deal with regular file */
    if (strcmp(dir->d_name, ".") == 0) return 0;
    if (strcmp(dir->d_name, "..") == 0) return 0;
    return 1;
  }

  return 0;
}

/* when return 1, scandir will put this dirent to the list */
static int select_flac(const struct dirent *dir)
{
  if(!dir)
    return 0;

  if(dir->d_type == DT_REG) { /* only deal with regular file */
    const char *ext = strrchr(dir->d_name,'.');
    if((!ext) || (ext == dir->d_name))
      return 0;
    else {
      if(strcmp(ext, ".flac") == 0)
	return 1;
    }
  }

  return 0;
}

/* when return 1, scandir will put this dirent to the list */
static int select_audio_file(const struct dirent *dir)
{
  if(!dir)
    return 0;

  if(dir->d_type == DT_REG) { /* only deal with regular file */
    const char *ext = strrchr(dir->d_name,'.');
    if((!ext) || (ext == dir->d_name))
      return 0;
    else {
      if(strcmp(ext, ".mp3") == 0)
	return 1;
      if(strcmp(ext, ".flac") == 0)
	return 1;
      if(strcmp(ext, ".ogg") == 0)
	return 1;
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

    // Skip these special dirs

    (*dirs)[dir_count] = *de;
    dir_count++;
  }

  closedir(dr);

  return dir_count;
}

