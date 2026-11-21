#include "../../include/sema.h"

typedef struct Scope { int id; struct Scope *parent; int *sym_ids; size_t count; size_t cap; } Scope;
typedef struct { SymbolTable *st; DiagList *diags; Scope *cur; int next_scope; FuncDecl *fn; int loop_depth; } SemaCtx;

static int add_sym(SymbolTable *st, Symbol s) {
    if (st->sym_count == st->sym_cap) { size_t nc = st->sym_cap ? st->sym_cap*2 : 32; st->syms = realloc(st->syms, nc*sizeof(Symbol)); st->sym_cap = nc; }
    s.id = (int)st->sym_count; st->syms[st->sym_count++] = s; return s.id;
}

static void add_res(SymbolTable *st, SourceLoc loc, const char *name, int sid) {
    if (st->res_count == st->res_cap) { size_t nc = st->res_cap ? st->res_cap*2 : 64; st->resolutions = realloc(st->resolutions, nc*sizeof(ResolutionRecord)); st->res_cap = nc; }
    st->resolutions[st->res_count++] = (ResolutionRecord){loc.line, loc.col, cm_strdup(name), sid};
}

static Scope *push_sc(SemaCtx *c) { Scope *s = calloc(1, sizeof(Scope)); s->id = c->next_scope++; s->parent = c->cur; c->cur = s; return s; }
static void pop_sc(SemaCtx *c) { Scope *s = c->cur; if (!s) return; c->cur = s->parent; free(s->sym_ids); free(s); }

static int lookup_sc(const SemaCtx *c, const Scope *s, const char *name) {
    for (size_t i = 0; i < s->count; i++) if (!strcmp(c->st->syms[s->sym_ids[i]].name, name)) return s->sym_ids[i];
    return -1;
}

static int lookup_lex(const SemaCtx *c, const char *name) {
    for (Scope *s = c->cur; s; s = s->parent) { int id = lookup_sc(c, s, name); if (id >= 0) return id; }
    return -1;
}

static void ins_sc(Scope *s, int id) {
    if (s->count == s->cap) { size_t nc = s->cap ? s->cap*2 : 8; s->sym_ids = realloc(s->sym_ids, nc*sizeof(int)); s->cap = nc; }
    s->sym_ids[s->count++] = id;
}

static void check_expr(SemaCtx *c, Expr *e);
static void check_stmt(SemaCtx *c, Stmt *s);

