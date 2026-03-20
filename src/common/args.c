#include "args.h"
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct argument *g_args = NULL;
size_t g_args_len = 0;
size_t g_args_cap = 0;
struct positional_arg *g_positionals = NULL;
size_t g_positionals_len = 0;
size_t g_positionals_cap = 0;
struct arg_namespace *g_namespaces = NULL;
size_t g_namespaces_len = 0;
size_t g_namespaces_cap = 0;

void *append(void *array, size_t *len, size_t *cap, const void *value,
             size_t elem_size) {
  if (!len || !cap || !value || elem_size == 0) {
    return NULL;
  }

  if (*len == *cap) {
    size_t new_cap = *cap == 0 ? 4 : *cap * 2;
    void *tmp = realloc(array, new_cap * elem_size);

    if (!tmp) {
      return NULL;
    }

    array = tmp;
    *cap = new_cap;
  }

  memcpy((char *)array + (*len * elem_size), value, elem_size);
  (*len)++;

  return array;
}

int define_arg_ex(const char *name, const int type, const bool flag,
                  const char *description, int namespace_id,
                  const char *value_name) {
  struct argument arg = {
      .name = name,
      .type = type,
      .flag = flag,
      .description = description,
      .namespace_id = namespace_id,
      .value_name = value_name,
  };
  void *tmp = append(g_args, &g_args_len, &g_args_cap, &arg, sizeof(arg));
  if (!tmp) {
    return EXIT_FAILURE;
  }
  g_args = tmp;
  return EXIT_SUCCESS;
}

int define_arg(const char *name, const int type, const bool flag) {
  return define_arg_ex(name, type, flag, NULL, NAMESPACE_NONE, NULL);
}

int define_positional(const char *name, const int type, const int expected_pos,
                      const char *description) {
  struct positional_arg pos = {
      .name = name,
      .type = type,
      .expected_pos = expected_pos,
      .description = description,
  };
  void *tmp = append(g_positionals, &g_positionals_len, &g_positionals_cap,
                     &pos, sizeof(pos));
  if (!tmp) {
    return EXIT_FAILURE;
  }
  g_positionals = tmp;
  return EXIT_SUCCESS;
}

int define_namespace(int id, const char *name, const char *description) {
  struct arg_namespace arg_namespace = {
      .id = id,
      .name = name,
      .description = description,
  };
  void *tmp = append(g_namespaces, &g_namespaces_len, &g_namespaces_cap,
                     &arg_namespace, sizeof(arg_namespace));
  if (!tmp) {
    return EXIT_FAILURE;
  }

  g_namespaces = tmp;
  return EXIT_SUCCESS;
}

static const struct arg_namespace *find_namespace_by_name(const char *name) {
  if (!name) {
    return NULL;
  }

  for (size_t i = 0; i < g_namespaces_len; i++) {
    if (strcmp(g_namespaces[i].name, name) == 0) {
      return &g_namespaces[i];
    }
  }

  return NULL;
}

static int find_arg(const char *name, int namespace_id) {
  int fallback_index = -1;

  for (size_t i = 0; i < g_args_len; i++) {
    if (strcmp(g_args[i].name, name) != 0) {
      continue;
    }

    if (g_args[i].namespace_id == namespace_id) {
      return (int)i;
    }

    if (g_args[i].namespace_id == NAMESPACE_NONE && fallback_index == -1) {
      fallback_index = (int)i;
    }
  }

  return fallback_index;
}

static bool has_required_arg(const struct return_arg *parsed, size_t parsed_len,
                             const char *name) {
  for (size_t i = 0; i < parsed_len; i++) {
    if (strcmp(parsed[i].name, name) == 0) {
      return true;
    }
  }

  return false;
}

static const struct arg_namespace *find_namespace(int id) {
  for (size_t i = 0; i < g_namespaces_len; i++) {
    if (g_namespaces[i].id == id) {
      return &g_namespaces[i];
    }
  }

  return NULL;
}

static bool arg_in_scope(const struct argument *arg, int namespace_id) {
  if (arg->namespace_id == NAMESPACE_NONE) {
    return true;
  }

  return arg->namespace_id == namespace_id;
}

