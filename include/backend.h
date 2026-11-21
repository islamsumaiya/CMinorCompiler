#ifndef CMINOR_BACKEND_H
#define CMINOR_BACKEND_H

#include "ir.h"

typedef enum { BACKEND_LLVM = 0, BACKEND_QBE = 1 } BackendKind;

void emit_llvm_ir(const IRProgram *irp, FILE *out);
void emit_qbe_il(const IRProgram *irp, FILE *out);
bool run_native_backend(BackendKind kind, const char *backend_il_path, const char *out_asm_path, DiagList *diags);
bool detect_backend_version(BackendKind kind, char *buf, size_t buf_sz);

#endif
