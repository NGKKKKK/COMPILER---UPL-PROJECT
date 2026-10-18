#!/usr/bin/env bash
# Chạy: make test   (hoặc bash tests/run_tests.sh từ bất kỳ đâu)
#
# Mỗi thư mục test kiểm tra đúng một nhiệm vụ:
#   lexical_ok/       LEXER  : chạy --tokens, không được có lỗi từ vựng
#                              (nếu có file .tokens cạnh file .upl thì so dãy token)
#   lexical_errors/   LEXER  : chạy --tokens, số lỗi từ vựng = "// EXPECT: N"
#   valid/            PARSER : chương trình đúng, mã thoát 0
#   syntax_errors/    PARSER : số lỗi (từ vựng + cú pháp) = "// EXPECT: N"
cd "$(dirname "$0")/.." || exit 2
BIN=build/upl
pass=0; fail=0

# Kiểm tra bản build đã có và chạy được
if [ ! -x "$BIN" ]; then
    echo "Không tìm thấy $BIN (hoặc thiếu quyền thực thi). Hãy chạy: make" >&2
    exit 2
fi

# Xóa ký tự CR (xuống dòng kiểu Windows) khỏi chuỗi đọc từ file test
strip_cr() { tr -d '\r'; }

ok()   { echo "PASS  $1"; pass=$((pass+1)); }
bad()  { echo "FAIL  $1 ($2)"; fail=$((fail+1)); }

# run_dir <thư mục con> <mã thoát mong đợi> [cờ cho build/upl]
run_dir() {
    for f in tests/$1/*.upl; do
        [ -e "$f" ] || continue
        "$BIN" $3 "$f" >/dev/null 2>&1
        code=$?
        if [ "$code" -eq "$2" ]; then ok "$f"
        else bad "$f" "exit $code, mong đợi $2"; fi
        # Nếu có file .tokens: so sánh dãy tên token (lexer)
        exp="${f%.upl}.tokens"
        if [ -e "$exp" ]; then
            got=$("$BIN" --tokens "$f" 2>/dev/null | grep -E '^[0-9]+:' | awk '{print $2}' | strip_cr)
            want=$(strip_cr < "$exp")
            if [ "$got" = "$want" ]; then ok "$exp"
            else bad "$exp" "dãy token khác mong đợi"; fi
        fi
    done
}

# check_errors <thư mục con> [cờ cho build/upl]
# So số lỗi báo ra với dòng "EXPECT: N" trong file
check_errors() {
    for f in tests/$1/*.upl; do
        [ -e "$f" ] || continue
        want=$(grep -m1 -oE 'EXPECT: [0-9]+' "$f" | grep -oE '[0-9]+' | strip_cr)
        [ -n "$want" ] || { echo "SKIP  $f (thiếu // EXPECT: N)"; continue; }
        got=$("$BIN" $2 "$f" 2>&1 | grep -oE 'Found [0-9]+' | grep -oE '[0-9]+' | strip_cr)
        got=${got:-0}
        if [ "$got" -eq "$want" ]; then ok "$f ($got lỗi)"
        else bad "$f" "báo $got lỗi, mong đợi $want"; fi
    done
}

echo "== Lexer =="
run_dir lexical_ok 0 --tokens
check_errors lexical_errors --tokens

echo "== Parser =="
run_dir valid 0
check_errors syntax_errors

echo "----"
echo "Passed: $pass, Failed: $fail"
[ "$fail" -eq 0 ]