static void check_expr(SemaCtx *c, Expr *e) {
    if (!e) return;
    switch (e->kind) {
        case EXPR_INT_LIT: e->eval_type = TYPE_INT; e->is_lvalue = false; break;
        case EXPR_IDENT: {
            int sid = lookup_lex(c, e->as.ident.name);
            if (sid < 0) { diag_emit(c->diags, DIAG_NAME_ERROR, e->loc, "undeclared identifier '%s'", e->as.ident.name); e->eval_type = TYPE_ERROR; }
            else {
                e->resolved_sym_id = sid; add_res(c->st, e->loc, e->as.ident.name, sid);
                if (c->st->syms[sid].kind == SYM_FUNC) { diag_emit(c->diags, DIAG_TYPE_ERROR, e->loc, "function used as value"); e->eval_type = TYPE_ERROR; }
                else { e->eval_type = TYPE_INT; e->is_lvalue = true; }
            }
            break;
        }
        case EXPR_PAREN:
            check_expr(c, e->as.paren.inner);
            if (e->as.paren.inner) { e->eval_type = e->as.paren.inner->eval_type; e->is_lvalue = e->as.paren.inner->is_lvalue; e->resolved_sym_id = e->as.paren.inner->resolved_sym_id; }
            break;
        case EXPR_UNARY:
            check_expr(c, e->as.unary.operand); e->is_lvalue = false;
            if (e->as.unary.operand && e->as.unary.operand->eval_type == TYPE_VOID) diag_emit(c->diags, DIAG_TYPE_ERROR, e->loc, "void operand");
            else if (e->as.unary.operand && e->as.unary.operand->eval_type == TYPE_INT) e->eval_type = TYPE_INT;
            break;
        case EXPR_BINARY:
            check_expr(c, e->as.binary.lhs); check_expr(c, e->as.binary.rhs); e->is_lvalue = false;
            if (!e->as.binary.lhs || !e->as.binary.rhs) break;
            if (e->as.binary.op == OP_ASSIGN) {
                if (e->as.binary.lhs->eval_type == TYPE_ERROR || e->as.binary.rhs->eval_type == TYPE_ERROR) break;
                if (!e->as.binary.lhs->is_lvalue) diag_emit(c->diags, DIAG_TYPE_ERROR, e->loc, "assignment target must be modifiable int lvalue");
                else if (e->as.binary.rhs->eval_type != TYPE_INT) diag_emit(c->diags, DIAG_TYPE_ERROR, e->loc, "assignment rhs must be int");
                else e->eval_type = TYPE_INT;
            } else {
                if (e->as.binary.lhs->eval_type == TYPE_VOID || e->as.binary.rhs->eval_type == TYPE_VOID) diag_emit(c->diags, DIAG_TYPE_ERROR, e->loc, "void operand in binary expr");
                else if (e->as.binary.lhs->eval_type == TYPE_INT && e->as.binary.rhs->eval_type == TYPE_INT) e->eval_type = TYPE_INT;
            }
            break;
        case EXPR_CALL: {
            e->is_lvalue = false;
            for (size_t i = 0; i < e->as.call.arg_count; i++) {
                check_expr(c, e->as.call.args[i]);
                if (e->as.call.args[i] && e->as.call.args[i]->eval_type == TYPE_VOID) diag_emit(c->diags, DIAG_TYPE_ERROR, e->as.call.args[i]->loc, "void argument");
            }
            int sid = lookup_lex(c, e->as.call.callee);
            if (sid < 0) { diag_emit(c->diags, DIAG_NAME_ERROR, e->loc, "undeclared function '%s'", e->as.call.callee); break; }
            Symbol *sym = &c->st->syms[sid]; e->resolved_sym_id = sid; add_res(c->st, e->loc, e->as.call.callee, sid);
            if (sym->kind != SYM_FUNC) diag_emit(c->diags, DIAG_CALL_ERROR, e->loc, "'%s' is not a function", e->as.call.callee);
            else if (e->as.call.arg_count != sym->param_count) diag_emit(c->diags, DIAG_CALL_ERROR, e->loc, "argument count mismatch for '%s'", e->as.call.callee);
            else e->eval_type = sym->type;
            break;
        }
    }
}

