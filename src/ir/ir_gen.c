#include "../../include/ir.h"

typedef struct { IRFunction *fn; const SymbolTable *st; int brk[64]; int cont[64]; int top; } IRB;

static IROperand op_none(void) { return (IROperand){OPERAND_NONE, 0, NULL}; }
static IROperand op_const(int32_t v) { return (IROperand){OPERAND_CONST, v, NULL}; }
static IROperand op_temp(int id) { return (IROperand){OPERAND_TEMP, id, NULL}; }
static IROperand op_var(const IRB *b, int sid) {
    return (IROperand){OPERAND_VAR, sid, (sid >= 0 && (size_t)sid < b->st->sym_count) ? cm_strdup(b->st->syms[sid].name) : NULL};
}

static void emit(IRB *b, IRInstr ins) {
    IRFunction *fn = b->fn;
    if (fn->code_count == fn->code_cap) { size_t nc = fn->code_cap ? fn->code_cap*2 : 64; fn->code = realloc(fn->code, nc*sizeof(IRInstr)); fn->code_cap = nc; }
    fn->code[fn->code_count++] = ins;
}

static void reg_slot(IRFunction *fn, int sid, const char *name, bool is_param) {
    for (size_t i = 0; i < fn->var_count; i++) if (fn->vars[i].sym_id == sid) return;
    if (fn->var_count == fn->var_cap) { size_t nc = fn->var_cap ? fn->var_cap*2 : 16; fn->vars = realloc(fn->vars, nc*sizeof(IRVarSlot)); fn->var_cap = nc; }
    fn->vars[fn->var_count++] = (IRVarSlot){sid, cm_strdup(name), is_param};
}

static void emit_lbl(IRB *b, int l) { IRInstr i = {0}; i.op = IR_LABEL; i.label_id = l; emit(b, i); }
static void emit_jmp(IRB *b, int l) { IRInstr i = {0}; i.op = IR_JMP; i.label_id = l; emit(b, i); }
static void emit_jz(IRB *b, IROperand c, int l) { IRInstr i = {0}; i.op = IR_JZ; i.src1 = c; i.label_id = l; emit(b, i); }
static void emit_jnz(IRB *b, IROperand c, int l) { IRInstr i = {0}; i.op = IR_JNZ; i.src1 = c; i.label_id = l; emit(b, i); }
static void emit_mov(IRB *b, IROperand d, IROperand s) { IRInstr i = {0}; i.op = IR_MOV; i.dst = d; i.src1 = s; emit(b, i); }

static IROperand gen_e(IRB *b, const Expr *e);
static void gen_s(IRB *b, const Stmt *s);

