#include "../../include/lexer.h"
#include <ctype.h>
#include <errno.h>

static void tl_push(TokenList *tl, Token t, DiagList *diags) {
    if (tl->count >= MAX_TOKENS) {
        if (tl->count == MAX_TOKENS)
            diag_emit(diags, DIAG_LIMIT_ERROR, t.loc, "token count exceeds baseline limit (%d)", MAX_TOKENS);
        free(t.lexeme);
        tl->count++;
        return;
    }
    if (tl->count == tl->cap) {
        size_t ncap = tl->cap ? tl->cap * 2 : 128;
        tl->data = realloc(tl->data, ncap * sizeof(Token));
        tl->cap = ncap;
    }
    tl->data[tl->count++] = t;
}

void token_list_free(TokenList *tl) {
    size_t limit = tl->count < MAX_TOKENS ? tl->count : MAX_TOKENS;
    for (size_t i = 0; i < limit; i++) free(tl->data[i].lexeme);
    free(tl->data);
    memset(tl, 0, sizeof(*tl));
}

const char *token_kind_name(TokenKind k) {
    switch (k) {
        case TOK_EOF: return "EOF"; case TOK_KW_INT: return "KW_INT"; case TOK_KW_VOID: return "KW_VOID";
        case TOK_KW_IF: return "KW_IF"; case TOK_KW_ELSE: return "KW_ELSE"; case TOK_KW_WHILE: return "KW_WHILE";
        case TOK_KW_FOR: return "KW_FOR"; case TOK_KW_BREAK: return "KW_BREAK"; case TOK_KW_CONTINUE: return "KW_CONTINUE";
        case TOK_KW_RETURN: return "KW_RETURN"; case TOK_IDENT: return "IDENT"; case TOK_INT_LIT: return "INT_LIT";
        case TOK_LPAREN: return "LPAREN"; case TOK_RPAREN: return "RPAREN"; case TOK_LBRACE: return "LBRACE";
        case TOK_RBRACE: return "RBRACE"; case TOK_SEMI: return "SEMI"; case TOK_COMMA: return "COMMA";
        case TOK_PLUS: return "PLUS"; case TOK_MINUS: return "MINUS"; case TOK_STAR: return "STAR";
        case TOK_SLASH: return "SLASH"; case TOK_PERCENT: return "PERCENT"; case TOK_LT: return "LT";
        case TOK_LE: return "LE"; case TOK_GT: return "GT"; case TOK_GE: return "GE";
        case TOK_EQ: return "EQ"; case TOK_NE: return "NE"; case TOK_BANG: return "BANG";
        case TOK_AND_AND: return "AND_AND"; case TOK_OR_OR: return "OR_OR"; case TOK_ASSIGN: return "ASSIGN";
        default: return "UNKNOWN";
    }
}

static bool is_c23_reserved(const char *s) {
    static const char *kw[] = {
        "alignas","alignof","auto","bool","case","char","const","constexpr","default","do","double",
        "enum","extern","false","float","goto","inline","long","nullptr","register","restrict","short",
        "signed","sizeof","static","static_assert","struct","switch","thread_local","true","typedef",
        "typeof","typeof_unqual","union","unsigned","volatile","_Alignas","_Alignof","_Atomic","_BitInt",
        "_Bool","_Complex","_Decimal128","_Decimal32","_Decimal64","_Generic","_Imaginary","_Noreturn",
        "_Static_assert","_Thread_local",NULL
    };
    for (int i = 0; kw[i]; i++) if (strcmp(s, kw[i]) == 0) return true;
    return false;
}

static TokenKind match_kw(const char *s) {
    if (!strcmp(s, "int")) return TOK_KW_INT;
    if (!strcmp(s, "void")) return TOK_KW_VOID;
    if (!strcmp(s, "if")) return TOK_KW_IF;
    if (!strcmp(s, "else")) return TOK_KW_ELSE;
    if (!strcmp(s, "while")) return TOK_KW_WHILE;
    if (!strcmp(s, "for")) return TOK_KW_FOR;
    if (!strcmp(s, "break")) return TOK_KW_BREAK;
    if (!strcmp(s, "continue")) return TOK_KW_CONTINUE;
    if (!strcmp(s, "return")) return TOK_KW_RETURN;
    return TOK_IDENT;
}

