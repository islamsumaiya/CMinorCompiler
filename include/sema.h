#ifndef CMINOR_SEMA_H
#define CMINOR_SEMA_H

#include "ast.h"

typedef enum { SYM_FUNC, SYM_PARAM, SYM_LOCAL } SymbolKind;

typedef struct {
    int id;
    SymbolKind kind;
    char *name;
    ValueType type;
    int scope_id;
    int parent_scope_id;
    SourceLoc loc;
    size_t param_count;
    bool has_definition;
    SourceLoc def_loc;
} Symbol;

typedef struct {
    int from_loc_line;
    int from_loc_col;
    char *ident_name;
    int target_sym_id;
} ResolutionRecord;

typedef struct {
    Symbol *syms;
    size_t sym_count;
    size_t sym_cap;
    ResolutionRecord *resolutions;
    size_t res_count;
    size_t res_cap;
} SymbolTable;

void sema_analyze(Program *prog, SymbolTable *st, DiagList *diags);
void symtab_free(SymbolTable *st);
void dump_symbols(const SymbolTable *st, FILE *out);

#endif
