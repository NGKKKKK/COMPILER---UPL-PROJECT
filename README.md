# UPL Compiler (BTL01)

Bộ phân tích từ vựng, phân tích cú pháp và dựng cây cú pháp trừu tượng (AST)
cho ngôn ngữ lập trình **UPL**, viết bằng **Flex** (lexer), **Bison** (parser) và C.

Nhóm thực hiện:

| MSSV     | Họ tên           |
| -------- | ---------------- |
| 24021485 | Lã Việt Hoàng    |
| 24021533 | Nguyễn Đức Khiêm |

## Mục lục

1. [Tính năng](#1-tính-năng)
2. [Yêu cầu cài đặt](#2-yêu-cầu-cài-đặt)
3. [Build và chạy](#3-build-và-chạy)
4. [Ngôn ngữ UPL](#4-ngôn-ngữ-upl)
5. [Kiến trúc](#5-kiến-trúc)
6. [Phân chia nhiệm vụ lexer và parser](#6-phân-chia-nhiệm-vụ-lexer-và-parser)
7. [Xử lý lỗi](#7-xử-lý-lỗi)
8. [AST](#8-ast)
9. [Kiểm thử](#9-kiểm-thử)
10. [Cấu trúc thư mục](#10-cấu-trúc-thư-mục)

---

## 1. Tính năng

- **Lexer** (Flex): nhận dạng từ khóa, định danh, hằng số, toán tử; bỏ qua khoảng
  trắng và comment kiểu Java (`//` và `/* ... */`); theo dõi vị trí dòng/cột.
- **Parser** (Bison): kiểm tra chương trình theo văn phạm phi ngữ cảnh của UPL,
  dựng AST, **báo được nhiều lỗi trong một lần chạy** nhờ phục hồi lỗi.
- **AST**: định nghĩa node, hàm tạo, in cây và giải phóng bộ nhớ.
- **Báo lỗi tập trung**: mọi lỗi đều đi qua một chỗ, định dạng thống nhất
  `file:dòng:cột: loại lỗi: mô tả`, trong đó:
  - `file`: đường dẫn file UPL đang phân tích.
  - `dòng`: số thứ tự dòng chứa lỗi trong file, **đếm từ 1**.
  - `cột`: vị trí ký tự đầu tiên của token gây lỗi trên dòng đó, **đếm từ 1**
    (mỗi ký tự, kể cả dấu cách và tab, tính là 1 cột).
  - `loại lỗi`: `lexical error` (lỗi từ vựng, do lexer) hoặc `syntax error`
    (lỗi cú pháp, do parser).
  - `mô tả`: nội dung lỗi, ví dụ `unexpected character '@'`.

  Ví dụ `prog.upl:4:9: lexical error: unexpected character '@'` nghĩa là lỗi ở
  dòng 4, cột 9 của file `prog.upl`.

## 2. Yêu cầu cài đặt

`gcc`, `make`, `flex`, `bison`.

Ubuntu / WSL:

```bash
sudo apt update
sudo apt install gcc make flex bison
```

## 3. Build và chạy

```bash
make                              # build -> build/upl
make test                         # chạy bộ kiểm thử tự động
make examples                     # chạy các ví dụ, ghi kết quả (lexer+parser) vào examples/output/.txt
make clean                        # xóa thư mục build
```

Chạy chương trình:

```bash
build/upl [--tokens] [--quiet] <file.upl>
```

| Cách chạy                     | Tác dụng                                                         |
| ----------------------------- | ---------------------------------------------------------------- |
| `build/upl file.upl`          | Chạy lexer + parser; nếu không có lỗi thì in AST                 |
| `build/upl --quiet file.upl`  | Như trên nhưng **không in AST**, chỉ in `OK: no errors.`         |
| `build/upl --tokens file.upl` | **Chỉ chạy lexer**, in từng token (dùng để kiểm tra riêng lexer) |

Chế độ `--tokens` in mỗi token trên một dòng gồm vị trí `dòng:cột`, tên token và
lexeme (đoạn chữ gốc). Cuối cùng in `Lexing OK.` hoặc `Found N error(s).`.

`make examples` tạo hai file cho mỗi ví dụ trong `examples/output/`:
`<tên>.tokens.txt` (kết quả lexer) và `<tên>.txt` (kết quả parser + AST).

**Mã thoát:**

| Mã  | Ý nghĩa                                                |
| --- | ------------------------------------------------------ |
| `0` | Không có lỗi                                           |
| `1` | Có lỗi (từ vựng hoặc cú pháp)                          |
| `2` | Sai cách dùng (thiếu tên file) hoặc không mở được file |

**Xem mã thoát:** mã thoát không tự hiện ra màn hình. Trong bash (Ubuntu/WSL), gõ
`echo $?` **ngay sau** lệnh chạy để xem mã của lệnh vừa chạy:

```bash
build/upl tests/valid/01_de_bai.upl
echo $?        # in 0: chương trình đúng

build/upl examples/04_identifier_errors.upl
echo $?        # in 1: có lỗi

build/upl
echo $?        # in 2: thiếu tên file
```

Lưu ý: `echo $?` chỉ cho mã của lệnh liền trước, nên chạy thêm bất kỳ lệnh nào xen
giữa thì mã bị ghi đè. Các script kiểm thử (`tests/run_tests.sh`) cũng dựa vào mã
thoát này để phân biệt chương trình đúng và chương trình có lỗi.

## 4. Ngôn ngữ UPL

Ví dụ một chương trình UPL:

```
begin
  int x = 1;
  bool a;                      // comment một dòng
  /* comment
     nhiều dòng */
  if (x >= 1) then {
    print(x);
  } else {
    x = x + 1;
  }
  do {
    x = x * 2;
  } while (x > 10);
  for (int i = 0; i > 5; i = i + 1) {
    print(i);
  }
end
```

**Đặc điểm (theo đề bài)**:

- Chương trình bắt đầu bằng `begin`, kết thúc bằng `end`.
- Hai kiểu dữ liệu: `int` và `bool`. Biến có thể khởi tạo ngay khi khai báo.
- Định danh `id`: bắt đầu bằng chữ cái, nếu có chữ số thì chỉ nằm ở cuối
  (`[A-Za-z]+[0-9]*`).
- Phép so sánh: `>`, `>=`, `==`. Phép toán số học: `+`, `*` (`*` ưu tiên hơn `+`),
  dùng `( )` để đổi thứ tự tính.
- Câu lệnh: khai báo, gán, `print(expr)`, `if ... then`, `if ... then ... else`,
  `do ... while`, `for`.

**Các giả định thiết kế**:

- So sánh không có tính kết hợp. Ví dụ: `a > b > c` là lỗi cú pháp.
- Chỉ kiểm tra cú pháp. Khai báo trước khi dùng, khai báo lại, tương thích kiểu
  thuộc giai đoạn ngữ nghĩa, **chưa cài đặt**.

Văn phạm đầy đủ (37 luật sinh) - bản .PDF nằm trong `docs/main.pdf`.

## 5. Kiến trúc

```
 file .upl ──► LEXER (upl.l) ──► dãy token ──► PARSER (upl.y) ──► AST ──► in cây
                  │                                │
                  └──────────► error_report() ◄────┘
                              (src/common/error.c)
```

- Parser gọi `yylex()` của lexer để lấy token từng cái một.
- Mã token (`T_BEGIN`, `T_ID`, ...) do Bison sinh ra trong `build/parser.tab.h`,
  lexer và `token_name.c` đều `#include` file này.
- Khi parser khớp một luật, đoạn mã trong `{ ... }` gọi hàm trong `ast.c` để tạo
  node; gốc cây lưu ở biến toàn cục `ast_root`, `main.c` đọc sau khi `yyparse` xong.

## 6. Phân chia nhiệm vụ lexer và parser

**Lexer** chỉ cắt chuỗi ký tự thành token theo luật khớp dài nhất:

```
id  = [A-Za-z]+[0-9]*
num = [0-9]+
```

Lexer **không** kiểm tra "định danh có đúng dạng hay không". Các chuỗi sai dạng
bị tách tự nhiên thành nhiều token liền nhau:

| Chuỗi   | Lexer trả về           |
| ------- | ---------------------- |
| `1abc`  | `T_NUM(1)` `T_ID(abc)` |
| `a1b`   | `T_ID(a1)` `T_ID(b)`   |
| `ab1cc` | `T_ID(ab1)` `T_ID(cc)` |

**Parser** nhận dãy token và đối chiếu với văn phạm. Hai id (hoặc số rồi id) đứng
liền nhau không khớp luật nào, nên parser báo **lỗi cú pháp**. Đây là cách các
định danh sai dạng được phát hiện.

## 7. Xử lý lỗi

**Lỗi từ vựng** (do lexer, `lexical error`):

- ký tự ngoài bảng chữ cái của UPL (`@`, `#`, `_`, `<`, ...),
- hằng số vượt miền `int`,
- comment `/* ... */` không đóng.

Lexer báo lỗi rồi đọc tiếp.

**Lỗi cú pháp** (do parser, `syntax error`): mọi dãy token sai văn phạm, gồm cả
định danh sai dạng. Parser phục hồi bằng token `error` của Bison:

- ở mức câu lệnh: bỏ token cho đến dấu `;`,
- ở điều kiện `if`: bỏ token cho đến dấu `)`.

Nhờ vậy một lần chạy báo được nhiều lỗi. Nếu có lỗi thì **không in AST**.

Ví dụ: chạy `build/upl examples/04_identifier_errors.upl` (các định danh `1abc`,
`a1b`, `ab1cc` đều sai dạng) cho kết quả thực tế:

```
examples/04_identifier_errors.upl:2:7: syntax error: unexpected T_NUM, expecting T_ID
examples/04_identifier_errors.upl:3:9: syntax error: unexpected T_ID, expecting ';'
examples/04_identifier_errors.upl:5:11: syntax error: unexpected T_ID, expecting ';'
Found 3 error(s).
```

Cách đọc `file:dòng:cột`: ví dụ `2:7` là dòng 2, cột 7 (đếm từ 1) của token mà parser
gặp lúc phát hiện lỗi. Ở dòng 2, `1abc` bị lexer tách thành `T_NUM(1) T_ID(abc)`; parser
đang chờ tên biến sau `int` nhưng gặp số `1` nên báo lỗi tại cột 7.

## 8. AST

Với chương trình:

```
begin
  int y = x + 1;
end
```

`build/upl file.upl` in cây:

```
Program @1:1
  Decl int y @2:3
    BinOp(+) @2:13
      Id x @2:11
      IntLit 1 @2:15
```

`@dòng:cột` là vị trí trong mã nguồn. Các loại node (xem `src/ast/ast.h`):

| Nhóm      | Node                                                                  |
| --------- | --------------------------------------------------------------------- |
| Câu lệnh  | `Program`, `Block`, `Decl`, `Assign`, `Print`, `If`, `DoWhile`, `For` |
| Biểu thức | `BinOp` (`+ * > >= ==`), `Id`, `IntLit`, `BoolLit`                    |

## 9. Kiểm thử

```bash
make test
```

`tests/run_tests.sh` chia test theo module được kiểm tra:

| Thư mục                 | Module | Cách chạy  | Mong đợi                                                                |
| ----------------------- | ------ | ---------- | ----------------------------------------------------------------------- |
| `tests/lexical_ok/`     | Lexer  | `--tokens` | Không có lỗi từ vựng. File `.tokens` (nếu có) là dãy tên token mong đợi |
| `tests/lexical_errors/` | Lexer  | `--tokens` | Số lỗi bằng `// EXPECT: N`                                              |
| `tests/valid/`          | Parser | đầy đủ     | Mã thoát `0`                                                            |
| `tests/syntax_errors/`  | Parser | đầy đủ     | Số lỗi bằng `// EXPECT: N`                                              |

Dòng `// EXPECT: N` nằm ở đầu file test, `N` là số lỗi chương trình phải báo.
Kết quả cuối cùng in `Passed: ..., Failed: ...`.

Thư mục `examples/` chứa các chương trình UPL minh họa (đúng, lỗi từ vựng, lỗi
định danh); chạy `make examples` để sinh kết quả.

## 10. Cấu trúc thư mục

| Đường dẫn          | Nội dung                                                                               |
| ------------------ | -------------------------------------------------------------------------------------- |
| `docs/`            | Văn phạm CFG (nộp bài): `main.tex` (LaTeX) và `main.pdf`                               |
| `src/lexer/upl.l`  | Luật Flex: token, bỏ comment, lỗi từ vựng, theo dõi dòng/cột                           |
| `src/parser/upl.y` | Luật Bison: văn phạm, dựng AST, phục hồi lỗi                                           |
| `src/ast/`         | `ast.h` (định nghĩa node), `ast.c` (tạo/giải phóng), `ast_print.c` (in cây)            |
| `src/common/`      | `location.h` (vị trí), `error.c/.h` (báo lỗi tập trung), `token_name.c/.h` (tên token) |
| `src/main.c`       | Điểm vào chương trình, xử lý tùy chọn dòng lệnh                                        |
| `tests/`           | `lexical_ok/`, `lexical_errors/`, `valid/`, `syntax_errors/`, `run_tests.sh`           |
| `examples/`        | Chương trình UPL minh họa và `output/` (kết quả chạy)                                  |
| `build/`           | File sinh ra khi build (không đưa vào git)                                             |
