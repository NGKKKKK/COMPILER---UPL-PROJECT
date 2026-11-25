#include <stdlib.h>
#include "tac.h"

static const TacOperand none = {TAC_NONE, 0, 0};

static TacOperand constant(VarType type, int value) {
    return (TacOperand){TAC_CONSTANT, type, value};
}

static TacOperand variable(const SymbolTable *symbols, int id) {
    return (TacOperand){TAC_VARIABLE, symbols->entries[id].type, id};
}

static TacOperand new_label(TacProgram *program) {
    return (TacOperand){TAC_LABEL_REF, 0, program->label_count++};
}

static void emit(TacProgram *program, TacOp op, TacOperand arg1,
                 TacOperand arg2, TacOperand result) {
    if (program->count == program->capacity) {
        int capacity = program->capacity ? program->capacity * 2 : 32;
        Quad *quads = realloc(program->quads, (size_t)capacity * sizeof *quads);
        if (!quads) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        program->quads = quads;
        program->capacity = capacity;
    }
    program->quads[program->count++] = (Quad){op, arg1, arg2, result};
}

static TacOperand gen_expr(TacProgram *program, const SymbolTable *symbols, const Node *node) {
    switch (node->kind) {
        case NODE_ID:
            return variable(symbols, node->symbol_id);
        case NODE_INTLIT:
            return constant(TYPE_INT, node->as.intlit.value);
        case NODE_BOOLLIT:
            return constant(TYPE_BOOL, node->as.boollit.value);
        case NODE_BINOP: {
            static const TacOp ops[] = {
                [OP_ADD] = TAC_ADD, [OP_MUL] = TAC_MUL,
                [OP_GT] = TAC_GT, [OP_GE] = TAC_GE, [OP_EQ] = TAC_EQ,
            };
            TacOperand left = gen_expr(program, symbols, node->as.binop.left);
            TacOperand right = gen_expr(program, symbols, node->as.binop.right);
            TacOperand result = {TAC_TEMPORARY, node->expr_type, program->temporary_count++};
            emit(program, ops[node->as.binop.op], left, right, result);
            return result;
        }
        default:
            abort();   /* Chỉ gọi cho AST đã qua phân tích ngữ nghĩa. */
    }
}

static void gen_stmt(TacProgram *program, const SymbolTable *symbols, const Node *node);

static void gen_if(TacProgram *program, const SymbolTable *symbols, const Node *node) {
    TacOperand cond = gen_expr(program, symbols, node->as.ifs.cond);
    TacOperand other = new_label(program);
    emit(program, TAC_IF_FALSE, cond, none, other);
    gen_stmt(program, symbols, node->as.ifs.then_blk);
    if (node->as.ifs.else_blk) {
        TacOperand end = new_label(program);
        emit(program, TAC_GOTO, none, none, end);
        emit(program, TAC_LABEL, none, none, other);
        gen_stmt(program, symbols, node->as.ifs.else_blk);
        emit(program, TAC_LABEL, none, none, end);
    } else {
        emit(program, TAC_LABEL, none, none, other);
    }
}