static void print_arg_line(const struct argument *arg) {
  const char *description = arg->description ? arg->description : "";
  const char *value_name = arg->value_name ? arg->value_name : "value";

  if (arg->flag) {
    printf("  %s <%s>", arg->name, value_name);
  } else {
    printf("  %s", arg->name);
  }

  if (arg->type == T_REQUIRED) {
    printf(" (required)");
  }

  if (description[0] != '\0') {
    printf("  %s", description);
  }

  putchar('\n');
}

void print_help(const char *program_name) {
  const char *name = program_name ? program_name : "program";
  bool printed_header = false;
  bool printed_commands = false;

  printf("Usage: %s", name);
  for (size_t i = 0; i < g_positionals_len; i++) {
    if (g_positionals[i].type == T_REQUIRED) {
      printf(" <%s>", g_positionals[i].name);
    } else {
      printf(" [%s]", g_positionals[i].name);
    }
  }
  printf(" [options]\n");

  if (g_namespaces_len > 0) {
    printf("       %s <command> [options]\n", name);
  }

  if (g_positionals_len > 0) {
    printf("\nPositional arguments:\n");
    for (size_t i = 0; i < g_positionals_len; i++) {
      printf("  %s", g_positionals[i].name);
      if (g_positionals[i].type == T_REQUIRED) {
        printf(" (required)");
      }
      if (g_positionals[i].description) {
        printf("  %s", g_positionals[i].description);
      }
      putchar('\n');
    }
  }

  for (size_t i = 0; i < g_namespaces_len; i++) {
    if (!printed_commands) {
      printf("\nCommands:\n");
      printed_commands = true;
    }

    printf("  %s", g_namespaces[i].name);
    if (g_namespaces[i].description && g_namespaces[i].description[0] != '\0') {
      printf("  %s", g_namespaces[i].description);
    }
    putchar('\n');
  }

  for (size_t i = 0; i < g_args_len; i++) {
    if (g_args[i].namespace_id != NAMESPACE_NONE) {
      continue;
    }

    if (!printed_header) {
      printf("\nOptions:\n");
      printed_header = true;
    }
    print_arg_line(&g_args[i]);
  }

  for (size_t i = 0; i < g_namespaces_len; i++) {
    bool printed_namespace_header = false;

    for (size_t j = 0; j < g_args_len; j++) {
      if (g_args[j].namespace_id != g_namespaces[i].id) {
        continue;
      }

      if (!printed_namespace_header) {
        printf("\n%s:\n", g_namespaces[i].name);
        if (g_namespaces[i].description &&
            g_namespaces[i].description[0] != '\0') {
          printf("  %s\n", g_namespaces[i].description);
        }
        printed_namespace_header = true;
      }
      print_arg_line(&g_args[j]);
    }
  }

  for (size_t i = 0; i < g_args_len; i++) {
    if (g_args[i].namespace_id == NAMESPACE_NONE) {
      continue;
    }
    if (find_namespace(g_args[i].namespace_id) != NULL) {
      continue;
    }

    if (!printed_header) {
      printf("\nOptions:\n");
      printed_header = true;
    }
    print_arg_line(&g_args[i]);
  }
}

struct return_arg *get_args(int argc, char *argv[], size_t *out_len) {
  struct return_arg *parsed = NULL;
  size_t parsed_len = 0;
  size_t parsed_cap = 0;
  int active_namespace_id = NAMESPACE_NONE;
  int positional_count = 0;

  if (out_len) {
    *out_len = 0;
  }

  if (argc <= 1) {
    parsed = calloc(1, sizeof(*parsed));
    if (!parsed) {
      return NULL;
    }

    return parsed;
  }

