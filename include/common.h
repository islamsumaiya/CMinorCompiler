#ifndef CMINOR_COMMON_H
#define CMINOR_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMINOR_VERSION "1.0.0"
#define MAX_INPUT_BYTES (32 * 1024)
#define MAX_TOKENS 4096
#define MAX_BLOCK_NESTING 16
#define MAX_EXPR_NESTING 16
#define MAX_FUNCTIONS 32
#define MAX_PARAMS 6
#define MAX_LOCALS_PER_BLOCK 64
#define MAX_LINE_LEN 2048

typedef struct {
    const char *file;
    int line;
    int col;
} SourceLoc;

typedef enum {
    DIAG_LEXICAL_ERROR,
    DIAG_SYNTAX_ERROR,
    DIAG_NAME_ERROR,
    DIAG_TYPE_ERROR,
    DIAG_CALL_ERROR,
    DIAG_RETURN_ERROR,
    DIAG_CONTROL_FLOW_ERROR,
    DIAG_UNSUPPORTED_FEATURE,
    DIAG_UNSUPPORTED_PREPROCESSING_DIRECTIVE,
    DIAG_UNSUPPORTED_LINE_SPLICING,
    DIAG_LIMIT_ERROR,
    DIAG_IO_ERROR,
    DIAG_BACKEND_ERROR,
    DIAG_COMMAND_ERROR
} DiagCategory;

typedef struct {
    DiagCategory cat;
    SourceLoc loc;
    char message[512];
    bool has_related;
    SourceLoc related_loc;
    char related_msg[256];
} Diagnostic;

typedef struct {
    Diagnostic *items;
    size_t count;
    size_t cap;
    bool had_error;
} DiagList;

void diag_init(DiagList *dl);
void diag_free(DiagList *dl);
void diag_emit(DiagList *dl, DiagCategory cat, SourceLoc loc, const char *fmt, ...);
void diag_emit_related(DiagList *dl, DiagCategory cat, SourceLoc loc, SourceLoc rel, const char *rel_msg, const char *fmt, ...);
void diag_emit_tool(DiagList *dl, DiagCategory cat, const char *fmt, ...);
const char *diag_cat_name(DiagCategory cat);

char *cm_strdup(const char *s);
char *cm_strndup(const char *s, size_t n);

#endif
