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

typedef struct {
  CXCursor children[3];
  int count;
} ChildCollector;

static enum CXChildVisitResult
collect_children(CXCursor cursor, CXCursor parent, CXClientData data) {
  (void)parent;
  ChildCollector *col = (ChildCollector *)data;
  col->children[col->count++] = cursor;
  return CXChildVisit_Continue;
}

static FILE *out_fp;

static void emit(const char *fmt, ...) {
  va_list args, args2;
  va_start(args, fmt);
  va_copy(args2, args);
  vfprintf(out_fp, fmt, args);
  vfprintf(
      stdout, fmt,
      args2); // write to stdout for debugging purposes, remove this in future

  va_end(args);
}
static const struct {
  const char *c;
  const char *rs;
} TYPE_MAP[] = {{"int", "i32"},
                {"int32_t", "i32"},
                {"unsigned int", "u32"},
                {"uint32_t", "u32"},
                {"int8_t", "i8"},
                {"uint8_t", "u8"},
                {"int16_t", "i16"},
                {"uint16_t", "u16"},
                {"int64_t", "i64"},
                {"long long", "i64"},
                {"uint64_t", "u64"},
                {"unsigned long long", "u64"},
                {"__int128", "i128"},
                {"unsigned __int128", "u128"},
                {"size_t", "usize"},
                {"ptrdiff_t", "isize"},
                {"ssize_t", "isize"},
                {"float", "f32"},
                {"double", "f64"},
                {"long double", "f64"},
                {"char", "i8"},
                {"unsigned char", "u8"},
                {"bool", "u8"},
                {"_Bool", "u8"},
                {NULL, NULL}};

char *rs_type(CXString c_type) {
  const char *c_str = clang_getCString(c_type);
  for (int i = 0; TYPE_MAP[i].c != NULL; i++) {
    if (strcmp(c_str, TYPE_MAP[i].c) == 0) {
      clang_disposeString(c_type);
      return TYPE_MAP[i].rs;
    }
  }
  clang_disposeString(c_type);
  return NULL;
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
    if (ret_type == NULL) {
      printf("Not implemented yet.");
      exit(EXIT_FAILURE);
    }
    char func_args[1024] = "";
    int num_args = clang_Cursor_getNumArguments(cursor);
    for (int i = 0; i < num_args; i++) {
      CXCursor arg = clang_Cursor_getArgument(cursor, i);
      CXString arg_name = clang_getCursorSpelling(arg);
      const char *arg_name_str = clang_getCString(arg_name);
      char *arg_type = rs_type(clang_getTypeSpelling(clang_getCursorType(arg)));
      char arg_str[256];
      snprintf(arg_str, 256, "%s: %s", arg_name_str,
               arg_type); // should probably increase this from 256
      strcat(func_args, arg_str);
      if (i < num_args - 1)
        strcat(func_args, ", ");
      clang_disposeString(arg_name);
    }
    if (strcmp(clang_getCString(name), "main") == 0) {
      state->in_main = true;
      emit("fn main() {\n");
    } else {
      emit("fn %s(%s) -> %s {\n", clang_getCString(name), func_args, ret_type);
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
  case CXCursor_IfStmt: {
    emit("if ");
    ChildCollector col = {.count = 0};
    clang_visitChildren(cursor, collect_children, &col);

    // condition bit
    CXCursor cond = col.children[0];
    visit(cond, cursor, data);

    emit("{\n");
    CXCursor then = col.children[1];
    clang_visitChildren(then, visit, data);

    if (col.count == 3) {
      emit("} ");
      CXCursor else_bdy = col.children[2]; // dont shadow C builtin else keyword
      if (clang_getCursorKind(else_bdy) == CXCursor_IfStmt) {
        emit("else ");
        visit(else_bdy, cursor, data);
      } else {
        emit("else {\n");
        clang_visitChildren(else_bdy, visit, data);
        emit("}\n");
      }
    } else {
      emit("}\n");
    }
    break;
  }
  case CXCursor_UnaryOperator: {
    CXSourceRange range = clang_getCursorExtent(cursor);
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cursor);
    CXToken *tokens = NULL;
    unsigned num_tokens = 0;
    clang_tokenize(tu, range, &tokens, &num_tokens);
    if (num_tokens > 0) {
      CXString op = clang_getTokenSpelling(tu, tokens[0]);
      emit("%s", clang_getCString(op));
      clang_disposeString(op);
    }
    clang_disposeTokens(tu, tokens, num_tokens);
    clang_visitChildren(cursor, visit, data);
    return CXChildVisit_Continue;
  }
  case CXCursor_BinaryOperator: {
    ChildCollector col = {.count = 0};
    clang_visitChildren(cursor, collect_children, &col);

    // left side
    visit(col.children[0], cursor, data);

    CXSourceRange left_range = clang_getCursorExtent(col.children[0]);
    CXSourceRange right_range = clang_getCursorExtent(col.children[1]);
    CXSourceLocation op_start = clang_getRangeEnd(left_range);
    CXSourceLocation op_end = clang_getRangeStart(right_range);
    CXSourceRange op_range = clang_getRange(op_start, op_end);

    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cursor);
    CXToken *tokens = NULL;
    unsigned num_tokens = 0;
    clang_tokenize(tu, op_range, &tokens, &num_tokens);

    if (num_tokens > 0) {
      CXString op = clang_getTokenSpelling(tu, tokens[0]);
      emit(" %s ", clang_getCString(op));
      clang_disposeString(op);
    }
    clang_disposeTokens(tu, tokens, num_tokens);

    // right
    visit(col.children[1], cursor, data);
    return CXChildVisit_Continue;
  }
  case CXCursor_DeclRefExpr: {
    CXString name = clang_getCursorSpelling(cursor);
    emit("%s", clang_getCString(name));
    clang_disposeString(name);
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
  const char *args[] = {"-std=c11"};
  CXTranslationUnit tu = clang_parseTranslationUnit(
      index, source_file, args, 1, NULL, 0, CXTranslationUnit_None);

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
