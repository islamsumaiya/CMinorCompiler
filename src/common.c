#include "../include/common.h"
#include <stdarg.h>

void diag_init(DiagList *dl) { memset(dl, 0, sizeof(*dl)); }
void diag_free(DiagList *dl) { free(dl->items); memset(dl, 0, sizeof(*dl)); }

const char *diag_cat_name(DiagCategory cat) {
    switch (cat) {
        case DIAG_LEXICAL_ERROR: return "LEXICAL_ERROR";
        case DIAG_SYNTAX_ERROR: return "SYNTAX_ERROR";
        case DIAG_NAME_ERROR: return "NAME_ERROR";
        case DIAG_TYPE_ERROR: return "TYPE_ERROR";
        case DIAG_CALL_ERROR: return "CALL_ERROR";
        case DIAG_RETURN_ERROR: return "RETURN_ERROR";
        case DIAG_CONTROL_FLOW_ERROR: return "CONTROL_FLOW_ERROR";
        case DIAG_UNSUPPORTED_FEATURE: return "UNSUPPORTED_FEATURE";
        case DIAG_UNSUPPORTED_PREPROCESSING_DIRECTIVE: return "UNSUPPORTED_PREPROCESSING_DIRECTIVE";
        case DIAG_UNSUPPORTED_LINE_SPLICING: return "UNSUPPORTED_LINE_SPLICING";
        case DIAG_LIMIT_ERROR: return "LIMIT_ERROR";
        case DIAG_IO_ERROR: return "IO_ERROR";
        case DIAG_BACKEND_ERROR: return "BACKEND_ERROR";
        case DIAG_COMMAND_ERROR: return "COMMAND_ERROR";
        default: return "COMPILER_ERROR";
    }
}

static void diag_push(DiagList *dl, Diagnostic d) {
    dl->had_error = true;
    if (dl->count >= 64) return;
    if (dl->count == dl->cap) {
        size_t ncap = dl->cap ? dl->cap * 2 : 16;
        dl->items = realloc(dl->items, ncap * sizeof(Diagnostic));
        dl->cap = ncap;
    }
    dl->items[dl->count++] = d;
    const char *f = d.loc.file ? d.loc.file : "<command>";
    if (d.loc.line > 0)
        fprintf(stderr, "%s:%d:%d: error [%s]: %s\n", f, d.loc.line, d.loc.col, diag_cat_name(d.cat), d.message);
    else
        fprintf(stderr, "%s: error [%s]: %s\n", f, diag_cat_name(d.cat), d.message);
    if (d.has_related && d.related_loc.line > 0)
        fprintf(stderr, "%s:%d:%d: note: %s\n", d.related_loc.file ? d.related_loc.file : f,
                d.related_loc.line, d.related_loc.col, d.related_msg);
}

void diag_emit(DiagList *dl, DiagCategory cat, SourceLoc loc, const char *fmt, ...) {
    Diagnostic d = {0}; d.cat = cat; d.loc = loc;
    va_list ap; va_start(ap, fmt); vsnprintf(d.message, sizeof(d.message), fmt, ap); va_end(ap);
    diag_push(dl, d);
}

void diag_emit_related(DiagList *dl, DiagCategory cat, SourceLoc loc, SourceLoc rel, const char *rel_msg, const char *fmt, ...) {
    Diagnostic d = {0}; d.cat = cat; d.loc = loc; d.has_related = true; d.related_loc = rel;
    snprintf(d.related_msg, sizeof(d.related_msg), "%s", rel_msg ? rel_msg : "previous declaration here");
    va_list ap; va_start(ap, fmt); vsnprintf(d.message, sizeof(d.message), fmt, ap); va_end(ap);
    diag_push(dl, d);
}

void diag_emit_tool(DiagList *dl, DiagCategory cat, const char *fmt, ...) {
    Diagnostic d = {0}; d.cat = cat;
    va_list ap; va_start(ap, fmt); vsnprintf(d.message, sizeof(d.message), fmt, ap); va_end(ap);
    diag_push(dl, d);
}

char *cm_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = malloc(n + 1);
    if (!p) { perror("malloc"); exit(1); }
    return memcpy(p, s, n + 1);
}

char *cm_strndup(const char *s, size_t n) {
    char *p = malloc(n + 1);
    if (!p) { perror("malloc"); exit(1); }
    memcpy(p, s, n); p[n] = '\0';
    return p;
}