void lex_source(const char *filename, const char *src, size_t len, TokenList *out, DiagList *diags) {
    memset(out, 0, sizeof(*out));
    if (len > MAX_INPUT_BYTES) {
        SourceLoc loc = {filename, 1, 1};
        diag_emit(diags, DIAG_LIMIT_ERROR, loc, "input exceeds %d bytes", MAX_INPUT_BYTES);
        return;
    }
    size_t i = 0; int line = 1, col = 1; bool at_line_start = true, line_err = false;
    while (i < len) {
        unsigned char c = (unsigned char)src[i];
        if (col > MAX_LINE_LEN && !line_err) {
            SourceLoc loc = {filename, line, col};
            diag_emit(diags, DIAG_LIMIT_ERROR, loc, "line exceeds %d chars", MAX_LINE_LEN);
            line_err = true;
        }
        if (c == '\r') { i += (i + 1 < len && src[i + 1] == '\n') ? 2 : 1; line++; col = 1; at_line_start = true; line_err = false; continue; }
        if (c == '\n') { i++; line++; col = 1; at_line_start = true; line_err = false; continue; }
        if (c == ' ' || c == '\t' || c == '\v' || c == '\f') { i++; col++; continue; }
        if (c == '/' && i + 1 < len && src[i + 1] == '/') {
            i += 2; col += 2;
            while (i < len && src[i] != '\n' && src[i] != '\r') { i++; col++; }
            continue;
        }
        if (c == '/' && i + 1 < len && src[i + 1] == '*') {
            SourceLoc start = {filename, line, col}; i += 2; col += 2; bool closed = false;
            while (i < len) {
                if (src[i] == '*' && i + 1 < len && src[i + 1] == '/') { i += 2; col += 2; closed = true; break; }
                if (src[i] == '\r') { i += (i + 1 < len && src[i + 1] == '\n') ? 2 : 1; line++; col = 1; at_line_start = true; line_err = false; }
                else if (src[i] == '\n') { i++; line++; col = 1; at_line_start = true; line_err = false; }
                else { i++; col++; }
            }
            if (!closed) diag_emit(diags, DIAG_LEXICAL_ERROR, start, "unterminated block comment");
            continue;
        }
        if (c == '\\') {
            SourceLoc loc = {filename, line, col};
            if (i + 1 < len && (src[i + 1] == '\n' || src[i + 1] == '\r'))
                diag_emit(diags, DIAG_UNSUPPORTED_LINE_SPLICING, loc, "line splicing is unsupported");
            else
                diag_emit(diags, DIAG_LEXICAL_ERROR, loc, "unexpected '\\'");
            i++; col++; at_line_start = false; continue;
        }
        if (c == '#' || (c == '%' && i + 1 < len && src[i + 1] == ':')) {
            SourceLoc loc = {filename, line, col};
            if (at_line_start) diag_emit(diags, DIAG_UNSUPPORTED_PREPROCESSING_DIRECTIVE, loc, "preprocessing directive rejected");
            else diag_emit(diags, DIAG_LEXICAL_ERROR, loc, "unexpected preprocessing token");
            while (i < len && src[i] != '\n' && src[i] != '\r') { i++; col++; }
            continue;
        }
        if (c >= 0x80) {
            SourceLoc loc = {filename, line, col};
            diag_emit(diags, DIAG_LEXICAL_ERROR, loc, "non-ASCII byte 0x%02X", c);
            i++; col++; at_line_start = false; continue;
        }
        if (c == '"' || c == '\'') {
            char q = (char)c; SourceLoc loc = {filename, line, col}; i++; col++;
            while (i < len && src[i] != q && src[i] != '\n' && src[i] != '\r') {
                if (src[i] == '\\' && i + 1 < len && src[i + 1] != '\n' && src[i + 1] != '\r') { i += 2; col += 2; }
                else { i++; col++; }
            }
            if (i < len && src[i] == q) { i++; col++; diag_emit(diags, DIAG_UNSUPPORTED_FEATURE, loc, "string/char literal unsupported"); }
            else diag_emit(diags, DIAG_LEXICAL_ERROR, loc, "unterminated literal");
            at_line_start = false; continue;
        }
        if (isalpha(c) || c == '_') {
            SourceLoc loc = {filename, line, col}; size_t s = i;
            while (i < len && (isalnum((unsigned char)src[i]) || src[i] == '_')) { i++; col++; }
            char *w = cm_strndup(src + s, i - s); TokenKind kw = match_kw(w);
            if (kw != TOK_IDENT) tl_push(out, (Token){kw, w, 0, loc}, diags);
            else if (is_c23_reserved(w)) { diag_emit(diags, DIAG_UNSUPPORTED_FEATURE, loc, "reserved C23 keyword '%s'", w); free(w); }
            else tl_push(out, (Token){TOK_IDENT, w, 0, loc}, diags);
            at_line_start = false; continue;
        }
        if (isdigit(c)) {
            SourceLoc loc = {filename, line, col}; size_t s = i;
            bool bad = (c == '0' && i + 1 < len && (isalnum((unsigned char)src[i + 1]) || src[i + 1] == '_' || src[i + 1] == '.'));
            while (i < len && (isalnum((unsigned char)src[i]) || src[i] == '_' || src[i] == '.')) {
                if (!isdigit((unsigned char)src[i])) bad = true;
                i++; col++;
            }
            char *lit = cm_strndup(src + s, i - s);
            if (bad) { diag_emit(diags, DIAG_UNSUPPORTED_FEATURE, loc, "malformed numeric literal '%s'", lit); free(lit); }
            else {
                errno = 0; unsigned long long v = strtoull(lit, NULL, 10);
                if (errno == ERANGE || v > 2147483647ULL) { diag_emit(diags, DIAG_UNSUPPORTED_FEATURE, loc, "literal '%s' out of int32 range", lit); free(lit); }
                else tl_push(out, (Token){TOK_INT_LIT, lit, (int32_t)v, loc}, diags);
            }
            at_line_start = false; continue;
        }
        SourceLoc loc = {filename, line, col};
        char c1 = (char)c, c2 = (i + 1 < len) ? src[i + 1] : '\0', c3 = (i + 2 < len) ? src[i + 2] : '\0';
        if ((c1=='+'&&c2=='+')||(c1=='-'&&c2=='-')||(c1=='+'&&c2=='=')||(c1=='-'&&c2=='=')||
            (c1=='*'&&c2=='=')||(c1=='/'&&c2=='=')||(c1=='%'&&c2=='=')||(c1=='&'&&c2=='=')||
            (c1=='|'&&c2=='=')||(c1=='^'&&c2=='=')||(c1=='-'&&c2=='>')||(c1=='.'&&c2=='.'&&c3=='.')) {
            int adv = (c1=='.'&&c2=='.'&&c3=='.') ? 3 : 2;
            diag_emit(diags, DIAG_UNSUPPORTED_FEATURE, loc, "unsupported operator");
            i += (size_t)adv; col += adv; at_line_start = false; continue;
        }
        if ((c1=='<'&&c2=='<')||(c1=='>'&&c2=='>')) {
            int adv = (c3=='=') ? 3 : 2;
            diag_emit(diags, DIAG_UNSUPPORTED_FEATURE, loc, "unsupported shift operator");
            i += (size_t)adv; col += adv; at_line_start = false; continue;
        }
        if (strchr("?:~^[]", c1) || (c1=='&'&&c2!='&') || (c1=='|'&&c2!='|')) {
            diag_emit(diags, DIAG_UNSUPPORTED_FEATURE, loc, "unsupported operator/punctuator '%c'", c1);
            i++; col++; at_line_start = false; continue;
        }
        if (c1=='<'&&c2=='=') { tl_push(out,(Token){TOK_LE,cm_strdup("<="),0,loc},diags); i+=2; col+=2; at_line_start=false; continue; }
        if (c1=='>'&&c2=='=') { tl_push(out,(Token){TOK_GE,cm_strdup(">="),0,loc},diags); i+=2; col+=2; at_line_start=false; continue; }
        if (c1=='='&&c2=='=') { tl_push(out,(Token){TOK_EQ,cm_strdup("=="),0,loc},diags); i+=2; col+=2; at_line_start=false; continue; }
        if (c1=='!'&&c2=='=') { tl_push(out,(Token){TOK_NE,cm_strdup("!="),0,loc},diags); i+=2; col+=2; at_line_start=false; continue; }
        if (c1=='&'&&c2=='&') { tl_push(out,(Token){TOK_AND_AND,cm_strdup("&&"),0,loc},diags); i+=2; col+=2; at_line_start=false; continue; }
        if (c1=='|'&&c2=='|') { tl_push(out,(Token){TOK_OR_OR,cm_strdup("||"),0,loc},diags); i+=2; col+=2; at_line_start=false; continue; }

        TokenKind sk = TOK_EOF;
        switch (c1) {
            case '(': sk=TOK_LPAREN; break; case ')': sk=TOK_RPAREN; break; case '{': sk=TOK_LBRACE; break;
            case '}': sk=TOK_RBRACE; break; case ';': sk=TOK_SEMI; break; case ',': sk=TOK_COMMA; break;
            case '+': sk=TOK_PLUS; break; case '-': sk=TOK_MINUS; break; case '*': sk=TOK_STAR; break;
            case '/': sk=TOK_SLASH; break; case '%': sk=TOK_PERCENT; break; case '<': sk=TOK_LT; break;
            case '>': sk=TOK_GT; break; case '!': sk=TOK_BANG; break; case '=': sk=TOK_ASSIGN; break;
        }
        if (sk != TOK_EOF) {
            char b[2] = {c1, 0}; tl_push(out, (Token){sk, cm_strdup(b), 0, loc}, diags);
            i++; col++; at_line_start = false; continue;
        }
        diag_emit(diags, DIAG_LEXICAL_ERROR, loc, "unknown character 0x%02X", c);
        i++; col++; at_line_start = false;
    }
    tl_push(out, (Token){TOK_EOF, cm_strdup("<eof>"), 0, {filename, line, col}}, diags);
}

void dump_tokens(const TokenList *tl, FILE *out) {
    size_t limit = tl->count < MAX_TOKENS ? tl->count : MAX_TOKENS;
    for (size_t i = 0; i < limit; i++) {
        const Token *t = &tl->data[i];
        fprintf(out, "%4zu | %s:%d:%d | %-12s | %s\n", i, t->loc.file, t->loc.line, t->loc.col, token_kind_name(t->kind), t->lexeme);
    }
}
