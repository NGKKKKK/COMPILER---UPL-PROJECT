# UPL Compiler (BTL01 và BTL02)

Trình biên dịch **UPL** dùng **Flex** và **Bison** để phân tích từ vựng, cú pháp
và dựng AST. Giai đoạn 2 kiểm tra ngữ nghĩa, sinh mã ba địa chỉ (TAC) và
dịch TAC sang bytecode JVM qua **Jasmin**.

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
11. [Biên dịch và chạy trên JVM](#11-biên-dịch-và-chạy-trên-jvm)

---

## 1. Tính năng

- **Lexer** (Flex): nhận dạng từ khóa, định danh, hằng số, toán tử; bỏ qua khoảng
  trắng và comment kiểu Java (`//` và `/* ... */`); theo dõi vị trí dòng/cột.
- **Parser** (Bison): kiểm tra chương trình theo văn phạm phi ngữ cảnh của UPL,
  dựng AST, **báo được nhiều lỗi trong một lần chạy** nhờ phục hồi lỗi.
- **AST**: định nghĩa node, hàm tạo, in cây và giải phóng bộ nhớ.
- **Biên dịch JVM**: kiểm tra phạm vi và kiểu, xuất TAC, sinh Jasmin và lắp ráp lớp JVM.
- **Báo lỗi tập trung**: mọi lỗi đều đi qua một chỗ, định dạng thống nhất
  `file:dòng:cột: loại lỗi: mô tả`, trong đó:
  - `file`: đường dẫn file UPL đang phân tích.
  - `dòng`: số thứ tự dòng chứa lỗi trong file, **đếm từ 1**.
  - `cột`: vị trí ký tự đầu tiên của token gây lỗi trên dòng đó, **đếm từ 1**
    (mỗi ký tự, kể cả dấu cách và tab, tính là 1 cột).
  - `loại lỗi`: `lexical error` (lỗi từ vựng, do lexer) hoặc `syntax error`
    (lỗi cú pháp, do parser) hoặc `semantic error` (lỗi ngữ nghĩa).
  - `mô tả`: nội dung lỗi, ví dụ `unexpected character '@'`.

  Ví dụ `prog.upl:4:9: lexical error: unexpected character '@'` nghĩa là lỗi ở
  dòng 4, cột 9 của file `prog.upl`.

## 2. Yêu cầu cài đặt

`gcc`, GNU Make 4.3 trở lên, `flex`, `bison`. Cần `java` và `jasmin`
trong `PATH` để biên dịch lớp JVM và chạy bộ kiểm thử đầy đủ.

Ubuntu / WSL:

```bash
sudo apt update
sudo apt install gcc make flex bison default-jre-headless jasmin-sable
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
build/upl --compile <thư-mục-đầu-ra> <file.upl>
```

| Cách chạy                     | Tác dụng                                                         |
| ----------------------------- | ---------------------------------------------------------------- |
| `build/upl file.upl`          | Chạy lexer + parser; nếu không có lỗi thì in AST                 |
| `build/upl --quiet file.upl`  | Như trên nhưng **không in AST**, chỉ in `OK: no errors.`         |
| `build/upl --tokens file.upl` | **Chỉ chạy lexer**, in từng token (dùng để kiểm tra riêng lexer) |

Chế độ `--compile` sinh TAC và lớp JVM. Xem mục [11](#11-biên-dịch-và-chạy-trên-jvm).

Chế độ `--tokens` in mỗi token trên một dòng gồm vị trí `dòng:cột`, tên token và
lexeme (đoạn chữ gốc). Cuối cùng in `Lexing OK.` hoặc `Found N error(s).`.

`make examples` tạo hai file cho mỗi ví dụ trong `examples/output/`:
`<tên>.tokens.txt` (kết quả lexer) và `<tên>.txt` (kết quả parser + AST).

**Mã thoát:**

| Mã  | Ý nghĩa                                                |
| --- | ------------------------------------------------------ |
| `0` | Không có lỗi                                           |
| `1` | Có lỗi từ vựng, cú pháp hoặc ngữ nghĩa                |
| `2` | Sai cách dùng, lỗi truy cập file hoặc lỗi sinh mã JVM |

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
- Mỗi khối có phạm vi riêng. Khai báo trong phần khởi tạo của `for` có hiệu lực
  đến hết vòng lặp. Điều kiện `do ... while` nằm ngoài phạm vi khối thân.
- Biến khai báo thiếu biểu thức khởi tạo nhận `0` với `int` hoặc `false` với `bool`.
  Biến mới chỉ có hiệu lực sau khi kiểm tra biểu thức khởi tạo.

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

**Lỗi ngữ nghĩa** (chế độ `--compile`, `semantic error`) được báo kèm dòng,
cột và nguyên nhân. Trình biên dịch tiếp tục kiểm tra các câu lệnh còn lại và
chỉ sinh TAC và lớp JVM khi mã nguồn qua cả ba pha kiểm tra.

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
| `tests/valid/`          | Parser | mặc định  | Mã thoát `0`                                                            |
| `tests/syntax_errors/`  | Parser | mặc định  | Số lỗi bằng `// EXPECT: N` |
| `tests/phase2/valid/` | JVM | `--compile` rồi `java -Xverify:all` | So sánh đầu ra với file `.out` |
| `tests/phase2/semantic_errors/` | Ngữ nghĩa | `--compile` | Mã thoát 1, chẩn đoán khớp file `.err` và không sinh file |

Dòng `// EXPECT: N` nằm ở đầu file test, `N` là số lỗi chương trình phải báo.
Kết quả cuối cùng in `Passed: ..., Failed: ...`.

Thư mục `examples/` chứa các chương trình UPL minh họa (đúng, lỗi từ vựng, lỗi
định danh); chạy `make examples` để sinh kết quả.

## 10. Cấu trúc thư mục

| Đường dẫn          | Nội dung                                                                               |
| ------------------ | -------------------------------------------------------------------------------------- |
| `docs/`            | Báo cáo văn phạm BTL01 (`main.tex`, `main.pdf`) và báo cáo triển khai BTL02 (`main_part_2.tex`) |
| `src/lexer/upl.l`  | Luật Flex: token, bỏ comment, lỗi từ vựng, theo dõi dòng/cột                           |
| `src/parser/upl.y` | Luật Bison: văn phạm, dựng AST, phục hồi lỗi                                           |
| `src/ast/`         | `ast.h` (định nghĩa node), `ast.c` (tạo/giải phóng), `ast_print.c` (in cây)            |
| `src/common/`      | `location.h` (vị trí), `error.c/.h` (báo lỗi tập trung), `token_name.c/.h` (tên token) |
| `src/main.c`       | Xử lý dòng lệnh và điều phối các pha                                                  |
| `src/semantic.c`   | Bảng ký hiệu và kiểm tra ngữ nghĩa                                                     |
| `src/tac.c`        | Sinh và xuất TAC                                                                      |
| `src/jvm.c`        | Dịch TAC sang Jasmin và gọi trình lắp ráp lớp JVM                                      |
| `tests/`           | `lexical_ok/`, `lexical_errors/`, `valid/`, `syntax_errors/`, `phase2/`, `run_tests.sh`           |
| `examples/`        | Chương trình UPL minh họa và `output/` (kết quả chạy), `phase2/` (TAC, Jasmin, lớp JVM mẫu)                                  |
| `build/`           | File sinh ra khi build (không đưa vào git)                                             |

## 11. Biên dịch và chạy trên JVM

Chạy từ thư mục dự án. Trình biên dịch tự tạo thư mục đầu ra, nên đường dẫn
`build/btl02-demo` phải chưa tồn tại và thư mục cha phải có sẵn.

```bash
make
build/upl --compile build/btl02-demo tests/phase2/valid/01_control_flow.upl
java -Xverify:all -cp build/btl02-demo UplProgram
```

Kết quả chương trình:

```text
26
true
```

Thư mục đầu ra chứa `UplProgram.tac`, `UplProgram.j` và `UplProgram.class`.
Bộ file mẫu được lưu trong `examples/phase2/` và chạy bằng
`java -Xverify:all -cp examples/phase2 UplProgram`.

Báo cáo dùng cùng bộ phông với báo cáo BTL01. Biên dịch bằng XeLaTeX với các
phông TeX Gyre Pagella, TeX Gyre Heros và Latin Modern Mono:

```bash
xelatex -output-directory=build docs/main_part_2.tex
```
