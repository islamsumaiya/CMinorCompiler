#define _POSIX_C_SOURCE 200809L
#include "../include/backend.h"
#include <errno.h>
#include <unistd.h>

typedef enum {
    MODE_COMPILE_ASM = 0,
    MODE_CHECK,
    MODE_DUMP_TOKENS,
    MODE_DUMP_AST,
    MODE_DUMP_SYMBOLS,
    MODE_DUMP_IR,
    MODE_EMIT_BACKEND_IR
} DriverMode;

static void print_help(FILE *out) {
    fprintf(out,
        "Usage: cminorcc [options] <input.cm|input.c> [-o <output>]\n"
        "Options:\n"
        "  -o <path>            Write assembly (.s) or backend IL (.ll/.ssa) to <path>\n"
        "  --backend=llvm|qbe   Select native backend family (default: llvm)\n"
        "  --check              Run lexer, parser, and semantic checks only\n"
        "  --dump-tokens        Print token stream to stdout\n"
        "  --dump-ast           Print inspectable AST to stdout\n"
        "  --dump-symbols       Print symbol table and resolutions to stdout\n"
        "  --dump-ir            Print student three-address IR to stdout\n"
        "  --emit-backend-ir    Emit LLVM IR (.ll) or QBE IL (.ssa)\n"
        "  --capabilities       Print supported profile, limits, and backend info\n"
        "  --version            Print compiler version\n"
        "  --help               Print this help message\n");
}

static void print_capabilities(void) {
    char llvm_ver[256], qbe_ver[256];
    bool has_llvm = detect_backend_version(BACKEND_LLVM, llvm_ver, sizeof(llvm_ver));
    bool has_qbe  = detect_backend_version(BACKEND_QBE, qbe_ver, sizeof(qbe_ver));

    printf("compiler: cminorcc %s\n", CMINOR_VERSION);
    printf("language: C Minor SRS v1.0 (compulsory baseline)\n");
    printf("target: x86_64-pc-linux-gnu (System V AMD64 ABI, ELF)\n");
    printf("preprocessing: disabled (rejects # and %%: directives and backslash-newline splicing)\n");
    printf("backends:\n");
    printf("  - llvm (default): %s [%s]\n", has_llvm ? "supported" : "configured (llc not found)", llvm_ver);
    printf("  - qbe:            %s [%s]\n", has_qbe  ? "supported" : "configured (qbe not found)", qbe_ver);
    printf("accepted_types: int (signed 32-bit), void (return/empty-param only)\n");
    printf("excluded_features: char, bool, float, double, pointers, arrays, structs, globals, variadics, macros\n");
    printf("limits:\n");
    printf("  max_input_bytes: %d\n", MAX_INPUT_BYTES);
    printf("  max_tokens: %d\n", MAX_TOKENS);
    printf("  max_block_nesting: %d\n", MAX_BLOCK_NESTING);
    printf("  max_expression_nesting: %d\n", MAX_EXPR_NESTING);
    printf("  max_functions: %d\n", MAX_FUNCTIONS);
    printf("  max_params_per_function: %d\n", MAX_PARAMS);
    printf("  max_locals_per_block: %d\n", MAX_LOCALS_PER_BLOCK);
    printf("  max_line_length: %d\n", MAX_LINE_LEN);
    printf("optional_extensions: none\n");
}

static bool has_valid_extension(const char *path) {
    size_t n = strlen(path);
    if (n >= 3 && strcmp(path + n - 3, ".cm") == 0) return true;
    if (n >= 2 && strcmp(path + n - 2, ".c") == 0) return true;
    return false;
}

static char *default_asm_path(const char *input_path) {
    const char *slash = strrchr(input_path, '/');
    const char *base = slash ? slash + 1 : input_path;
    size_t len = strlen(base);
    size_t stem = len;
    if (len >= 3 && strcmp(base + len - 3, ".cm") == 0) stem = len - 3;
    else if (len >= 2 && strcmp(base + len - 2, ".c") == 0) stem = len - 2;

    char *out = malloc(stem + 3);
    if (!out) { perror("malloc"); exit(1); }
    memcpy(out, base, stem);
    memcpy(out + stem, ".s", 3);
    return out;
}

static char *read_source_file(const char *path, size_t *out_len, DiagList *diags) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        diag_emit_tool(diags, DIAG_IO_ERROR, "cannot open input file '%s': %s", path, strerror(errno));
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        diag_emit_tool(diags, DIAG_IO_ERROR, "failed seeking input file '%s'", path);
        return NULL;
    }
    long sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        diag_emit_tool(diags, DIAG_IO_ERROR, "failed determining size of '%s'", path);
        return NULL;
    }
    rewind(fp);
    if ((size_t)sz > MAX_INPUT_BYTES) {
        fclose(fp);
        SourceLoc loc = {path, 1, 1};
        diag_emit(diags, DIAG_LIMIT_ERROR, loc, "input file size (%ld bytes) exceeds baseline limit (%d bytes)", sz, MAX_INPUT_BYTES);
        return NULL;
    }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); perror("malloc"); exit(1); }
    size_t rd = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (rd != (size_t)sz) {
        free(buf);
        diag_emit_tool(diags, DIAG_IO_ERROR, "short read on input file '%s'", path);
        return NULL;
    }
    buf[rd] = '\0';
    *out_len = rd;
    return buf;
}

