#ifndef CMINOR_IR_H
#define CMINOR_IR_H

#include "sema.h"

typedef enum { OPERAND_NONE = 0, OPERAND_CONST, OPERAND_TEMP, OPERAND_VAR } OperandKind;

typedef struct {
    OperandKind kind;
    int32_t val;
    char *var_name;
} IROperand;

typedef enum {
    IR_LABEL, IR_MOV,
    IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_MOD,
    IR_NEG, IR_NOT,
    IR_LT, IR_LE, IR_GT, IR_GE, IR_EQ, IR_NE,
    IR_JMP, IR_JZ, IR_JNZ,
    IR_ARG, IR_CALL, IR_RET, IR_RET_VOID
} IROpcode;

typedef struct {
    IROpcode op;
    IROperand dst;
    IROperand src1;
    IROperand src2;
    int label_id;
    char *callee;
    size_t arg_count;
} IRInstr;

typedef struct {
    int sym_id;
    char *name;
    bool is_param;
} IRVarSlot;

typedef struct {
    char *name;
    ValueType ret_type;
    int *param_sym_ids;
    char **param_names;
    size_t param_count;
    IRVarSlot *vars;
    size_t var_count;
    size_t var_cap;
    int next_temp;
    int next_label;
    IRInstr *code;
    size_t code_count;
    size_t code_cap;
} IRFunction;

typedef struct {
    IRFunction *funcs;
    size_t func_count;
} IRProgram;

IRProgram *ir_generate(const Program *prog, const SymbolTable *st);
void ir_free(IRProgram *irp);
void dump_ir(const IRProgram *irp, FILE *out);

#endif