  for (int i = 1; i < argc; i++) {
    const struct arg_namespace *arg_namespace = NULL;
    int index = -1;

    if (argv[i][0] != '-') {
      arg_namespace = find_namespace_by_name(argv[i]);
      if (arg_namespace != NULL) {
        if (active_namespace_id != NAMESPACE_NONE) {
          fprintf(stderr, "Error: multiple namespaces are not supported: %s\n",
                  argv[i]);
          free(parsed);
          return NULL;
        }

        active_namespace_id = arg_namespace->id;

        struct return_arg parsed_namespace = {
            .name = arg_namespace->name,
            .value = NULL,
            .pos = i,
            .is_namespace = true,
            .namespace_id = arg_namespace->id,
        };
        void *namespace_tmp = append(parsed, &parsed_len, &parsed_cap,
                                     &parsed_namespace,
                                     sizeof(parsed_namespace));
        if (!namespace_tmp) {
          fprintf(stderr, "Error: failed to store parsed namespace\n");
          free(parsed);
          return NULL;
        }

        parsed = namespace_tmp;
        continue;
      }
    }

    index = find_arg(argv[i], active_namespace_id);

    if (index == -1) {
      if (argv[i][0] != '-') {
        const char *pos_name = "";
        for (size_t p = 0; p < g_positionals_len; p++) {
          if (g_positionals[p].expected_pos == positional_count ||
              g_positionals[p].expected_pos == -1) {
            pos_name = g_positionals[p].name;
            break;
          }
        }
        struct return_arg positional = {
            .name = pos_name,
            .value = argv[i],
            .pos = i,
            .is_namespace = false,
            .namespace_id = NAMESPACE_NONE,
        };
        void *tmp = append(parsed, &parsed_len, &parsed_cap, &positional,
                           sizeof(positional));
        if (!tmp) {
          free(parsed);
          return NULL;
        }
        parsed = tmp;
        positional_count++;
        continue;
      }
      fprintf(stderr, "Error: unknown argument: %s\n", argv[i]);
      free(parsed);
      return NULL;
    }

    struct argument arg = g_args[index];
    struct return_arg parsed_arg = {
        .name = arg.name,
        .value = NULL,
        .pos = i,
        .is_namespace = false,
        .namespace_id = arg.namespace_id,
    };

    if (arg.flag) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: missing value for argument: %s\n", arg.name);
        free(parsed);
        return NULL;
      }

      parsed_arg.value = argv[i + 1];
      i++;
    }

    void *tmp = append(parsed, &parsed_len, &parsed_cap, &parsed_arg,
                       sizeof(parsed_arg));
    if (!tmp) {
      fprintf(stderr, "Error: failed to store parsed arguments\n");
      free(parsed);
      return NULL;
    }

    parsed = tmp;
  }

  if (!has_required_arg(parsed, parsed_len, "--help")) {
    for (size_t i = 0; i < g_args_len; i++) {
      if (!arg_in_scope(&g_args[i], active_namespace_id)) {
        continue;
      }

      if (g_args[i].type == T_REQUIRED &&
          !has_required_arg(parsed, parsed_len, g_args[i].name)) {
        fprintf(stderr, "Error: missing required argument: %s\n", g_args[i].name);
        free(parsed);
        return NULL;
      }
    }

    for (size_t i = 0; i < g_positionals_len; i++) {
      if (g_positionals[i].type == T_REQUIRED &&
          !has_required_arg(parsed, parsed_len, g_positionals[i].name)) {
        fprintf(stderr, "Error: missing required positional argument: %s\n",
                g_positionals[i].name);
        free(parsed);
        return NULL;
      }
    }
  }

  if (out_len) {
    *out_len = parsed_len;
  }

  return parsed;
}

const char *arg_value(const struct return_arg *args, size_t len,
                      const char *name) {
  if (!args || !name) {
    return NULL;
  }

  for (size_t i = 0; i < len; i++) {
    if (strcmp(args[i].name, name) == 0) {
      return args[i].value;
    }
  }

  return NULL;
}

bool arg_exists(const struct return_arg *args, size_t len, const char *name) {
  if (!args || !name) {
    return false;
  }

  for (size_t i = 0; i < len; i++) {
    if (strcmp(args[i].name, name) == 0) {
      return true;
    }
  }

  return false;
}

const char *arg_positional(const struct return_arg *args, size_t len, int index) {
  if (!args) {
    return NULL;
  }

  int count = 0;
  for (size_t i = 0; i < len; i++) {
    if (args[i].name[0] == '\0') {
      if (count == index) {
        return args[i].value;
      }
      count++;
    }
  }

  return NULL;
}

int arg_namespace_id(const struct return_arg *args, size_t len) {
  if (!args) {
    return NAMESPACE_NONE;
  }

  for (size_t i = 0; i < len; i++) {
    if (args[i].is_namespace) {
      return args[i].namespace_id;
    }
  }

  return NAMESPACE_NONE;
}

const char *arg_namespace_name(const struct return_arg *args, size_t len) {
  if (!args) {
    return NULL;
  }

  for (size_t i = 0; i < len; i++) {
    if (args[i].is_namespace) {
      return args[i].name;
    }
  }

  return NULL;
}
