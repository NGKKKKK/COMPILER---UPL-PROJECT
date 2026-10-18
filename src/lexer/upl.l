%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "error.h"
#include "parser.tab.h"

/* Cột tự đếm; dòng do Flex đếm (yylineno) */
static int yycolumn = 1;

/* Vi tri bat dau cua comment nhieu dong (de bao loi chua dong) */
static int comment_line = 0, comment_col = 0;

#define YY_USER_ACTION \
    yylloc.first_line   = yylloc.last_line = yylineno; \
    yylloc.first_column = yycolumn; \
    yylloc.last_column  = yycolumn + yyleng - 1; \
    yycolumn += yyleng;
%}

%option noyywrap yylineno nounput noinput
%x COMMENT

DIGIT   [0-9]
LETTER  [A-Za-z]

%%

 /* Từ khóa (phải đứng trước id) */
"begin"   { return T_BEGIN; }
"end"     { return T_END; }
"int"     { return T_INT; }
"bool"    { return T_BOOL; }
"if"      { return T_IF; }
"then"    { return T_THEN; }
"else"    { return T_ELSE; }
"do"      { return T_DO; }
"while"   { return T_WHILE; }
"for"     { return T_FOR; }
"print"   { return T_PRINT; }
"true"    { return T_TRUE; }
"false"   { return T_FALSE; }

 /* Toán tử hai ký tự */
">="      { return T_GE; }
"=="      { return T_EQ; }

 /*  Ký tự đơn: trả về chính ký tự  */
"+"|"*"|">"|"="|"("|")"|"{"|"}"|";"   { return yytext[0]; }

 /* Định danh và số.
    Lexer chỉ nhận dạng token theo luật hợp lệ (khớp dài nhất, maximal munch):
      id  = [A-Za-z]+[0-9]*      num = [0-9]+
    Lexer KHÔNG kiểm tra "id có đúng dạng hay không". Chuỗi như 1abc, a1b,
    ab1cc được cắt tự nhiên thành nhiều token liền nhau:
      1abc  -> T_NUM(1) T_ID(abc)
      a1b   -> T_ID(a1) T_ID(b)
      ab1cc -> T_ID(ab1) T_ID(cc)
    Các dãy token này không khớp văn phạm nên PARSER sẽ báo lỗi cú pháp. */
{LETTER}+{DIGIT}*  { yylval.sval = strdup(yytext); return T_ID; }

{DIGIT}+  {
    /* Kiểm tra miền giá trị của hằng số là việc của lexer (lỗi từ vựng) */
    errno = 0;
    long v = strtol(yytext, NULL, 10);
    if (errno != 0 || v > INT_MAX) {
        error_report("lexical error", yylloc.first_line, yylloc.first_column,
                     "integer literal too large");
        v = 0;
    }
    yylval.ival = (int)v;
    return T_NUM;
}

 /* Khoảng trắng và comment */
\n          { yycolumn = 1; }
[ \t\r]+    { /* bỏ qua */ }

"//"[^\n]*  { /* comment một dòng, không nuốt \n */ }

"/*"        {
    comment_line = yylloc.first_line;
    comment_col  = yylloc.first_column;
    BEGIN(COMMENT);
}
<COMMENT>"*/"      { BEGIN(INITIAL); }
<COMMENT>[^*\n]+   { /* bỏ qua nội dung */ }
<COMMENT>"*"       { /* dấu * không kèm / */ }
<COMMENT>\n        { yycolumn = 1; }
<COMMENT><<EOF>>   {
    error_report("lexical error", comment_line, comment_col,
                 "unterminated comment");
    BEGIN(INITIAL);
    yyterminate();
}

 /* Ký tự không thuộc bảng chữ cái của UPL (ví dụ: @ # _ < ...).
    Đây mới là lỗi từ vựng thật sự: không có token nào chứa ký tự này.
    Báo lỗi rồi bỏ qua ký tự và đọc tiếp. */
.   {
    char msg[64];
    snprintf(msg, sizeof msg, "unexpected character '%s'", yytext);
    error_report("lexical error", yylloc.first_line, yylloc.first_column, msg);
}

%%