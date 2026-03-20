#pragma once

#include <stddef.h>

#define F_NONE_EXISTENT 0
#define F_DIRECTORY 1
#define F_FILE 2
#define F_OTHER 3

struct file_entry {
  char *name;
  int type;
};

int check_exists(const char *fp);
struct file_entry *read_dir(const char *path, size_t *out_len);
void free_dir_entries(struct file_entry *entries, size_t len);
int rmr(const char *path);
int copy_file(const char *src, const char *dst);
