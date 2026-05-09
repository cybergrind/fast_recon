; test_strlen.asm — strlen(rdi) returns count of bytes before the NUL.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; case 1: empty string -> 0
    lea     rdi, [s_empty]
    call    strlen
    assert_eq rax, 0

    ; case 2: "a" -> 1
    lea     rdi, [s_a]
    call    strlen
    assert_eq rax, 1

    ; case 3: "hello" -> 5
    lea     rdi, [s_hello]
    call    strlen
    assert_eq rax, 5

    ; case 4: 32-byte payload, NUL at 32
    lea     rdi, [s_32]
    call    strlen
    assert_eq rax, 32

    test_pass

include '../src/lib.inc'

segment readable
s_empty db 0
s_a     db 'a', 0
s_hello db 'hello', 0
s_32    db '0123456789abcdef0123456789abcdef', 0