static void check_stmt(SemaCtx *c, Stmt *s) {
    if (!s) return;
    switch (s->kind) {
        case STMT_BLOCK: push_sc(c); for (size_t i = 0; i < s->as.block.count; i++) check_stmt(c, s->as.block.items[i]); pop_sc(c); break;
        case STMT_LOCAL_DECL: {
            if (s->as.local_decl.init) { check_expr(c, s->as.local_decl.init); if (s->as.local_decl.init->eval_type == TYPE_VOID) diag_emit(c->diags, DIAG_TYPE_ERROR, s->loc, "void initializer"); }
            int ex = lookup_sc(c, c->cur, s->as.local_decl.name);
            if (ex >= 0) diag_emit_related(c->diags, DIAG_NAME_ERROR, s->loc, c->st->syms[ex].loc, "previous declaration", "redeclaration of '%s'", s->as.local_decl.name);
            else {
                Symbol sym = {0}; sym.kind = SYM_LOCAL; sym.name = cm_strdup(s->as.local_decl.name); sym.type = TYPE_INT;
                sym.scope_id = c->cur->id; sym.parent_scope_id = c->cur->parent ? c->cur->parent->id : -1; sym.loc = s->loc;
                int id = add_sym(c->st, sym); ins_sc(c->cur, id); s->as.local_decl.sym_id = id;
            }
            break;
        }
        case STMT_EXPR: if (s->as.expr_stmt.expr) check_expr(c, s->as.expr_stmt.expr); break;
        case STMT_IF:
            if (s->as.if_stmt.cond) { check_expr(c, s->as.if_stmt.cond); if (s->as.if_stmt.cond->eval_type == TYPE_VOID) diag_emit(c->diags, DIAG_TYPE_ERROR, s->loc, "void condition"); }
            check_stmt(c, s->as.if_stmt.then_branch); if (s->as.if_stmt.else_branch) check_stmt(c, s->as.if_stmt.else_branch);
            break;
        case STMT_WHILE:
            if (s->as.while_stmt.cond) { check_expr(c, s->as.while_stmt.cond); if (s->as.while_stmt.cond->eval_type == TYPE_VOID) diag_emit(c->diags, DIAG_TYPE_ERROR, s->loc, "void condition"); }
            c->loop_depth++; check_stmt(c, s->as.while_stmt.body); c->loop_depth--;
            break;
        case STMT_FOR:
            push_sc(c);
            if (s->as.for_stmt.init) check_stmt(c, s->as.for_stmt.init);
            if (s->as.for_stmt.cond) { check_expr(c, s->as.for_stmt.cond); if (s->as.for_stmt.cond->eval_type == TYPE_VOID) diag_emit(c->diags, DIAG_TYPE_ERROR, s->loc, "void condition"); }
            if (s->as.for_stmt.update) check_expr(c, s->as.for_stmt.update);
            c->loop_depth++; check_stmt(c, s->as.for_stmt.body); c->loop_depth--;
            pop_sc(c);
            break;
        case STMT_BREAK: if (c->loop_depth <= 0) diag_emit(c->diags, DIAG_CONTROL_FLOW_ERROR, s->loc, "'break' outside loop"); break;
        case STMT_CONTINUE: if (c->loop_depth <= 0) diag_emit(c->diags, DIAG_CONTROL_FLOW_ERROR, s->loc, "'continue' outside loop"); break;
        case STMT_RETURN:
            if (c->fn->ret_type == TYPE_VOID) {
                if (s->as.return_stmt.value) { check_expr(c, s->as.return_stmt.value); diag_emit(c->diags, DIAG_RETURN_ERROR, s->loc, "void function cannot return value"); }
            } else {
                if (!s->as.return_stmt.value) diag_emit(c->diags, DIAG_RETURN_ERROR, s->loc, "int function must return value");
                else { check_expr(c, s->as.return_stmt.value); if (s->as.return_stmt.value->eval_type == TYPE_VOID) diag_emit(c->diags, DIAG_RETURN_ERROR, s->loc, "cannot return void expr"); }
            }
            break;
    }
}

