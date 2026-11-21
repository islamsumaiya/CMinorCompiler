#include "../../include/ast.h"

typedef struct {
    const TokenList *tl;
    size_t pos;
    DiagList *diags;
    int block_depth;
    int expr_depth;
    bool limit_reported;
} Parser;

static const Token *cur(Parser *p) {
    size_t max_idx = p->tl->count > 0 ? p->tl->count - 1 : 0;
    return &p->tl->data[p->pos <= max_idx ? p->pos : max_idx];
}

static const Token *peek(Parser *p, size_t offset) {
    size_t max_idx = p->tl->count > 0 ? p->tl->count - 1 : 0;
    return &p->tl->data[p->pos + offset <= max_idx ? p->pos + offset : max_idx];
}

static const Token *advance(Parser *p) {
    const Token *t = cur(p);
    if (t->kind != TOK_EOF) p->pos++;
    return t;
}

static bool check(Parser *p, TokenKind k) { return cur(p)->kind == k; }
static bool match(Parser *p, TokenKind k) { if (check(p, k)) { advance(p); return true; } return false; }

static bool expect(Parser *p, TokenKind k, const char *desc) {
    if (check(p, k)) { advance(p); return true; }
    diag_emit(p->diags, DIAG_SYNTAX_ERROR, cur(p)->loc, "expected %s, found '%s'", desc, cur(p)->lexeme);
    return false;
}

static void sync_stmt(Parser *p) {
    while (!check(p, TOK_EOF)) {
        if (check(p, TOK_SEMI)) { advance(p); return; }
        if (check(p, TOK_RBRACE) || check(p, TOK_KW_INT) || check(p, TOK_KW_IF) ||
            check(p, TOK_KW_WHILE) || check(p, TOK_KW_FOR) || check(p, TOK_KW_BREAK) ||
            check(p, TOK_KW_CONTINUE) || check(p, TOK_KW_RETURN) || check(p, TOK_LBRACE)) return;
        advance(p);
    }
}

static void sync_top(Parser *p) {
    while (!check(p, TOK_EOF)) {
        if ((check(p, TOK_KW_INT) || check(p, TOK_KW_VOID)) && peek(p, 1)->kind == TOK_IDENT && peek(p, 2)->kind == TOK_LPAREN) return;
        if (check(p, TOK_SEMI) || check(p, TOK_RBRACE)) { advance(p); return; }
        advance(p);
    }
}

static Expr *new_expr(ExprKind k, SourceLoc loc) {
    Expr *e = calloc(1, sizeof(Expr)); e->kind = k; e->loc = loc; e->eval_type = TYPE_ERROR; e->resolved_sym_id = -1; return e;
}

static Stmt *new_stmt(StmtKind k, SourceLoc loc) {
    Stmt *s = calloc(1, sizeof(Stmt)); s->kind = k; s->loc = loc; return s;
}

static Expr *parse_assignment_expr(Parser *p);
static Stmt *parse_block(Parser *p);
static Stmt *parse_statement(Parser *p);

static Expr *parse_primary(Parser *p) {
    const Token *t = cur(p);
    if (t->kind == TOK_INT_LIT) { advance(p); Expr *e = new_expr(EXPR_INT_LIT, t->loc); e->as.int_val = t->int_val; return e; }
    if (t->kind == TOK_IDENT) {
        advance(p);
        if (match(p, TOK_LPAREN)) {
            Expr *e = new_expr(EXPR_CALL, t->loc); e->as.call.callee = cm_strdup(t->lexeme); size_t cap = 0;
            if (!check(p, TOK_RPAREN)) {
                while (1) {
                    Expr *arg = parse_assignment_expr(p);
                    if (arg) {
                        if (e->as.call.arg_count == cap) { cap = cap ? cap * 2 : 4; e->as.call.args = realloc(e->as.call.args, cap * sizeof(Expr *)); }
                        e->as.call.args[e->as.call.arg_count++] = arg;
                    }
                    if (!match(p, TOK_COMMA)) break;
                }
            }
            expect(p, TOK_RPAREN, "')' after call arguments");
            return e;
        }
        Expr *e = new_expr(EXPR_IDENT, t->loc); e->as.ident.name = cm_strdup(t->lexeme); return e;
    }
    if (t->kind == TOK_LPAREN) {
        advance(p); Expr *inner = parse_assignment_expr(p); expect(p, TOK_RPAREN, "')'");
        Expr *e = new_expr(EXPR_PAREN, t->loc); e->as.paren.inner = inner; return e;
    }
    diag_emit(p->diags, DIAG_SYNTAX_ERROR, t->loc, "expected expression, found '%s'", t->lexeme);
    return NULL;
}

