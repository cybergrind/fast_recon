; test_memcmp.asm — memcmp(rdi, rsi, rdx) -> rax: 0 equal, <0 a<b, >0 a>b.
; We test via sign — assert_eq with literal 0 / 1 / -1 (after sign-normalize).

format ELF64 executable 3
include '../src/common.inc'

; sign(rax) -> rax in {-1, 0, 1}
macro sign_rax {
    local   .neg, .done
    test    rax, rax
    jz      .done
    js      .neg
    mov     eax, 1
    jmp     .done
.neg:
    mov     rax, -1
.done:
}

segment readable executable
entry $
    ; len 0 always equal
    lea     rdi, [a_hello]
    lea     rsi, [b_world]
    xor     edx, edx
    call    memcmp
    assert_eq rax, 0

    ; equal 5 bytes
    lea     rdi, [a_hello]
    lea     rsi, [c_hello]
    mov     edx, 5
    call    memcmp
    assert_eq rax, 0

    ; "hello" vs "world" -> negative ('h' < 'w')
    lea     rdi, [a_hello]
    lea     rsi, [b_world]
    mov     edx, 5
    call    memcmp
    sign_rax
    assert_eq rax, -1

    ; "world" vs "hello" -> positive
    lea     rdi, [b_world]
    lea     rsi, [a_hello]
    mov     edx, 5
    call    memcmp
    sign_rax
    assert_eq rax, 1

    ; differ at byte 3: "abcDe" vs "abcZe"
    lea     rdi, [d_abcde]
    lea     rsi, [d_abcze]
    mov     edx, 5
    call    memcmp
    sign_rax
    assert_eq rax, -1

    test_pass

include '../src/lib.inc'

segment readable
a_hello db 'hello'
c_hello db 'hello'
b_world db 'world'
d_abcde db 'abcDe'
d_abcze db 'abcZe'
