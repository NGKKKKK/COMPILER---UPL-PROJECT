#include <stdio.h>
#include <string.h>
#include "error.h"
#include "token_name.h"
#include "ast.h"
#include "parser.tab.h"
#include "jvm.h"

extern FILE *yyin;
extern char *yytext;
extern Node *ast_root;
int yylex(void);
int yyparse(void);

/* Chế độ --tokens: chỉ chạy lexer, in từng token */
static void dump_tokens(void) {
    int tok;
    printf("%-8s %-10s %s\n", "LINE:COL", "TOKEN", "LEXEME");
    while ((tok = yylex()) != 0) {
        printf("%d:%-5d %-10s '%s'\n", yylloc.first_line, yylloc.first_column,
               token_name(tok), yytext);
    }
}

int main(int argc, char **argv) {
    int tokens_mode = 0, quiet = 0;
    const char *path = NULL, *output = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) tokens_mode = 1;
        else if (strcmp(argv[i], "--quiet") == 0) quiet = 1;   /* không in AST */
        else if (strcmp(argv[i], "--compile") == 0 && !output && i + 1 < argc)
            output = argv[++i];
        else if (!path && argv[i][0] != '-') path = argv[i];
        else goto usage;
    }
    if (!path || (tokens_mode && output)) goto usage;

    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return 2; }

    error_set_file(path);
    yyin = f;

    int parse_status = 0;
    if (tokens_mode) dump_tokens();
    else             parse_status = yyparse();
    fclose(f);

    SymbolTable symbols = {0};
    if (output && !parse_status && error_count() == 0)
        semantic_check(ast_root, &symbols);

    if (error_count() > 0) {
        printf("Found %d error(s).\n", error_count());
        symbol_table_free(&symbols);
        ast_free(ast_root);
        return 1;
    }

    if (tokens_mode) {
        printf("Lexing OK.\n");
        return 0;
    }

    if (parse_status) {
        ast_free(ast_root);
        return 2;
    }

    if (output) {
        TacProgram program = {0};
        tac_generate(&program, ast_root, &symbols);
        int status = jvm_compile(&program, &symbols, output);
        tac_free(&program);
        symbol_table_free(&symbols);
        ast_free(ast_root);
        if (status != 0) return 2;
        if (!quiet) printf("Compiled UplProgram to %s\n", output);
        return 0;
    }

    if (!quiet) ast_print_tree(ast_root);
    printf("OK: no errors.\n");
    ast_free(ast_root);
    return 0;

usage:
    fprintf(stderr, "Usage: %s [--tokens] [--quiet] <file.upl>\n"
                    "       %s --compile <output-dir> <file.upl>\n", argv[0], argv[0]);
    return 2;
}