static Expr *parse_unary(Parser *p) {
    const Token *t = cur(p);
    if (t->kind == TOK_PLUS || t->kind == TOK_MINUS || t->kind == TOK_BANG) {
        advance(p);
        UnOp op = (t->kind == TOK_PLUS) ? UOP_PLUS : (t->kind == TOK_MINUS ? UOP_MINUS : UOP_NOT);
        Expr *e = new_expr(EXPR_UNARY, t->loc); e->as.unary.op = op; e->as.unary.operand = parse_unary(p); return e;
    }
    return parse_primary(p);
}

static Expr *parse_mul(Parser *p) {
    Expr *lhs = parse_unary(p);
    while (check(p, TOK_STAR) || check(p, TOK_SLASH) || check(p, TOK_PERCENT)) {
        const Token *op = advance(p);
        BinOp bop = (op->kind == TOK_STAR) ? OP_MUL : (op->kind == TOK_SLASH ? OP_DIV : OP_MOD);
        Expr *bin = new_expr(EXPR_BINARY, op->loc); bin->as.binary.op = bop; bin->as.binary.lhs = lhs; bin->as.binary.rhs = parse_unary(p); lhs = bin;
    }
    return lhs;
}

static Expr *parse_add(Parser *p) {
    Expr *lhs = parse_mul(p);
    while (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
        const Token *op = advance(p);
        Expr *bin = new_expr(EXPR_BINARY, op->loc); bin->as.binary.op = (op->kind == TOK_PLUS) ? OP_ADD : OP_SUB;
        bin->as.binary.lhs = lhs; bin->as.binary.rhs = parse_mul(p); lhs = bin;
    }
    return lhs;
}

static Expr *parse_rel(Parser *p) {
    Expr *lhs = parse_add(p);
    while (check(p, TOK_LT) || check(p, TOK_LE) || check(p, TOK_GT) || check(p, TOK_GE)) {
        const Token *op = advance(p);
        BinOp bop = (op->kind == TOK_LT) ? OP_LT : (op->kind == TOK_LE ? OP_LE : (op->kind == TOK_GT ? OP_GT : OP_GE));
        Expr *bin = new_expr(EXPR_BINARY, op->loc); bin->as.binary.op = bop; bin->as.binary.lhs = lhs; bin->as.binary.rhs = parse_add(p); lhs = bin;
    }
    return lhs;
}

static Expr *parse_eq(Parser *p) {
    Expr *lhs = parse_rel(p);
    while (check(p, TOK_EQ) || check(p, TOK_NE)) {
        const Token *op = advance(p);
        Expr *bin = new_expr(EXPR_BINARY, op->loc); bin->as.binary.op = (op->kind == TOK_EQ) ? OP_EQ : OP_NE;
        bin->as.binary.lhs = lhs; bin->as.binary.rhs = parse_rel(p); lhs = bin;
    }
    return lhs;
}

static Expr *parse_and(Parser *p) {
    Expr *lhs = parse_eq(p);
    while (check(p, TOK_AND_AND)) {
        const Token *op = advance(p);
        Expr *bin = new_expr(EXPR_BINARY, op->loc); bin->as.binary.op = OP_AND; bin->as.binary.lhs = lhs; bin->as.binary.rhs = parse_eq(p); lhs = bin;
    }
    return lhs;
}

static Expr *parse_or(Parser *p) {
    Expr *lhs = parse_and(p);
    while (check(p, TOK_OR_OR)) {
        const Token *op = advance(p);
        Expr *bin = new_expr(EXPR_BINARY, op->loc); bin->as.binary.op = OP_OR; bin->as.binary.lhs = lhs; bin->as.binary.rhs = parse_and(p); lhs = bin;
    }
    return lhs;
}

