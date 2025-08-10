
#include <string.h>

#include <dirent.h>


int select_dirs(const struct dirent *dir) {
  if(!dir) return 0;

  if (dir->d_type == DT_DIR) { /* only deal with regular file */
    if (strcmp(dir->d_name, ".") == 0) return 0;
    if (strcmp(dir->d_name, "..") == 0) return 0;
    return 1;
  }

  return 0;
}

int select_audio_file(const struct dirent *dir) {
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

int select_image_file(const struct dirent *dir) {
  if (!dir) return 0;

  if (dir->d_type == DT_REG) { /* only deal with regular file */
    const char *ext = strrchr(dir->d_name,'.');
    if ((!ext) || (ext == dir->d_name)) return 0;
    else {
      if (strcmp(ext, ".jpg") == 0)  return 1;
      if (strcmp(ext, ".png") == 0) return 1;
    }
  }

  return 0;
}