static IROperand gen_e(IRB *b, const Expr *e) {
    if (!e) return op_const(0);
    switch (e->kind) {
        case EXPR_INT_LIT: return op_const(e->as.int_val);
        case EXPR_IDENT: { int t = b->fn->next_temp++; emit_mov(b, op_temp(t), op_var(b, e->resolved_sym_id)); return op_temp(t); }
        case EXPR_PAREN: return gen_e(b, e->as.paren.inner);
        case EXPR_UNARY: {
            IROperand v = gen_e(b, e->as.unary.operand);
            if (e->as.unary.op == UOP_PLUS) return v;
            int t = b->fn->next_temp++; IRInstr i = {0};
            i.op = e->as.unary.op == UOP_MINUS ? IR_NEG : IR_NOT; i.dst = op_temp(t); i.src1 = v; emit(b, i); return op_temp(t);
        }
        case EXPR_BINARY: {
            BinOp op = e->as.binary.op;
            if (op == OP_ASSIGN) {
                IROperand r = gen_e(b, e->as.binary.rhs); int sid = e->as.binary.lhs->resolved_sym_id;
                emit_mov(b, op_var(b, sid), r); int t = b->fn->next_temp++; emit_mov(b, op_temp(t), op_var(b, sid)); return op_temp(t);
            }
            if (op == OP_AND) {
                int rt = b->fn->next_temp++, fl = b->fn->next_label++, el = b->fn->next_label++;
                IROperand l = gen_e(b, e->as.binary.lhs); emit_jz(b, l, fl);
                IROperand r = gen_e(b, e->as.binary.rhs); emit_jz(b, r, fl);
                emit_mov(b, op_temp(rt), op_const(1)); emit_jmp(b, el);
                emit_lbl(b, fl); emit_mov(b, op_temp(rt), op_const(0)); emit_lbl(b, el); return op_temp(rt);
            }
            if (op == OP_OR) {
                int rt = b->fn->next_temp++, tl = b->fn->next_label++, el = b->fn->next_label++;
                IROperand l = gen_e(b, e->as.binary.lhs); emit_jnz(b, l, tl);
                IROperand r = gen_e(b, e->as.binary.rhs); emit_jnz(b, r, tl);
                emit_mov(b, op_temp(rt), op_const(0)); emit_jmp(b, el);
                emit_lbl(b, tl); emit_mov(b, op_temp(rt), op_const(1)); emit_lbl(b, el); return op_temp(rt);
            }
            IROperand l = gen_e(b, e->as.binary.lhs), r = gen_e(b, e->as.binary.rhs);
            int t = b->fn->next_temp++; IRInstr i = {0};
            static const IROpcode map[] = {IR_ADD,IR_SUB,IR_MUL,IR_DIV,IR_MOD,IR_LT,IR_LE,IR_GT,IR_GE,IR_EQ,IR_NE};
            i.op = map[op]; i.dst = op_temp(t); i.src1 = l; i.src2 = r; emit(b, i); return op_temp(t);
        }
        case EXPR_CALL: {
            size_t ac = e->as.call.arg_count;
            if (ac > 0) {
                IROperand *av = malloc(ac * sizeof(IROperand));
                for (size_t k = 0; k < ac; k++) av[k] = gen_e(b, e->as.call.args[k]);
                for (size_t k = 0; k < ac; k++) { IRInstr ai = {0}; ai.op = IR_ARG; ai.src1 = av[k]; ai.arg_count = k; emit(b, ai); }
                free(av);
            }
            IRInstr ci = {0}; ci.op = IR_CALL; ci.callee = cm_strdup(e->as.call.callee); ci.arg_count = ac;
            if (e->eval_type == TYPE_INT) { int t = b->fn->next_temp++; ci.dst = op_temp(t); emit(b, ci); return op_temp(t); }
            ci.dst = op_none(); emit(b, ci); return op_none();
        }
    }
    return op_none();
}