static Expr *parse_assignment_expr(Parser *p) {
    p->expr_depth++;
    if (p->expr_depth > MAX_EXPR_NESTING && !p->limit_reported) {
        diag_emit(p->diags, DIAG_LIMIT_ERROR, cur(p)->loc, "expression nesting exceeds %d", MAX_EXPR_NESTING);
        p->limit_reported = true;
    }
    Expr *lhs = parse_or(p);
    if (check(p, TOK_ASSIGN)) {
        const Token *op = advance(p);
        Expr *bin = new_expr(EXPR_BINARY, op->loc); bin->as.binary.op = OP_ASSIGN;
        bin->as.binary.lhs = lhs; bin->as.binary.rhs = parse_assignment_expr(p); lhs = bin;
    }
    p->expr_depth--;
    return lhs;
}

static Stmt *parse_local_decl(Parser *p) {
    const Token *kw = advance(p);
    const Token *id = cur(p);
    if (!check(p, TOK_IDENT)) {
        diag_emit(p->diags, DIAG_SYNTAX_ERROR, id->loc, "expected identifier in local declaration");
        sync_stmt(p); return NULL;
    }
    advance(p);
    Stmt *s = new_stmt(STMT_LOCAL_DECL, kw->loc);
    s->as.local_decl.name = cm_strdup(id->lexeme); s->as.local_decl.sym_id = -1;
    if (match(p, TOK_ASSIGN)) s->as.local_decl.init = parse_assignment_expr(p);
    if (!expect(p, TOK_SEMI, "';' after declaration")) sync_stmt(p);
    return s;
}

static Stmt *parse_statement(Parser *p) {
    const Token *t = cur(p);
    if (t->kind == TOK_KW_INT || t->kind == TOK_KW_VOID) {
        diag_emit(p->diags, DIAG_SYNTAX_ERROR, t->loc, "unbraced declaration not allowed as statement");
        sync_stmt(p); return NULL;
    }
    if (t->kind == TOK_LBRACE) return parse_block(p);
    if (t->kind == TOK_KW_IF) {
        advance(p); Stmt *s = new_stmt(STMT_IF, t->loc);
        expect(p, TOK_LPAREN, "'('"); s->as.if_stmt.cond = parse_assignment_expr(p); expect(p, TOK_RPAREN, "')'");
        s->as.if_stmt.then_branch = parse_statement(p);
        if (match(p, TOK_KW_ELSE)) s->as.if_stmt.else_branch = parse_statement(p);
        return s;
    }
    if (t->kind == TOK_KW_WHILE) {
        advance(p); Stmt *s = new_stmt(STMT_WHILE, t->loc);
        expect(p, TOK_LPAREN, "'('"); s->as.while_stmt.cond = parse_assignment_expr(p); expect(p, TOK_RPAREN, "')'");
        s->as.while_stmt.body = parse_statement(p); return s;
    }
    if (t->kind == TOK_KW_FOR) {
        advance(p); Stmt *s = new_stmt(STMT_FOR, t->loc); expect(p, TOK_LPAREN, "'('");
        if (check(p, TOK_KW_INT)) s->as.for_stmt.init = parse_local_decl(p);
        else if (!match(p, TOK_SEMI)) {
            Stmt *ie = new_stmt(STMT_EXPR, cur(p)->loc); ie->as.expr_stmt.expr = parse_assignment_expr(p);
            expect(p, TOK_SEMI, "';'"); s->as.for_stmt.init = ie;
        }
        if (!check(p, TOK_SEMI)) s->as.for_stmt.cond = parse_assignment_expr(p);
        expect(p, TOK_SEMI, "';'");
        if (!check(p, TOK_RPAREN)) s->as.for_stmt.update = parse_assignment_expr(p);
        expect(p, TOK_RPAREN, "')'"); s->as.for_stmt.body = parse_statement(p); return s;
    }
    if (t->kind == TOK_KW_BREAK) { advance(p); expect(p, TOK_SEMI, "';'"); return new_stmt(STMT_BREAK, t->loc); }
    if (t->kind == TOK_KW_CONTINUE) { advance(p); expect(p, TOK_SEMI, "';'"); return new_stmt(STMT_CONTINUE, t->loc); }
    if (t->kind == TOK_KW_RETURN) {
        advance(p); Stmt *s = new_stmt(STMT_RETURN, t->loc);
        if (!check(p, TOK_SEMI)) s->as.return_stmt.value = parse_assignment_expr(p);
        expect(p, TOK_SEMI, "';'"); return s;
    }
    if (match(p, TOK_SEMI)) return new_stmt(STMT_EXPR, t->loc);
    Stmt *s = new_stmt(STMT_EXPR, t->loc); s->as.expr_stmt.expr = parse_assignment_expr(p);
    if (!expect(p, TOK_SEMI, "';'")) sync_stmt(p);
    return s;
}

