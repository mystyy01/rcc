#pragma once

#include <stdbool.h>
#include <stddef.h>

#define T_OPTIONAL 0
#define T_REQUIRED 1
#define NAMESPACE_NONE -1

struct arg_namespace {
  const int id;
  const char *name;
  const char *description;
};

struct argument {
  const char *name; // the actual arg passed. so in "program --arg", the "--arg"
                    // would be the name
  const int type;  // defines whether an error should be thrown if this argument
                   // is not provided
  const bool flag; // "--flag value" can be treated as one arg
  const char *description; // showed on the auto generated help page
  const int
      namespace_id; // set -1 if no namespace, or set corresponding namespace_id
  const char
      *value_name; // shown on the help page as: name <value_name> description
};

struct positional_arg {
  const char *name;
  const int type;
  const int expected_pos; // -1 for any position
  const char *description;
};
struct return_arg {
  const char *name;
  const char *value;
  const int pos;
  const bool is_namespace;
  const int namespace_id;
};

void *append(void *array, size_t *len, size_t *cap, const void *value,
             size_t elem_size);

int define_arg(const char *name, const int type, const bool flag);
int define_arg_ex(const char *name, const int type, const bool flag,
                  const char *description, int namespace_id,
                  const char *value_name);
int define_positional(const char *name, const int type, const int expected_pos,
                      const char *description);
int define_namespace(int id, const char *name, const char *description);

struct return_arg *get_args(int argc, char *argv[], size_t *out_len);

const char *arg_value(const struct return_arg *args, size_t len,
                      const char *name);
bool arg_exists(const struct return_arg *args, size_t len, const char *name);
const char *arg_positional(const struct return_arg *args, size_t len, int index);
int arg_namespace_id(const struct return_arg *args, size_t len);
const char *arg_namespace_name(const struct return_arg *args, size_t len);

void print_help(const char *program_name);
