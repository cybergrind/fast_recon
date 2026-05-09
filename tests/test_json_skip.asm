; test_json_skip.asm — internal helpers used by json_find_key.
;
; json_skip_ws(rdi=p, rsi=end) -> rax = new_p (>= p, <= end).
; json_skip_string(rdi=p, rsi=end) -> rax = new_p
;   Pre: byte at p is the opening '"'. Post: rax points one past the
;   closing '"', or to end if unterminated.
; json_skip_value(rdi=p, rsi=end) -> rax = new_p
;   Skips one complete JSON value (string / number / object / array /
;   true / false / null). Whitespace before/after is the caller's problem.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; ws: "   x" -> after the spaces
    lea     rdi, [s_ws]
    lea     rsi, [s_ws + 4]
    call    json_skip_ws
    lea     rcx, [s_ws + 3]
    assert_eq rax, rcx

    ; ws empty: p==end stays
    lea     rdi, [s_ws]
    lea     rsi, [s_ws]
    call    json_skip_ws
    lea     rcx, [s_ws]
    assert_eq rax, rcx

    ; string: "abc" -> 5 (one past closing quote)
    lea     rdi, [s_str_abc]
    lea     rsi, [s_str_abc + 5]
    call    json_skip_string
    lea     rcx, [s_str_abc + 5]
    assert_eq rax, rcx

    ; string with escape: "a\"b" (6 bytes total)
    lea     rdi, [s_str_esc]
    lea     rsi, [s_str_esc + 6]
    call    json_skip_string
    lea     rcx, [s_str_esc + 6]
    assert_eq rax, rcx

    ; value: number 12345 followed by comma
    lea     rdi, [s_num]
    lea     rsi, [s_num + 6]
    call    json_skip_value
    lea     rcx, [s_num + 5]
    assert_eq rax, rcx

    ; value: nested object {"a":{"b":1}} (13 bytes)
    lea     rdi, [s_nest]
    lea     rsi, [s_nest + 13]
    call    json_skip_value
    lea     rcx, [s_nest + 13]
    assert_eq rax, rcx

    ; value: string with brace inside "a{b" (5 bytes incl. quotes)
    lea     rdi, [s_strbrace]
    lea     rsi, [s_strbrace + 5]
    call    json_skip_value
    lea     rcx, [s_strbrace + 5]
    assert_eq rax, rcx

    ; value: literal true
    lea     rdi, [s_true]
    lea     rsi, [s_true + 4]
    call    json_skip_value
    lea     rcx, [s_true + 4]
    assert_eq rax, rcx

    test_pass

include '../src/lib.inc'

segment readable
s_ws       db '   x'
s_str_abc  db '"abc"'
s_str_esc  db '"a\"b"'
s_num      db '12345,'
s_nest     db '{"a":{"b":1}}'
s_strbrace db '"a{b"'
s_true     db 'true'