static Stmt *parse_block(Parser *p) {
    const Token *lb = cur(p);
    if (!expect(p, TOK_LBRACE, "'{'")) return NULL;
    p->block_depth++;
    if (p->block_depth > MAX_BLOCK_NESTING && !p->limit_reported) {
        diag_emit(p->diags, DIAG_LIMIT_ERROR, lb->loc, "block nesting exceeds %d", MAX_BLOCK_NESTING);
        p->limit_reported = true;
    }
    Stmt *blk = new_stmt(STMT_BLOCK, lb->loc); size_t cap = 0, locals = 0;
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        Stmt *item = NULL;
        if (check(p, TOK_KW_INT)) {
            if (++locals > MAX_LOCALS_PER_BLOCK) diag_emit(p->diags, DIAG_LIMIT_ERROR, cur(p)->loc, "locals exceed %d", MAX_LOCALS_PER_BLOCK);
            item = parse_local_decl(p);
        } else item = parse_statement(p);
        if (item) {
            if (blk->as.block.count == cap) { cap = cap ? cap * 2 : 8; blk->as.block.items = realloc(blk->as.block.items, cap * sizeof(Stmt *)); }
            blk->as.block.items[blk->as.block.count++] = item;
        }
    }
    expect(p, TOK_RBRACE, "'}'"); p->block_depth--; return blk;
}

static FuncDecl *parse_external(Parser *p) {
    const Token *rt = cur(p); ValueType rty = TYPE_INT;
    if (match(p, TOK_KW_INT)) rty = TYPE_INT;
    else if (match(p, TOK_KW_VOID)) rty = TYPE_VOID;
    else { diag_emit(p->diags, DIAG_SYNTAX_ERROR, rt->loc, "expected 'int' or 'void'"); sync_top(p); return NULL; }
    const Token *nm = cur(p);
    if (!check(p, TOK_IDENT)) { diag_emit(p->diags, DIAG_SYNTAX_ERROR, nm->loc, "expected function name"); sync_top(p); return NULL; }
    advance(p);
    if (!match(p, TOK_LPAREN)) { diag_emit(p->diags, DIAG_SYNTAX_ERROR, cur(p)->loc, "expected '(' (globals unsupported)"); sync_top(p); return NULL; }

    FuncDecl *fn = calloc(1, sizeof(FuncDecl));
    fn->ret_type = rty; fn->name = cm_strdup(nm->lexeme); fn->loc = nm->loc; fn->sym_id = -1;

    if (match(p, TOK_KW_VOID)) {
        if (!check(p, TOK_RPAREN)) { diag_emit(p->diags, DIAG_SYNTAX_ERROR, cur(p)->loc, "'void' must be sole parameter"); sync_top(p); free(fn->name); free(fn); return NULL; }
        advance(p);
    } else if (check(p, TOK_RPAREN)) {
        diag_emit(p->diags, DIAG_SYNTAX_ERROR, cur(p)->loc, "use '(void)' for zero-parameter functions"); advance(p);
    } else {
        size_t cap = 0;
        while (1) {
            const Token *pt = cur(p);
            if (!match(p, TOK_KW_INT)) { diag_emit(p->diags, DIAG_SYNTAX_ERROR, pt->loc, "expected 'int' parameter"); break; }
            ParamNode pr = {0}; pr.loc = pt->loc; pr.sym_id = -1;
            if (check(p, TOK_IDENT)) { const Token *id = advance(p); pr.name = cm_strdup(id->lexeme); pr.loc = id->loc; }
            if (fn->param_count == cap) { cap = cap ? cap * 2 : 4; fn->params = realloc(fn->params, cap * sizeof(ParamNode)); }
            fn->params[fn->param_count++] = pr;
            if (!match(p, TOK_COMMA)) break;
        }
        expect(p, TOK_RPAREN, "')'");
    }
    if (match(p, TOK_SEMI)) { fn->is_definition = false; return fn; }
    if (check(p, TOK_LBRACE)) {
        fn->is_definition = true;
        for (size_t i = 0; i < fn->param_count; i++)
            if (!fn->params[i].name) diag_emit(p->diags, DIAG_SYNTAX_ERROR, fn->params[i].loc, "unnamed parameter in definition");
        fn->body = parse_block(p); return fn;
    }
    diag_emit(p->diags, DIAG_SYNTAX_ERROR, cur(p)->loc, "expected ';' or '{'"); sync_top(p);
    for (size_t i = 0; i < fn->param_count; i++) free(fn->params[i].name);
    free(fn->params); free(fn->name); free(fn); return NULL;
}

