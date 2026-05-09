; test_memops.asm — memcpy and memset.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; ---- memset(buf, 0xAB, 8) ----
    lea     rdi, [buf]
    mov     esi, 0xAB
    mov     edx, 8
    call    memset
    movzx   eax, byte [buf + 0]
    assert_eq rax, 0xAB
    movzx   eax, byte [buf + 7]
    assert_eq rax, 0xAB
    ; sentinel byte at buf[8] must be untouched (still 0)
    movzx   eax, byte [buf + 8]
    assert_eq rax, 0

    ; ---- memset len=0 is no-op ----
    lea     rdi, [buf2]
    mov     esi, 0xFF
    xor     edx, edx
    call    memset
    movzx   eax, byte [buf2]
    assert_eq rax, 0

    ; ---- memcpy("HELLO", buf3, 5) ----
    lea     rdi, [buf3]
    lea     rsi, [src_hello]
    mov     edx, 5
    call    memcpy
    movzx   eax, byte [buf3 + 0]
    assert_eq rax, 'H'
    movzx   eax, byte [buf3 + 4]
    assert_eq rax, 'O'
    movzx   eax, byte [buf3 + 5]
    assert_eq rax, 0                 ; sentinel intact

    ; ---- memcpy len=0 ----
    lea     rdi, [buf4]
    lea     rsi, [src_hello]
    xor     edx, edx
    call    memcpy
    movzx   eax, byte [buf4]
    assert_eq rax, 0

    test_pass

include '../src/lib.inc'

segment readable writeable
buf      rb 16
buf2     rb 4
buf3     rb 8
buf4     rb 4

segment readable
src_hello db 'HELLO'
