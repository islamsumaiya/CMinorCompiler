#ifndef CMINOR_LEXER_H
#define CMINOR_LEXER_H

#include "common.h"

typedef enum {
    TOK_EOF = 0,
    TOK_KW_INT, TOK_KW_VOID, TOK_KW_IF, TOK_KW_ELSE,
    TOK_KW_WHILE, TOK_KW_FOR, TOK_KW_BREAK, TOK_KW_CONTINUE, TOK_KW_RETURN,
    TOK_IDENT, TOK_INT_LIT,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_SEMI, TOK_COMMA,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_LT, TOK_LE, TOK_GT, TOK_GE, TOK_EQ, TOK_NE,
    TOK_BANG, TOK_AND_AND, TOK_OR_OR, TOK_ASSIGN
} TokenKind;

typedef struct {
    TokenKind kind;
    char *lexeme;
    int32_t int_val;
    SourceLoc loc;
} Token;

typedef struct {
    Token *data;
    size_t count;
    size_t cap;
} TokenList;

void lex_source(const char *filename, const char *src, size_t len, TokenList *out, DiagList *diags);
void token_list_free(TokenList *tl);
const char *token_kind_name(TokenKind k);
void dump_tokens(const TokenList *tl, FILE *out);

#endif