int main(int argc, char **argv) {
    DriverMode mode = MODE_COMPILE_ASM;
    bool mode_explicit = false;
    BackendKind backend = BACKEND_LLVM;
    const char *input_path = NULL;
    const char *output_path = NULL;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0) { print_help(stdout); return 0; }
        else if (strcmp(arg, "--version") == 0) { printf("cminorcc %s\n", CMINOR_VERSION); return 0; }
        else if (strcmp(arg, "--capabilities") == 0) { print_capabilities(); return 0; }
        else if (strcmp(arg, "--check") == 0) {
            if (mode_explicit) { fprintf(stderr, "error [COMMAND_ERROR]: mutually exclusive mode flags\n"); return 2; }
            mode = MODE_CHECK; mode_explicit = true;
        } else if (strcmp(arg, "--dump-tokens") == 0) {
            if (mode_explicit) { fprintf(stderr, "error [COMMAND_ERROR]: mutually exclusive mode flags\n"); return 2; }
            mode = MODE_DUMP_TOKENS; mode_explicit = true;
        } else if (strcmp(arg, "--dump-ast") == 0) {
            if (mode_explicit) { fprintf(stderr, "error [COMMAND_ERROR]: mutually exclusive mode flags\n"); return 2; }
            mode = MODE_DUMP_AST; mode_explicit = true;
        } else if (strcmp(arg, "--dump-symbols") == 0) {
            if (mode_explicit) { fprintf(stderr, "error [COMMAND_ERROR]: mutually exclusive mode flags\n"); return 2; }
            mode = MODE_DUMP_SYMBOLS; mode_explicit = true;
        } else if (strcmp(arg, "--dump-ir") == 0) {
            if (mode_explicit) { fprintf(stderr, "error [COMMAND_ERROR]: mutually exclusive mode flags\n"); return 2; }
            mode = MODE_DUMP_IR; mode_explicit = true;
        } else if (strcmp(arg, "--emit-backend-ir") == 0) {
            if (mode_explicit) { fprintf(stderr, "error [COMMAND_ERROR]: mutually exclusive mode flags\n"); return 2; }
            mode = MODE_EMIT_BACKEND_IR; mode_explicit = true;
        } else if (strcmp(arg, "--backend=llvm") == 0) {
            backend = BACKEND_LLVM;
        } else if (strcmp(arg, "--backend=qbe") == 0) {
            backend = BACKEND_QBE;
        } else if (strncmp(arg, "--backend=", 10) == 0) {
            fprintf(stderr, "error [COMMAND_ERROR]: unsupported backend '%s'\n", arg + 10);
            return 2;
        } else if (strcmp(arg, "-o") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "error [COMMAND_ERROR]: '-o' requires a path\n"); return 2; }
            output_path = argv[++i];
        } else if (arg[0] == '-') {
            fprintf(stderr, "error [COMMAND_ERROR]: unknown option '%s'\n", arg);
            return 2;
        } else {
            if (input_path != NULL) { fprintf(stderr, "error [COMMAND_ERROR]: multiple input files given\n"); return 2; }
            input_path = arg;
        }
    }

    if (!input_path) { fprintf(stderr, "error [COMMAND_ERROR]: missing input file\n"); return 2; }
    if (!has_valid_extension(input_path)) { fprintf(stderr, "error [COMMAND_ERROR]: input must end with .cm or .c\n"); return 2; }
    if (output_path && (mode == MODE_CHECK || mode == MODE_DUMP_TOKENS ||
                        mode == MODE_DUMP_AST || mode == MODE_DUMP_SYMBOLS || mode == MODE_DUMP_IR)) {
        fprintf(stderr, "error [COMMAND_ERROR]: '-o' cannot be combined with --check or --dump-*\n");
        return 2;
    }

    DiagList diags;
    diag_init(&diags);

    size_t src_len = 0;
    char *src = read_source_file(input_path, &src_len, &diags);
    if (!src) { diag_free(&diags); return 1; }

    TokenList tokens;
    lex_source(input_path, src, src_len, &tokens, &diags);
    if (mode == MODE_DUMP_TOKENS) {
        if (diags.had_error) { token_list_free(&tokens); free(src); diag_free(&diags); return 1; }
        dump_tokens(&tokens, stdout);
        token_list_free(&tokens); free(src); diag_free(&diags);
        return 0;
    }

    Program *ast = parse_tokens(&tokens, &diags);
    if (mode == MODE_DUMP_AST) {
        if (diags.had_error) { ast_free(ast); token_list_free(&tokens); free(src); diag_free(&diags); return 1; }
        dump_ast(ast, stdout);
        ast_free(ast); token_list_free(&tokens); free(src); diag_free(&diags);
        return 0;
    }

    SymbolTable symtab;
    sema_analyze(ast, &symtab, &diags);
    if (diags.had_error) {
        symtab_free(&symtab); ast_free(ast); token_list_free(&tokens); free(src); diag_free(&diags);
        return 1;
    }

    if (mode == MODE_CHECK) {
        symtab_free(&symtab); ast_free(ast); token_list_free(&tokens); free(src); diag_free(&diags);
        return 0;
    }

    if (mode == MODE_DUMP_SYMBOLS) {
        dump_symbols(&symtab, stdout);
        symtab_free(&symtab); ast_free(ast); token_list_free(&tokens); free(src); diag_free(&diags);
        return 0;
    }

    IRProgram *ir = ir_generate(ast, &symtab);
    if (mode == MODE_DUMP_IR) {
        dump_ir(ir, stdout);
        ir_free(ir); symtab_free(&symtab); ast_free(ast); token_list_free(&tokens); free(src); diag_free(&diags);
        return 0;
    }

    if (mode == MODE_EMIT_BACKEND_IR) {
        if (!output_path) {
            if (backend == BACKEND_LLVM) emit_llvm_ir(ir, stdout);
            else emit_qbe_il(ir, stdout);
        } else {
            FILE *outf = fopen(output_path, "wb");
            if (!outf) {
                diag_emit_tool(&diags, DIAG_IO_ERROR, "cannot open output '%s': %s", output_path, strerror(errno));
                ir_free(ir); symtab_free(&symtab); ast_free(ast); token_list_free(&tokens); free(src); diag_free(&diags);
                return 1;
            }
            if (backend == BACKEND_LLVM) emit_llvm_ir(ir, outf);
            else emit_qbe_il(ir, outf);
            fclose(outf);
        }
        ir_free(ir); symtab_free(&symtab); ast_free(ast); token_list_free(&tokens); free(src); diag_free(&diags);
        return 0;
    }

    char *owned_out = NULL;
    const char *final_s_path = output_path;
    if (!final_s_path) {
        owned_out = default_asm_path(input_path);
        final_s_path = owned_out;
    }

    char tmp_il[512], tmp_s[512];
    pid_t pid = getpid();
    snprintf(tmp_il, sizeof(tmp_il), "/tmp/cminor_%d.%s", (int)pid, backend == BACKEND_LLVM ? "ll" : "ssa");
    snprintf(tmp_s, sizeof(tmp_s), "%s.tmp.%d", final_s_path, (int)pid);

    FILE *il_fp = fopen(tmp_il, "wb");
    if (!il_fp) {
        diag_emit_tool(&diags, DIAG_IO_ERROR, "cannot create temp backend IL '%s': %s", tmp_il, strerror(errno));
        free(owned_out); ir_free(ir); symtab_free(&symtab); ast_free(ast); token_list_free(&tokens); free(src); diag_free(&diags);
        return 1;
    }
    if (backend == BACKEND_LLVM) emit_llvm_ir(ir, il_fp);
    else emit_qbe_il(ir, il_fp);
    fclose(il_fp);

    FILE *probe_fp = fopen(tmp_s, "wb");
    if (!probe_fp) {
        diag_emit_tool(&diags, DIAG_IO_ERROR, "cannot write output '%s': %s", final_s_path, strerror(errno));
        unlink(tmp_il); free(owned_out); ir_free(ir); symtab_free(&symtab); ast_free(ast); token_list_free(&tokens); free(src); diag_free(&diags);
        return 1;
    }
    fclose(probe_fp);

    bool ok = run_native_backend(backend, tmp_il, tmp_s, &diags);
    unlink(tmp_il);

    if (!ok) {
        unlink(tmp_s);
        free(owned_out); ir_free(ir); symtab_free(&symtab); ast_free(ast); token_list_free(&tokens); free(src); diag_free(&diags);
        return 1;
    }

    if (rename(tmp_s, final_s_path) != 0) {
        diag_emit_tool(&diags, DIAG_IO_ERROR, "failed moving assembly to '%s': %s", final_s_path, strerror(errno));
        unlink(tmp_s);
        free(owned_out); ir_free(ir); symtab_free(&symtab); ast_free(ast); token_list_free(&tokens); free(src); diag_free(&diags);
        return 1;
    }

    free(owned_out); ir_free(ir); symtab_free(&symtab); ast_free(ast); token_list_free(&tokens); free(src); diag_free(&diags);
    return 0;
}