static void gen_s(IRB *b, const Stmt *s) {
    if (!s) return;
    switch (s->kind) {
        case STMT_BLOCK: for (size_t i = 0; i < s->as.block.count; i++) gen_s(b, s->as.block.items[i]); break;
        case STMT_LOCAL_DECL:
            reg_slot(b->fn, s->as.local_decl.sym_id, s->as.local_decl.name, false);
            if (s->as.local_decl.init) { IROperand v = gen_e(b, s->as.local_decl.init); emit_mov(b, op_var(b, s->as.local_decl.sym_id), v); }
            break;
        case STMT_EXPR: if (s->as.expr_stmt.expr) { IROperand t = gen_e(b, s->as.expr_stmt.expr); free(t.var_name); } break;
        case STMT_IF: {
            int el = b->fn->next_label++, end = b->fn->next_label++; IROperand c = gen_e(b, s->as.if_stmt.cond);
            if (s->as.if_stmt.else_branch) { emit_jz(b, c, el); gen_s(b, s->as.if_stmt.then_branch); emit_jmp(b, end); emit_lbl(b, el); gen_s(b, s->as.if_stmt.else_branch); emit_lbl(b, end); }
            else { emit_jz(b, c, end); gen_s(b, s->as.if_stmt.then_branch); emit_lbl(b, end); }
            break;
        }
        case STMT_WHILE: {
            int cl = b->fn->next_label++, end = b->fn->next_label++;
            b->brk[b->top] = end; b->cont[b->top] = cl; b->top++;
            emit_lbl(b, cl); IROperand c = gen_e(b, s->as.while_stmt.cond); emit_jz(b, c, end);
            gen_s(b, s->as.while_stmt.body); emit_jmp(b, cl); emit_lbl(b, end); b->top--;
            break;
        }
        case STMT_FOR: {
            int cl = b->fn->next_label++, ul = b->fn->next_label++, end = b->fn->next_label++;
            if (s->as.for_stmt.init) gen_s(b, s->as.for_stmt.init);
            b->brk[b->top] = end; b->cont[b->top] = ul; b->top++;
            emit_lbl(b, cl);
            if (s->as.for_stmt.cond) { IROperand c = gen_e(b, s->as.for_stmt.cond); emit_jz(b, c, end); }
            gen_s(b, s->as.for_stmt.body); emit_lbl(b, ul);
            if (s->as.for_stmt.update) { IROperand u = gen_e(b, s->as.for_stmt.update); free(u.var_name); }
            emit_jmp(b, cl); emit_lbl(b, end); b->top--;
            break;
        }
        case STMT_BREAK: emit_jmp(b, b->brk[b->top - 1]); break;
        case STMT_CONTINUE: emit_jmp(b, b->cont[b->top - 1]); break;
        case STMT_RETURN: {
            IRInstr i = {0};
            if (s->as.return_stmt.value) { i.op = IR_RET; i.src1 = gen_e(b, s->as.return_stmt.value); }
            else i.op = IR_RET_VOID;
            emit(b, i); break;
        }
    }
}

IRProgram *ir_generate(const Program *prog, const SymbolTable *st) {
    IRProgram *irp = calloc(1, sizeof(IRProgram));
    size_t dc = 0; for (size_t i = 0; i < prog->func_count; i++) if (prog->funcs[i]->is_definition) dc++;
    irp->funcs = calloc(dc ? dc : 1, sizeof(IRFunction));
    for (size_t i = 0; i < prog->func_count; i++) {
        const FuncDecl *af = prog->funcs[i]; if (!af->is_definition) continue;
        IRFunction *fn = &irp->funcs[irp->func_count++];
        fn->name = cm_strdup(af->name); fn->ret_type = af->ret_type; fn->param_count = af->param_count;
        if (fn->param_count) {
            fn->param_sym_ids = malloc(fn->param_count * sizeof(int)); fn->param_names = malloc(fn->param_count * sizeof(char *));
            for (size_t p = 0; p < fn->param_count; p++) {
                fn->param_sym_ids[p] = af->params[p].sym_id; fn->param_names[p] = cm_strdup(af->params[p].name);
                reg_slot(fn, af->params[p].sym_id, af->params[p].name, true);
            }
        }
        IRB b = {fn, st, {0}, {0}, 0}; gen_s(&b, af->body);
        if (fn->ret_type == TYPE_VOID && (!fn->code_count || fn->code[fn->code_count-1].op != IR_RET_VOID)) {
            IRInstr r = {0}; r.op = IR_RET_VOID; emit(&b, r);
        }
    }
    return irp;
}

void ir_free(IRProgram *irp) {
    if (!irp) return;
    for (size_t i = 0; i < irp->func_count; i++) {
        IRFunction *fn = &irp->funcs[i]; free(fn->name);
        for (size_t p = 0; p < fn->param_count; p++) free(fn->param_names[p]);
        free(fn->param_names); free(fn->param_sym_ids);
        for (size_t v = 0; v < fn->var_count; v++) free(fn->vars[v].name);
        free(fn->vars);
        for (size_t c = 0; c < fn->code_count; c++) { free(fn->code[c].dst.var_name); free(fn->code[c].src1.var_name); free(fn->code[c].src2.var_name); free(fn->code[c].callee); }
        free(fn->code);
    }
    free(irp->funcs); free(irp);
}

