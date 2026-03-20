#include "fs.h"

#include "args.h"
#include <dirent.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int check_exists(const char *fp) {
  struct stat s;
  if (stat(fp, &s) != 0) {
    return F_NONE_EXISTENT;
  }
  if (S_ISDIR(s.st_mode)) {
    return F_DIRECTORY;
  } else if (S_ISREG(s.st_mode)) {
    return F_FILE;
  } else {
    return F_OTHER;
  }
}

struct file_entry *read_dir(const char *path, size_t *out_len) {
  DIR *dir = opendir(path);
  if (!dir) {
    return NULL;
  }

  struct file_entry *entries = NULL;
  size_t entries_len = 0;
  size_t entries_cap = 0;
  struct dirent *entry;
  char full_path[PATH_MAX];

  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.' &&
        (entry->d_name[1] == '\0' ||
         (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
      continue;
    }

    int written =
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
    if (written < 0 || written >= (int)sizeof(full_path)) {
      closedir(dir);
      free_dir_entries(entries, entries_len);
      return NULL;
    }

    size_t name_len = strlen(entry->d_name) + 1;
    struct file_entry file = {
        .name = malloc(name_len),
        .type = check_exists(full_path),
    };

    if (!file.name) {
      closedir(dir);
      free_dir_entries(entries, entries_len);
      return NULL;
    }

    memcpy(file.name, entry->d_name, name_len);

    void *tmp =
        append(entries, &entries_len, &entries_cap, &file, sizeof(file));
    if (!tmp) {
      free(file.name);
      closedir(dir);
      free_dir_entries(entries, entries_len);
      return NULL;
    }

    entries = tmp;
  }

  closedir(dir);
  if (out_len) {
    *out_len = entries_len;
  }
  return entries;
}

void free_dir_entries(struct file_entry *entries, size_t len) {
  if (!entries) {
    return;
  }

  for (size_t i = 0; i < len; i++) {
    free(entries[i].name);
  }

  free(entries);
}

int rmr(const char *path) {
  int exists = check_exists(path);
  if (exists == F_NONE_EXISTENT) {
    return EXIT_SUCCESS;
  }

  if (exists == F_FILE || exists == F_OTHER) {
    if (unlink(path) != 0) {
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }

  size_t dir_len = 0;
  struct file_entry *entries = read_dir(path, &dir_len);
  if (!entries && dir_len > 0) {
    return EXIT_FAILURE;
  }

  for (size_t i = 0; i < dir_len; i++) {
    char full_path[PATH_MAX];
    int written =
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entries[i].name);
    if (written < 0 || written >= (int)sizeof(full_path)) {
      free_dir_entries(entries, dir_len);
      return EXIT_FAILURE;
    }

    if (rmr(full_path) != EXIT_SUCCESS) {
      free_dir_entries(entries, dir_len);
      return EXIT_FAILURE;
    }
  }

  free_dir_entries(entries, dir_len);

  if (rmdir(path) != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int copy_file(const char *src, const char *dst) {
  struct stat src_stat;
  FILE *in = fopen(src, "rb");
  if (!in) {
    return EXIT_FAILURE;
  }

  if (stat(src, &src_stat) != 0) {
    fclose(in);
    return EXIT_FAILURE;
  }

  FILE *out = fopen(dst, "wb");
  if (!out) {
    fclose(in);
    return EXIT_FAILURE;
  }

  char buf[8192];
  size_t n;

  while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
    if (fwrite(buf, 1, n, out) != n) {
      fclose(in);
      fclose(out);
      return EXIT_FAILURE;
    }
  }

  fclose(in);
  fclose(out);

  if (chmod(dst, src_stat.st_mode & 0777) != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
