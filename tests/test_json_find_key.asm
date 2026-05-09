; test_json_find_key.asm — locate a top-level key, return its value range.
;
; Each assertion uses a unique exit code so a failure tells us which step.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; --- key "type" -> string value "msg" ---
    lea     rdi, [doc1]
    lea     rsi, [doc1_end]
    lea     rdx, [k_type]
    mov     ecx, 4
    call    json_find_key
    assert_eq_n 10, rcx, 0
    lea     r12, [doc1 + 8]
    assert_eq_n 11, rax, r12
    lea     r12, [doc1 + 13]
    assert_eq_n 12, rdx, r12

    ; --- key "n" -> number 42 ---
    lea     rdi, [doc1]
    lea     rsi, [doc1_end]
    lea     rdx, [k_n]
    mov     ecx, 1
    call    json_find_key
    assert_eq_n 20, rcx, 0
    lea     r12, [doc1 + 18]
    assert_eq_n 21, rax, r12
    lea     r12, [doc1 + 20]
    assert_eq_n 22, rdx, r12

    ; --- missing key -> rcx=-1 ---
    lea     rdi, [doc1]
    lea     rsi, [doc1_end]
    lea     rdx, [k_missing]
    mov     ecx, 7
    call    json_find_key
    assert_eq_n 30, rcx, -1

    ; --- nested object — only top-level "outer" matches, value = whole inner ---
    lea     rdi, [doc2]
    lea     rsi, [doc2_end]
    lea     rdx, [k_outer]
    mov     ecx, 5
    call    json_find_key
    assert_eq_n 40, rcx, 0
    lea     r12, [doc2 + 9]
    assert_eq_n 41, rax, r12
    lea     r12, [doc2 + 22]
    assert_eq_n 42, rdx, r12

    ; --- search for "x" inside the inner object slice ---
    lea     rdi, [doc2 + 9]
    lea     rsi, [doc2 + 22]
    lea     rdx, [k_x]
    mov     ecx, 1
    call    json_find_key
    assert_eq_n 50, rcx, 0

    ; --- whitespace around colon: {"a" : 7} ---
    lea     rdi, [doc3]
    lea     rsi, [doc3_end]
    lea     rdx, [k_a]
    mov     ecx, 1
    call    json_find_key
    assert_eq_n 60, rcx, 0
    lea     r12, [doc3 + 7]
    assert_eq_n 61, rax, r12
    lea     r12, [doc3 + 8]
    assert_eq_n 62, rdx, r12

    test_pass

include '../src/lib.inc'

segment readable
;                           111111111122
;                  0123456789012345678901
doc1       db     '{"type":"msg","n":42}'
doc1_end:
;                           111111111122
;                  0123456789012345678901
doc2       db     '{"outer":{"x":1,"y":2}}'
doc2_end:
doc3       db     '{"a" : 7}'
doc3_end:
k_type     db 'type'
k_n        db 'n'
k_a        db 'a'
k_outer    db 'outer'
k_x        db 'x'
k_missing  db 'nothere'