Program *parse_tokens(const TokenList *tl, DiagList *diags) {
    Parser p = {tl, 0, diags, 0, 0, false};
    Program *prog = calloc(1, sizeof(Program)); size_t cap = 0;
    while (!check(&p, TOK_EOF)) {
        FuncDecl *fn = parse_external(&p);
        if (fn) {
            if (prog->func_count >= MAX_FUNCTIONS) diag_emit(diags, DIAG_LIMIT_ERROR, fn->loc, "functions exceed %d", MAX_FUNCTIONS);
            if (prog->func_count == cap) { cap = cap ? cap * 2 : 8; prog->funcs = realloc(prog->funcs, cap * sizeof(FuncDecl *)); }
            prog->funcs[prog->func_count++] = fn;
        }
    }
    return prog;
}

static void free_expr(Expr *e) {
    if (!e) return;
    if (e->kind == EXPR_IDENT) free(e->as.ident.name);
    else if (e->kind == EXPR_CALL) { free(e->as.call.callee); for (size_t i = 0; i < e->as.call.arg_count; i++) free_expr(e->as.call.args[i]); free(e->as.call.args); }
    else if (e->kind == EXPR_UNARY) free_expr(e->as.unary.operand);
    else if (e->kind == EXPR_BINARY) { free_expr(e->as.binary.lhs); free_expr(e->as.binary.rhs); }
    else if (e->kind == EXPR_PAREN) free_expr(e->as.paren.inner);
    free(e);
}

static void free_stmt(Stmt *s) {
    if (!s) return;
    if (s->kind == STMT_BLOCK) { for (size_t i = 0; i < s->as.block.count; i++) free_stmt(s->as.block.items[i]); free(s->as.block.items); }
    else if (s->kind == STMT_LOCAL_DECL) { free(s->as.local_decl.name); free_expr(s->as.local_decl.init); }
    else if (s->kind == STMT_EXPR) free_expr(s->as.expr_stmt.expr);
    else if (s->kind == STMT_IF) { free_expr(s->as.if_stmt.cond); free_stmt(s->as.if_stmt.then_branch); free_stmt(s->as.if_stmt.else_branch); }
    else if (s->kind == STMT_WHILE) { free_expr(s->as.while_stmt.cond); free_stmt(s->as.while_stmt.body); }
    else if (s->kind == STMT_FOR) { free_stmt(s->as.for_stmt.init); free_expr(s->as.for_stmt.cond); free_expr(s->as.for_stmt.update); free_stmt(s->as.for_stmt.body); }
    else if (s->kind == STMT_RETURN) free_expr(s->as.return_stmt.value);
    free(s);
}

void ast_free(Program *prog) {
    if (!prog) return;
    for (size_t i = 0; i < prog->func_count; i++) {
        FuncDecl *fn = prog->funcs[i]; free(fn->name);
        for (size_t j = 0; j < fn->param_count; j++) free(fn->params[j].name);
        free(fn->params); free_stmt(fn->body); free(fn);
    }
    free(prog->funcs); free(prog);
}

static void indent(FILE *out, int d) { for (int i = 0; i < d; i++) fputs("  ", out); }
static const char *bop_s(BinOp op) {
    static const char *n[] = {"+","-","*","/","%","<","<=",">",">=","==","!=","&&","||","="};
    return n[op];
}
static const char *uop_s(UnOp op) { static const char *n[] = {"+","-","!"}; return n[op]; }

