#define _GNU_SOURCE
#include "codegen.h"
#include <clang-c/CXSourceLocation.h>
#include <clang-c/CXString.h>
#include <clang-c/Index.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  bool in_main;
} CodegenState;

static FILE *out_fp;

static void emit(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(out_fp, fmt, args);
  vfprintf(
      stdout, fmt,
      args); // write to stdout for debugging purposes, remove this in future

  va_end(args);
}

char *rs_type(CXString c_type) {
  const char *c_str = clang_getCString(c_type);
  if (strcmp(c_str, "int") == 0) {
    return "i32";
  }
  return NULL;
  clang_disposeString(c_type);
}

static enum CXChildVisitResult visit(CXCursor cursor, CXCursor parent,
                                     CXClientData data) {
  CodegenState *state = (CodegenState *)data;
  (void)parent;

  CXSourceLocation loc = clang_getCursorLocation(cursor);
  if (clang_Location_isInSystemHeader(loc)) {
    return CXChildVisit_Continue;
  }

  enum CXCursorKind kind = clang_getCursorKind(cursor);

  switch (kind) {
  case CXCursor_FunctionDecl: {
    CXString name = clang_getCursorSpelling(cursor);
    char *ret_type =
        rs_type(clang_getTypeSpelling(clang_getCursorResultType(cursor)));
    if (strcmp(clang_getCString(name), "main") == 0) {
      state->in_main = true;
      emit("fn main() {\n");
    } else {
      emit("fn %s() -> %s {\n", clang_getCString(name), ret_type);
    }
    clang_disposeString(name);

    clang_visitChildren(cursor, visit, data);

    emit("}\n");
    break;
  }
  case CXCursor_CallExpr: {
    CXString name = clang_getCursorSpelling(cursor);
    const char *fn = clang_getCString(name);
    if (strcmp(fn, "printf") == 0) {
      emit("println!(");
      clang_visitChildren(cursor, visit, data);
      emit(");\n");
    }
    clang_disposeString(name);
    return CXChildVisit_Continue;
  }
  case CXCursor_StringLiteral: {
    CXSourceRange range = clang_getCursorExtent(cursor);
    CXToken *tokens = NULL;
    unsigned num_tokens = 0;
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cursor);
    clang_tokenize(tu, range, &tokens, &num_tokens);
    if (num_tokens > 0) {
      CXString tok =
          clang_getTokenSpelling(tu, tokens[0]); // this will include quotes
      emit("%s", clang_getCString(tok));
      clang_disposeString(tok);
    }

    clang_disposeTokens(tu, tokens, num_tokens);
    return CXChildVisit_Continue;
  }
  case CXCursor_CompoundStmt: {
    clang_visitChildren(cursor, visit, data);
    return CXChildVisit_Continue;
    break;
  }
  case CXCursor_UnexposedExpr: {
    clang_visitChildren(cursor, visit, data);
    return CXChildVisit_Continue;
  }
  case CXCursor_ReturnStmt: {
    if (state->in_main) {
      return CXChildVisit_Continue;
    }
    emit("return ");
    clang_visitChildren(cursor, visit, data);
    emit(";\n");
    return CXChildVisit_Continue;
  }
  case CXCursor_IntegerLiteral: {
    CXSourceRange range = clang_getCursorExtent(cursor);
    CXToken *tokens = NULL;
    unsigned num_tokens = 0;
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cursor);
    clang_tokenize(tu, range, &tokens, &num_tokens);

    if (num_tokens > 0) {
      CXString tok = clang_getTokenSpelling(tu, tokens[0]);
      emit("%s", clang_getCString(tok));
      clang_disposeString(tok);
    }

    clang_disposeTokens(tu, tokens, num_tokens);
    return CXChildVisit_Continue;
  }
  }

  return CXChildVisit_Continue;
}

void codegen(const char *source_file, char **rust_file) {
  char template[] = "/tmp/rcc_XXXXXX.rs";
  int fd = mkstemps(template, 3);
  if (fd < 0) {
    perror("mkstemps");
    return;
  }

  out_fp = fdopen(fd, "w");
  if (!out_fp) {
    perror("fdopen");
    close(fd);
    return;
  }

  *rust_file = strdup(template);

  CXIndex index = clang_createIndex(0, 0);
  CXTranslationUnit tu = clang_parseTranslationUnit(
      index, source_file, NULL, 0, NULL, 0, CXTranslationUnit_None);

  if (!tu) {
    fprintf(stderr, "Failed to parse '%s'\n", source_file);
    fclose(out_fp);
    clang_disposeIndex(index);
    return;
  }

  CXCursor root = clang_getTranslationUnitCursor(tu);

  CodegenState state = {false};
  clang_visitChildren(root, visit, &state);

  fclose(out_fp);
  clang_disposeTranslationUnit(tu);
  clang_disposeIndex(index);
}