static void gen_stmt(TacProgram *program, const SymbolTable *symbols, const Node *node) {
    switch (node->kind) {
        case NODE_PROGRAM:
        case NODE_BLOCK:
            for (int i = 0; i < node->as.block.count; i++)
                gen_stmt(program, symbols, node->as.block.stmts[i]);
            break;
        case NODE_DECL: {
            TacOperand target = variable(symbols, node->symbol_id);
            TacOperand value = node->as.decl.init
                ? gen_expr(program, symbols, node->as.decl.init)
                : constant(target.type, 0);
            emit(program, TAC_COPY, value, none, target);
            break;
        }
        case NODE_ASSIGN: {
            TacOperand value = gen_expr(program, symbols, node->as.assign.value);
            emit(program, TAC_COPY, value, none, variable(symbols, node->symbol_id));
            break;
        }
        case NODE_PRINT: {
            TacOperand expr = gen_expr(program, symbols, node->as.print.expr);
            emit(program, TAC_PRINT, expr, none, none);
            break;
        }
        case NODE_IF:
            gen_if(program, symbols, node);
            break;
        case NODE_DOWHILE: {
            TacOperand start = new_label(program);
            emit(program, TAC_LABEL, none, none, start);
            gen_stmt(program, symbols, node->as.dowhile.body);
            TacOperand cond = gen_expr(program, symbols, node->as.dowhile.cond);
            emit(program, TAC_IF_TRUE, cond, none, start);
            break;
        }
        case NODE_FOR: {
            TacOperand start = new_label(program);
            TacOperand end = new_label(program);
            gen_stmt(program, symbols, node->as.fors.init);
            emit(program, TAC_LABEL, none, none, start);
            TacOperand cond = gen_expr(program, symbols, node->as.fors.cond);
            emit(program, TAC_IF_FALSE, cond, none, end);
            gen_stmt(program, symbols, node->as.fors.body);
            gen_stmt(program, symbols, node->as.fors.update);
            emit(program, TAC_GOTO, none, none, start);
            emit(program, TAC_LABEL, none, none, end);
            break;
        }
        default:
            abort();
    }
}

void tac_generate(TacProgram *program, const Node *root, const SymbolTable *symbols) {
    program->variable_count = symbols->count;
    gen_stmt(program, symbols, root);
}

static const char *type_name(VarType type) {
    return type == TYPE_INT ? "int" : "bool";
}

static void write_operand(FILE *out, TacOperand operand) {
    switch (operand.kind) {
        case TAC_CONSTANT:
            if (operand.type == TYPE_BOOL)
                fputs(operand.value ? "true" : "false", out);
            else
                fprintf(out, "%d", operand.value);
            break;
        case TAC_VARIABLE:  fprintf(out, "v%d", operand.value); break;
        case TAC_TEMPORARY: fprintf(out, "t%d", operand.value); break;
        case TAC_LABEL_REF: fprintf(out, "L%d", operand.value); break;
        case TAC_NONE:      break;
    }
}

void tac_write(FILE *out, const TacProgram *program, const SymbolTable *symbols) {
    for (int i = 0; i < symbols->count; i++) {
        const Symbol *symbol = &symbols->entries[i];
        fprintf(out, "# v%d: %s %s, scope %d, declared at %d:%d\n", i,
                type_name(symbol->type), symbol->name, symbol->scope_id,
                symbol->declaration.line, symbol->declaration.col);
    }
    for (int i = 0; i < program->count; i++) {
        const Quad *q = &program->quads[i];
        switch (q->op) {
            case TAC_COPY:
                fputs("  ", out);
                write_operand(out, q->result);
                fputs(" = ", out);
                write_operand(out, q->arg1);
                break;
            case TAC_ADD:
            case TAC_MUL:
            case TAC_GT:
            case TAC_GE:
            case TAC_EQ: {
                static const char *const ops[] = {
                    [TAC_ADD] = "+", [TAC_MUL] = "*", [TAC_GT] = ">",
                    [TAC_GE] = ">=", [TAC_EQ] = "==",
                };
                fputs("  ", out);
                write_operand(out, q->result);
                fprintf(out, ":%s = ", type_name(q->result.type));
                write_operand(out, q->arg1);
                fprintf(out, " %s ", ops[q->op]);
                write_operand(out, q->arg2);
                break;
            }
            case TAC_LABEL:
                write_operand(out, q->result);
                fputc(':', out);
                break;
            case TAC_GOTO:
                fputs("  goto ", out);
                write_operand(out, q->result);
                break;
            case TAC_IF_TRUE:
            case TAC_IF_FALSE:
                fputs(q->op == TAC_IF_TRUE ? "  if " : "  ifFalse ", out);
                write_operand(out, q->arg1);
                fputs(" goto ", out);
                write_operand(out, q->result);
                break;
            case TAC_PRINT:
                fputs("  print ", out);
                write_operand(out, q->arg1);
                break;
        }
        fputc('\n', out);
    }
}

void tac_free(TacProgram *program) {
    free(program->quads);
    *program = (TacProgram){0};
}
