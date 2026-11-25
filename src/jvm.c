#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "jvm.h"

static const char tac_name[] = "UplProgram.tac";
static const char jasmin_name[] = "UplProgram.j";
static const char class_name[] = "UplProgram.class";

static int slot(const TacProgram *program, TacOperand operand) {
    /* Slot 0 là tham số String[] của main. */
    return 1 + operand.value + (operand.kind == TAC_TEMPORARY ? program->variable_count : 0);
}

static int load(FILE *out, const TacProgram *program, TacOperand operand) {
    if (operand.kind != TAC_CONSTANT) {
        int index = slot(program, operand);
        fprintf(out, "  iload %d\n", index);
        return index < 256 ? 2 : 4;
    }
    if (operand.value <= 5) {
        fprintf(out, "  iconst_%d\n", operand.value);
        return 1;
    }
    if (operand.value <= 127) {
        fprintf(out, "  bipush %d\n", operand.value);
        return 2;
    }
    if (operand.value <= 32767) {
        fprintf(out, "  sipush %d\n", operand.value);
        return 3;
    }
    fprintf(out, "  ldc_w %d\n", operand.value);
    return 3;
}

static int store(FILE *out, const TacProgram *program, TacOperand operand) {
    int index = slot(program, operand);
    fprintf(out, "  istore %d\n", index);
    return index < 256 ? 2 : 4;
}

/* Mỗi bộ bốn bắt đầu và kết thúc với stack rỗng. Giá trị trả về là
   cận trên số byte JVM của những lệnh vừa ghi. */
static int write_quad(FILE *out, const TacProgram *program, const Quad *q, int index) {
    int bytes;
    switch (q->op) {
        case TAC_COPY:
            bytes = load(out, program, q->arg1);
            return bytes + store(out, program, q->result);
        case TAC_ADD:
        case TAC_MUL:
            bytes = load(out, program, q->arg1);
            bytes += load(out, program, q->arg2);
            fputs(q->op == TAC_ADD ? "  iadd\n" : "  imul\n", out);
            return bytes + 1 + store(out, program, q->result);
        case TAC_GT:
        case TAC_GE:
        case TAC_EQ: {
            static const char *const branch[] = {
                [TAC_GT] = "if_icmpgt", [TAC_GE] = "if_icmpge", [TAC_EQ] = "if_icmpeq",
            };
            bytes = load(out, program, q->arg1);
            bytes += load(out, program, q->arg2);
            fprintf(out, "  %s C%dTrue\n"
                         "  iconst_0\n"
                         "  goto C%dEnd\n"
                         "C%dTrue:\n"
                         "  iconst_1\n"
                         "C%dEnd:\n",
                    branch[q->op], index, index, index, index);
            return bytes + 8 + store(out, program, q->result);
        }
        case TAC_LABEL:
            fprintf(out, "L%d:\n", q->result.value);
            return 0;
        case TAC_GOTO:
            fprintf(out, "  goto_w L%d\n", q->result.value);
            return 5;
        case TAC_IF_TRUE:
        case TAC_IF_FALSE:
            bytes = load(out, program, q->arg1);
            /* Nhánh có điều kiện chỉ nhảy gần. goto_w đi tới nhãn TAC. */
            fprintf(out, "  %s B%dNext\n"
                         "  goto_w L%d\n"
                         "B%dNext:\n",
                    q->op == TAC_IF_TRUE ? "ifeq" : "ifne", index, q->result.value, index);
            return bytes + 8;
        case TAC_PRINT:
            fputs("  getstatic java/lang/System/out Ljava/io/PrintStream;\n", out);
            bytes = load(out, program, q->arg1);
            fprintf(out, "  invokevirtual java/io/PrintStream/println(%c)V\n",
                    q->arg1.type == TYPE_BOOL ? 'Z' : 'I');
            return bytes + 6;
    }
    abort();
}

static int write_jasmin(FILE *out, const TacProgram *program) {
    if (program->variable_count > 65534 - program->temporary_count) {
        fprintf(stderr, "code generation error: JVM local variable limit exceeded\n");
        return -1;
    }
    fputs(".class public UplProgram\n"
          ".super java/lang/Object\n\n"
          ".method public static main([Ljava/lang/String;)V\n"
          "  .limit stack 2\n", out);
    fprintf(out, "  .limit locals %d\n", 1 + program->variable_count + program->temporary_count);
    int code_bytes = 1;   /* return */
    for (int i = 0; i < program->count; i++) {
        code_bytes += write_quad(out, program, &program->quads[i], i);
        if (code_bytes > 65535) {
            fprintf(stderr, "code generation error: JVM main method exceeds 65535 bytes\n");
            return -1;
        }
    }
    fputs("  return\n"
          ".end method\n", out);
    return 0;
}

static FILE *open_output(int dir, const char *name) {
    int fd = openat(dir, name, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd >= 0) {
        FILE *out = fdopen(fd, "w");
        if (out) return out;
        perror(name);
        close(fd);
    } else {
        perror(name);
    }
    return NULL;
}

static int close_output(FILE *out, const char *name) {
    int write_failed = ferror(out);
    if (fclose(out) != 0) {
        perror(name);
        return -1;
    }
    if (write_failed) {
        fprintf(stderr, "%s: write failed\n", name);
        return -1;
    }
    return 0;
}

static int assemble(int dir) {
    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        return -1;
    }
    if (child == 0) {
        if (fchdir(dir) == 0)
            execlp("jasmin", "jasmin", jasmin_name, (char *)NULL);
        perror("jasmin");
        _exit(127);
    }
    int status;
    pid_t result;
    do {
        result = waitpid(child, &status, 0);
    } while (result < 0 && errno == EINTR);
    if (result < 0) {
        perror("waitpid");
        return -1;
    }

    /* Một số phiên bản Jasmin trả 0 ngay cả khi assembly lỗi. */
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        static const unsigned char magic[] = {0xca, 0xfe, 0xba, 0xbe};
        unsigned char header[sizeof magic];
        int fd = openat(dir, class_name, O_RDONLY);
        if (fd >= 0) {
            ssize_t count = read(fd, header, sizeof header);
            close(fd);
            if (count == (ssize_t)sizeof header && memcmp(header, magic, sizeof magic) == 0)
                return 0;
        }
    }
    fprintf(stderr, "code generation error: Jasmin did not generate %s\n", class_name);
    return -1;
}

int jvm_compile(const TacProgram *program, const SymbolTable *symbols, const char *directory) {
    if (mkdir(directory, 0777) != 0) {
        perror(directory);
        return -1;
    }
    int dir = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir < 0) {
        perror(directory);
        rmdir(directory);
        return -1;
    }
    FILE *out = open_output(dir, tac_name);
    if (!out) goto failed;
    tac_write(out, program, symbols);
    if (close_output(out, tac_name) != 0) goto failed;
    out = open_output(dir, jasmin_name);
    if (!out) goto failed;
    int written = write_jasmin(out, program);
    if (close_output(out, jasmin_name) != 0) goto failed;
    if (written != 0 || assemble(dir) != 0) goto failed;

    close(dir);
    return 0;

failed:
    unlinkat(dir, class_name, 0);
    unlinkat(dir, jasmin_name, 0);
    unlinkat(dir, tac_name, 0);
    close(dir);
    rmdir(directory);
    return -1;
}
