#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "semantic.h"

typedef enum { SEM_INT = TYPE_INT, SEM_BOOL = TYPE_BOOL, SEM_INVALID } ExprType;

typedef struct Scope {
    int id;
    int last_symbol;
    const struct Scope *parent;
} Scope;

typedef struct {
    SymbolTable *table;
    int next_scope;
} Analyzer;

static const char *type_name(ExprType type) {
    return type == SEM_INT ? "int" : "bool";
}

static void report(Location loc, const char *message) {
    error_report("semantic error", loc.line, loc.col, message);
}

static int lookup_local(const SymbolTable *table, const Scope *scope, const char *name) {
    for (int id = scope->last_symbol; id >= 0; id = table->entries[id].previous_in_scope) {
        if (strcmp(table->entries[id].name, name) == 0) return id;
    }
    return -1;
}

static int lookup_use(const SymbolTable *table, const Scope *scope,
                      const char *name, Location loc) {
    for (; scope; scope = scope->parent) {
        int id = lookup_local(table, scope, name);
        if (id >= 0) return id;
    }
    char message[256];
    snprintf(message, sizeof message, "variable '%s' is not declared", name);
    report(loc, message);
    return -1;
}

static int insert_symbol(SymbolTable *table, Scope *scope, const Node *decl) {
    if (table->count == table->capacity) {
        int capacity = table->capacity ? table->capacity * 2 : 16;
        Symbol *entries = realloc(table->entries, (size_t)capacity * sizeof *entries);
        if (!entries) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
        table->entries = entries;
        table->capacity = capacity;
    }
    int id = table->count++;
    table->entries[id] = (Symbol){
        .name = decl->as.decl.name,
        .type = decl->as.decl.type,
        .scope_id = scope->id,
        .declaration = decl->loc,
        .previous_in_scope = scope->last_symbol,
    };
    scope->last_symbol = id;
    return id;
}

static ExprType check_expr(const SymbolTable *table, const Scope *scope, Node *expr);

static ExprType check_binop(const SymbolTable *table, const Scope *scope, Node *expr) {
    ExprType left = check_expr(table, scope, expr->as.binop.left);
    ExprType right = check_expr(table, scope, expr->as.binop.right);
    if (left == SEM_INVALID || right == SEM_INVALID) return SEM_INVALID;

    switch (expr->as.binop.op) {
        case OP_ADD:
        case OP_MUL:
            if (left == SEM_INT && right == SEM_INT) return SEM_INT;
            report(expr->loc, "arithmetic operands must both be int");
            break;
        case OP_GT:
        case OP_GE:
            if (left == SEM_INT && right == SEM_INT) return SEM_BOOL;
            report(expr->loc, "ordered comparison operands must both be int");
            break;
        case OP_EQ:
            if (left == right) return SEM_BOOL;
            report(expr->loc, "equality operands must have the same type");
            break;
    }
    return SEM_INVALID;
}

static ExprType check_expr(const SymbolTable *table, const Scope *scope, Node *expr) {
    ExprType type = SEM_INVALID;
    switch (expr->kind) {
        case NODE_ID:
            expr->symbol_id = lookup_use(table, scope, expr->as.id.name, expr->loc);
            if (expr->symbol_id >= 0)
                type = (ExprType)table->entries[expr->symbol_id].type;
            break;
        case NODE_INTLIT:
            type = SEM_INT;
            break;
        case NODE_BOOLLIT:
            type = SEM_BOOL;
            break;
        case NODE_BINOP:
            type = check_binop(table, scope, expr);
            break;
        default:
            report(expr->loc, "expected an int or bool expression");
            break;
    }
    if (type != SEM_INVALID) expr->expr_type = (VarType)type;
    return type;
}

static void require_type(ExprType actual, ExprType expected, Location loc, const char *context) {
    if (actual == SEM_INVALID || actual == expected) return;
    char message[256];
    snprintf(message, sizeof message, "%s requires %s, got %s",
             context, type_name(expected), type_name(actual));
    report(loc, message);
}

static void check_decl(Analyzer *analyzer, Scope *scope, Node *node) {
    SymbolTable *table = analyzer->table;
    const char *name = node->as.decl.name;
    int existing = lookup_local(table, scope, name);
    if (existing >= 0) {
        Location first = table->entries[existing].declaration;
        char message[256];
        snprintf(message, sizeof message, "duplicate declaration of '%s' (first at %d:%d)",
                 name, first.line, first.col);
        report(node->loc, message);
    }

    Node *init = node->as.decl.init;
    if (init) {
        ExprType actual = check_expr(table, scope, init);
        require_type(actual, (ExprType)node->as.decl.type, init->loc, "initializer");
    }
    if (existing < 0) node->symbol_id = insert_symbol(table, scope, node);
}

static void check_condition(const SymbolTable *table, const Scope *scope,
                            Node *cond, const char *context) {
    ExprType actual = check_expr(table, scope, cond);
    require_type(actual, SEM_BOOL, cond->loc, context);
}

static void check_stmt(Analyzer *analyzer, Scope *scope, Node *node) {
    SymbolTable *table = analyzer->table;
    switch (node->kind) {
        case NODE_PROGRAM:
        case NODE_BLOCK: {
            Scope block = {analyzer->next_scope++, -1, scope};
            for (int i = 0; i < node->as.block.count; i++)
                check_stmt(analyzer, &block, node->as.block.stmts[i]);
            break;
        }
        case NODE_DECL:
            check_decl(analyzer, scope, node);
            break;
        case NODE_ASSIGN: {
            node->symbol_id = lookup_use(table, scope, node->as.assign.name, node->loc);
            Node *value = node->as.assign.value;
            ExprType actual = check_expr(table, scope, value);
            if (node->symbol_id >= 0) {
                ExprType expected = (ExprType)table->entries[node->symbol_id].type;
                require_type(actual, expected, value->loc, "assignment");
            }
            break;
        }
        case NODE_PRINT:
            /* Mọi biểu thức hợp lệ đều có kiểu int hoặc bool. */
            check_expr(table, scope, node->as.print.expr);
            break;
        case NODE_IF:
            check_condition(table, scope, node->as.ifs.cond, "if condition");
            check_stmt(analyzer, scope, node->as.ifs.then_blk);
            if (node->as.ifs.else_blk)
                check_stmt(analyzer, scope, node->as.ifs.else_blk);
            break;
        case NODE_DOWHILE:
            check_stmt(analyzer, scope, node->as.dowhile.body);
            check_condition(table, scope, node->as.dowhile.cond, "do-while condition");
            break;
        case NODE_FOR: {
            Scope loop = {analyzer->next_scope++, -1, scope};
            check_stmt(analyzer, &loop, node->as.fors.init);
            check_condition(table, &loop, node->as.fors.cond, "for condition");
            check_stmt(analyzer, &loop, node->as.fors.update);
            check_stmt(analyzer, &loop, node->as.fors.body);
            break;
        }
        default:
            break;
    }
}

void semantic_check(Node *root, SymbolTable *table) {
    Analyzer analyzer = {.table = table};
    check_stmt(&analyzer, NULL, root);
}

void symbol_table_free(SymbolTable *table) {
    free(table->entries);
    *table = (SymbolTable){0};
}