static void dump_e(const Expr *e, FILE *out, int d) {
    indent(out, d);
    if (!e) { fprintf(out, "<null>\n"); return; }
    switch (e->kind) {
        case EXPR_INT_LIT: fprintf(out, "IntLit(%d) @%d:%d\n", e->as.int_val, e->loc.line, e->loc.col); break;
        case EXPR_IDENT: fprintf(out, "Ident(%s) @%d:%d\n", e->as.ident.name, e->loc.line, e->loc.col); break;
        case EXPR_PAREN: fprintf(out, "Paren @%d:%d\n", e->loc.line, e->loc.col); dump_e(e->as.paren.inner, out, d+1); break;
        case EXPR_UNARY: fprintf(out, "Unary(%s) @%d:%d\n", uop_s(e->as.unary.op), e->loc.line, e->loc.col); dump_e(e->as.unary.operand, out, d+1); break;
        case EXPR_BINARY: fprintf(out, "Binary(%s) @%d:%d\n", bop_s(e->as.binary.op), e->loc.line, e->loc.col); dump_e(e->as.binary.lhs, out, d+1); dump_e(e->as.binary.rhs, out, d+1); break;
        case EXPR_CALL: fprintf(out, "Call(%s) @%d:%d\n", e->as.call.callee, e->loc.line, e->loc.col); for (size_t i=0;i<e->as.call.arg_count;i++) dump_e(e->as.call.args[i], out, d+1); break;
    }
}

static void dump_s(const Stmt *s, FILE *out, int d) {
    indent(out, d);
    if (!s) { fprintf(out, "<null>\n"); return; }
    switch (s->kind) {
        case STMT_BLOCK: fprintf(out, "Block @%d:%d\n", s->loc.line, s->loc.col); for (size_t i=0;i<s->as.block.count;i++) dump_s(s->as.block.items[i], out, d+1); break;
        case STMT_LOCAL_DECL: fprintf(out, "LocalDecl(int %s) @%d:%d\n", s->as.local_decl.name, s->loc.line, s->loc.col); if (s->as.local_decl.init) dump_e(s->as.local_decl.init, out, d+1); break;
        case STMT_EXPR: fprintf(out, "ExprStmt @%d:%d\n", s->loc.line, s->loc.col); if (s->as.expr_stmt.expr) dump_e(s->as.expr_stmt.expr, out, d+1); break;
        case STMT_IF: fprintf(out, "IfStmt @%d:%d\n", s->loc.line, s->loc.col); dump_e(s->as.if_stmt.cond, out, d+1); dump_s(s->as.if_stmt.then_branch, out, d+1); if (s->as.if_stmt.else_branch) dump_s(s->as.if_stmt.else_branch, out, d+1); break;
        case STMT_WHILE: fprintf(out, "WhileStmt @%d:%d\n", s->loc.line, s->loc.col); dump_e(s->as.while_stmt.cond, out, d+1); dump_s(s->as.while_stmt.body, out, d+1); break;
        case STMT_FOR: fprintf(out, "ForStmt @%d:%d\n", s->loc.line, s->loc.col); if (s->as.for_stmt.init) dump_s(s->as.for_stmt.init, out, d+1); if (s->as.for_stmt.cond) dump_e(s->as.for_stmt.cond, out, d+1); if (s->as.for_stmt.update) dump_e(s->as.for_stmt.update, out, d+1); dump_s(s->as.for_stmt.body, out, d+1); break;
        case STMT_BREAK: fprintf(out, "Break @%d:%d\n", s->loc.line, s->loc.col); break;
        case STMT_CONTINUE: fprintf(out, "Continue @%d:%d\n", s->loc.line, s->loc.col); break;
        case STMT_RETURN: fprintf(out, "Return @%d:%d\n", s->loc.line, s->loc.col); if (s->as.return_stmt.value) dump_e(s->as.return_stmt.value, out, d+1); break;
    }
}

void dump_ast(const Program *prog, FILE *out) {
    fprintf(out, "Program(externals=%zu)\n", prog->func_count);
    for (size_t i = 0; i < prog->func_count; i++) {
        const FuncDecl *fn = prog->funcs[i];
        fprintf(out, "  %s(ret=%s, name=%s, params=%zu) @%d:%d\n", fn->is_definition ? "FunctionDef" : "FunctionDecl",
                fn->ret_type == TYPE_INT ? "int" : "void", fn->name, fn->param_count, fn->loc.line, fn->loc.col);
        for (size_t j = 0; j < fn->param_count; j++)
            fprintf(out, "    Param(int %s) @%d:%d\n", fn->params[j].name ? fn->params[j].name : "_", fn->params[j].loc.line, fn->params[j].loc.col);
        if (fn->body) dump_s(fn->body, out, 2);
    }
}