void sema_analyze(Program *prog, SymbolTable *st, DiagList *diags) {
    memset(st, 0, sizeof(*st));
    SemaCtx c = {st, diags, NULL, 0, NULL, 0};
    Scope *file_sc = push_sc(&c);
    bool found_main = false;

    for (size_t i = 0; i < prog->func_count; i++) {
        FuncDecl *fn = prog->funcs[i];
        if (fn->param_count > MAX_PARAMS) diag_emit(diags, DIAG_LIMIT_ERROR, fn->loc, "too many parameters");
        if (!strcmp(fn->name, "main")) {
            if (fn->ret_type != TYPE_INT || fn->param_count != 0) diag_emit(diags, DIAG_TYPE_ERROR, fn->loc, "main must be 'int main(void)'");
            if (fn->is_definition) { if (found_main) diag_emit(diags, DIAG_NAME_ERROR, fn->loc, "duplicate main"); found_main = true; }
        }
        int ex = lookup_sc(&c, file_sc, fn->name);
        if (ex < 0) {
            Symbol sym = {0}; sym.kind = SYM_FUNC; sym.name = cm_strdup(fn->name); sym.type = fn->ret_type;
            sym.scope_id = file_sc->id; sym.parent_scope_id = -1; sym.loc = fn->loc; sym.param_count = fn->param_count;
            sym.has_definition = fn->is_definition; if (fn->is_definition) sym.def_loc = fn->loc;
            fn->sym_id = add_sym(st, sym); ins_sc(file_sc, fn->sym_id);
        } else {
            Symbol *prev = &st->syms[ex]; fn->sym_id = ex;
            if (prev->type != fn->ret_type || prev->param_count != fn->param_count) diag_emit(diags, DIAG_TYPE_ERROR, fn->loc, "conflicting signature for '%s'", fn->name);
            if (fn->is_definition) {
                if (prev->has_definition) diag_emit(diags, DIAG_NAME_ERROR, fn->loc, "redefinition of '%s'", fn->name);
                else { prev->has_definition = true; prev->def_loc = fn->loc; }
            }
        }
        if (fn->is_definition) {
            c.fn = fn; c.loop_depth = 0; Scope *psc = push_sc(&c);
            for (size_t p = 0; p < fn->param_count; p++) {
                if (!fn->params[p].name) continue;
                if (lookup_sc(&c, psc, fn->params[p].name) >= 0) diag_emit(diags, DIAG_NAME_ERROR, fn->params[p].loc, "duplicate param '%s'", fn->params[p].name);
                else {
                    Symbol ps = {0}; ps.kind = SYM_PARAM; ps.name = cm_strdup(fn->params[p].name); ps.type = TYPE_INT;
                    ps.scope_id = psc->id; ps.parent_scope_id = file_sc->id; ps.loc = fn->params[p].loc;
                    fn->params[p].sym_id = add_sym(st, ps); ins_sc(psc, fn->params[p].sym_id);
                }
            }
            if (fn->body && fn->body->kind == STMT_BLOCK) {
                for (size_t b = 0; b < fn->body->as.block.count; b++) check_stmt(&c, fn->body->as.block.items[b]);
                if (fn->ret_type == TYPE_INT) {
                    size_t cnt = fn->body->as.block.count;
                    if (cnt == 0 || fn->body->as.block.items[cnt - 1]->kind != STMT_RETURN)
                        diag_emit(diags, DIAG_RETURN_ERROR, fn->loc, "int function '%s' must end with top-level return", fn->name);
                }
            }
            pop_sc(&c); c.fn = NULL;
        }
    }
    if (!found_main) { SourceLoc loc = {prog->func_count ? prog->funcs[0]->loc.file : "<input>", 1, 1}; diag_emit(diags, DIAG_NAME_ERROR, loc, "missing 'int main(void)'"); }
    for (size_t i = 0; i < st->sym_count; i++)
        if (st->syms[i].kind == SYM_FUNC && !st->syms[i].has_definition) diag_emit(diags, DIAG_NAME_ERROR, st->syms[i].loc, "undefined function '%s'", st->syms[i].name);
    pop_sc(&c);
}

void symtab_free(SymbolTable *st) {
    for (size_t i = 0; i < st->sym_count; i++) free(st->syms[i].name);
    free(st->syms);
    for (size_t i = 0; i < st->res_count; i++) free(st->resolutions[i].ident_name);
    free(st->resolutions);
    memset(st, 0, sizeof(*st));
}

void dump_symbols(const SymbolTable *st, FILE *out) {
    fprintf(out, "=== SYMBOL TABLE (symbols=%zu) ===\n", st->sym_count);
    for (size_t i = 0; i < st->sym_count; i++) {
        const Symbol *s = &st->syms[i];
        const char *k = s->kind == SYM_FUNC ? "FUNC" : (s->kind == SYM_PARAM ? "PARAM" : "LOCAL");
        const char *t = s->type == TYPE_INT ? "int" : "void";
        fprintf(out, "sym#%d | %-5s | name=%-12s | type=%-4s | scope=%d (parent=%d) | loc=%d:%d\n",
                s->id, k, s->name, t, s->scope_id, s->parent_scope_id, s->loc.line, s->loc.col);
    }
    fprintf(out, "\n=== NAME RESOLUTIONS (uses=%zu) ===\n", st->res_count);
    for (size_t i = 0; i < st->res_count; i++) {
        const ResolutionRecord *r = &st->resolutions[i];
        fprintf(out, "use @%d:%d '%s' -> sym#%d\n", r->from_loc_line, r->from_loc_col, r->ident_name, r->target_sym_id);
    }
}
