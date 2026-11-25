#ifndef TAC_H
#define TAC_H

#include <stdio.h>
#include "semantic.h"

typedef enum {
    TAC_NONE, TAC_CONSTANT, TAC_VARIABLE, TAC_TEMPORARY, TAC_LABEL_REF
} TacOperandKind;

typedef struct {
    TacOperandKind kind;
    VarType type;                  /* chỉ dùng cho hằng, biến và biến tạm */
    int value;                     /* giá trị hằng hoặc ID của toán hạng */
} TacOperand;

typedef enum {
    TAC_COPY, TAC_ADD, TAC_MUL, TAC_GT, TAC_GE, TAC_EQ,
    TAC_LABEL, TAC_GOTO, TAC_IF_TRUE, TAC_IF_FALSE, TAC_PRINT
} TacOp;

typedef struct {
    TacOp op;
    TacOperand arg1, arg2, result;
} Quad;

typedef struct {
    Quad *quads;
    int count, capacity;
    int variable_count, temporary_count, label_count;
} TacProgram;

void tac_generate(TacProgram *program, const Node *root, const SymbolTable *symbols);
void tac_write(FILE *out, const TacProgram *program, const SymbolTable *symbols);
void tac_free(TacProgram *program);

#endif
