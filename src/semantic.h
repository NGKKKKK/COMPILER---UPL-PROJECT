#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"

typedef struct {
    const char *name;              /* được AST sở hữu */
    VarType type;
    int scope_id;
    Location declaration;
    int previous_in_scope;
} Symbol;

typedef struct {
    Symbol *entries;               /* chỉ số ổn định kể cả sau khi rời scope */
    int count, capacity;
} SymbolTable;

void semantic_check(Node *root, SymbolTable *table);
void symbol_table_free(SymbolTable *table);

#endif
