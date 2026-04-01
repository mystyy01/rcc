#include "compiler.h"
#include "codegen.h"
#include <args.h>
#include <fcntl.h>
#include <fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
int main(int argc, char *argv[]) {
  define_arg_ex("-o", T_REQUIRED, true, "Output binary", NAMESPACE_NONE,
                "file");
  define_arg_ex("--help", T_OPTIONAL, false, "Display this help page",
                NAMESPACE_NONE, NULL);
  define_positional("source_file", T_REQUIRED, -1, "Input source file");
  size_t nargs;
  struct return_arg *args = get_args(argc, argv, &nargs);

  if (arg_exists(args, nargs, "--help")) {
    print_help(argv[0]);
    return EXIT_FAILURE;
  }

  const char *outfile = arg_value(args, nargs, "-o");
  const char *source_file = arg_value(args, nargs, "source_file");

  (void)outfile;

  // open the source file

  if (check_exists(source_file) != F_FILE) {
    // not a file / doesnt exist
    printf("Source file could not be found.\n");
    return EXIT_FAILURE;
  }
  int fd = open(source_file, O_RDONLY);
  if (fd < 0) {
    printf("Failed to open source file.\n");
    return EXIT_FAILURE;
  }
  char *rust_file = NULL;
  codegen(source_file, &rust_file);

  char cmd[4096]; // todo: make the size of the cmd buffer bigger/heap allocated
  snprintf(cmd, sizeof(cmd) - 1, "rustc %s -o %s", rust_file, outfile);
  system(cmd);

  return EXIT_SUCCESS;
}
