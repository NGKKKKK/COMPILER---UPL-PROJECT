#ifndef JVM_H
#define JVM_H

#include "tac.h"

/* Ghi TAC, Jasmin và lớp UplProgram vào một thư mục mới. */
int jvm_compile(const TacProgram *program, const SymbolTable *symbols, const char *directory);

#endif
