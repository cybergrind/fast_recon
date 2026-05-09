; test_parse_u64.asm
;
; parse_u64(rdi=ptr, rsi=len) -> rax = value, rdx = bytes consumed.
; Stops at first non-decimal byte. Empty / leading-non-digit: rdx=0, rax=0.
; No overflow handling needed for our use cases (token counts < 2^63).

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; "0" -> 0, consumed=1
    lea     rdi, [s_zero]
    mov     esi, 1
    call    parse_u64
    assert_eq rax, 0
    assert_eq rdx, 1

    ; "12345" -> 12345, consumed=5
    lea     rdi, [s_12345]
    mov     esi, 5
    call    parse_u64
    assert_eq rax, 12345
    assert_eq rdx, 5

    ; "42abc" len=5 -> 42, consumed=2
    lea     rdi, [s_42abc]
    mov     esi, 5
    call    parse_u64
    assert_eq rax, 42
    assert_eq rdx, 2

    ; "" -> 0, consumed=0
    lea     rdi, [s_empty]
    xor     esi, esi
    call    parse_u64
    assert_eq rax, 0
    assert_eq rdx, 0

    ; "abc" -> 0, consumed=0
    lea     rdi, [s_abc]
    mov     esi, 3
    call    parse_u64
    assert_eq rax, 0
    assert_eq rdx, 0

    ; large: 18446744073709551610 (well below 2^64-1) — but use 1234567890123 to be safe
    lea     rdi, [s_big]
    mov     esi, 13
    call    parse_u64
    mov     rcx, 1234567890123
    assert_eq rax, rcx
    assert_eq rdx, 13

    ; honour len: "999" with len=2 -> 99, consumed=2
    lea     rdi, [s_999]
    mov     esi, 2
    call    parse_u64
    assert_eq rax, 99
    assert_eq rdx, 2

    test_pass

include '../src/lib.inc'

segment readable
s_zero  db '0'
s_12345 db '12345'
s_42abc db '42abc'
s_empty db 0
s_abc   db 'abc'
s_big   db '1234567890123'
s_999   db '999'