static void fmt_op(IROperand o, char *b, size_t sz) {
    if (o.kind == OPERAND_CONST) snprintf(b, sz, "%d", o.val);
    else if (o.kind == OPERAND_TEMP) snprintf(b, sz, "t%d", o.val);
    else if (o.kind == OPERAND_VAR) snprintf(b, sz, "v%d(%s)", o.val, o.var_name ? o.var_name : "?");
    else snprintf(b, sz, "_");
}

void dump_ir(const IRProgram *irp, FILE *out) {
    for (size_t i = 0; i < irp->func_count; i++) {
        const IRFunction *fn = &irp->funcs[i];
        fprintf(out, "func %s %s(", fn->ret_type == TYPE_INT ? "int" : "void", fn->name);
        for (size_t p = 0; p < fn->param_count; p++) fprintf(out, "%sint v%d(%s)", p ? ", " : "", fn->param_sym_ids[p], fn->param_names[p]);
        fprintf(out, "):\n");
        for (size_t c = 0; c < fn->code_count; c++) {
            const IRInstr *ins = &fn->code[c]; char d[64], s1[64], s2[64];
            fmt_op(ins->dst, d, sizeof(d)); fmt_op(ins->src1, s1, sizeof(s1)); fmt_op(ins->src2, s2, sizeof(s2));
            switch (ins->op) {
                case IR_LABEL: fprintf(out, "  L%d:\n", ins->label_id); break;
                case IR_MOV: fprintf(out, "    %s = %s\n", d, s1); break;
                case IR_ADD: fprintf(out, "    %s = %s + %s\n", d, s1, s2); break;
                case IR_SUB: fprintf(out, "    %s = %s - %s\n", d, s1, s2); break;
                case IR_MUL: fprintf(out, "    %s = %s * %s\n", d, s1, s2); break;
                case IR_DIV: fprintf(out, "    %s = %s / %s\n", d, s1, s2); break;
                case IR_MOD: fprintf(out, "    %s = %s %% %s\n", d, s1, s2); break;
                case IR_NEG: fprintf(out, "    %s = -%s\n", d, s1); break;
                case IR_NOT: fprintf(out, "    %s = !%s\n", d, s1); break;
                case IR_LT: fprintf(out, "    %s = (%s < %s)\n", d, s1, s2); break;
                case IR_LE: fprintf(out, "    %s = (%s <= %s)\n", d, s1, s2); break;
                case IR_GT: fprintf(out, "    %s = (%s > %s)\n", d, s1, s2); break;
                case IR_GE: fprintf(out, "    %s = (%s >= %s)\n", d, s1, s2); break;
                case IR_EQ: fprintf(out, "    %s = (%s == %s)\n", d, s1, s2); break;
                case IR_NE: fprintf(out, "    %s = (%s != %s)\n", d, s1, s2); break;
                case IR_JMP: fprintf(out, "    jmp L%d\n", ins->label_id); break;
                case IR_JZ: fprintf(out, "    jz %s, L%d\n", s1, ins->label_id); break;
                case IR_JNZ: fprintf(out, "    jnz %s, L%d\n", s1, ins->label_id); break;
                case IR_ARG: fprintf(out, "    arg #%zu %s\n", ins->arg_count, s1); break;
                case IR_CALL: if (ins->dst.kind != OPERAND_NONE) fprintf(out, "    %s = call %s(%zu)\n", d, ins->callee, ins->arg_count); else fprintf(out, "    call %s(%zu)\n", ins->callee, ins->arg_count); break;
                case IR_RET: fprintf(out, "    ret %s\n", s1); break;
                case IR_RET_VOID: fprintf(out, "    ret\n"); break;
            }
        }
        fprintf(out, "end func %s\n\n", fn->name);
    }
}
