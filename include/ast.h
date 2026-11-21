#ifndef CMINOR_AST_H
#define CMINOR_AST_H

#include "common.h"
#include "lexer.h"

typedef enum { TYPE_VOID = 0, TYPE_INT = 1, TYPE_ERROR = 2 } ValueType;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_LT, OP_LE, OP_GT, OP_GE, OP_EQ, OP_NE,
    OP_AND, OP_OR, OP_ASSIGN
} BinOp;

typedef enum { UOP_PLUS, UOP_MINUS, UOP_NOT } UnOp;

typedef enum {
    EXPR_INT_LIT, EXPR_IDENT, EXPR_CALL, EXPR_UNARY, EXPR_BINARY, EXPR_PAREN
} ExprKind;

typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef struct {
    char *name;
    SourceLoc loc;
    int sym_id;
} ParamNode;

struct Expr {
    ExprKind kind;
    SourceLoc loc;
    ValueType eval_type;
    bool is_lvalue;
    int resolved_sym_id;
    union {
        int32_t int_val;
        struct { char *name; } ident;
        struct { char *callee; Expr **args; size_t arg_count; } call;
        struct { UnOp op; Expr *operand; } unary;
        struct { BinOp op; Expr *lhs; Expr *rhs; } binary;
        struct { Expr *inner; } paren;
    } as;
};

typedef enum {
    STMT_BLOCK, STMT_LOCAL_DECL, STMT_EXPR,
    STMT_IF, STMT_WHILE, STMT_FOR,
    STMT_BREAK, STMT_CONTINUE, STMT_RETURN
} StmtKind;

struct Stmt {
    StmtKind kind;
    SourceLoc loc;
    union {
        struct { Stmt **items; size_t count; } block;
        struct { char *name; Expr *init; int sym_id; } local_decl;
        struct { Expr *expr; } expr_stmt;
        struct { Expr *cond; Stmt *then_branch; Stmt *else_branch; } if_stmt;
        struct { Expr *cond; Stmt *body; } while_stmt;
        struct { Stmt *init; Expr *cond; Expr *update; Stmt *body; } for_stmt;
        struct { Expr *value; } return_stmt;
    } as;
};

typedef struct {
    bool is_definition;
    ValueType ret_type;
    char *name;
    SourceLoc loc;
    ParamNode *params;
    size_t param_count;
    Stmt *body;
    int sym_id;
} FuncDecl;

typedef struct {
    FuncDecl **funcs;
    size_t func_count;
} Program;

Program *parse_tokens(const TokenList *tl, DiagList *diags);
void ast_free(Program *prog);
void dump_ast(const Program *prog, FILE *out);

#endif
