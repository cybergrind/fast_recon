; test_io.asm — sys_open / sys_read / sys_close wrappers.
;
; sys_open(rdi=path0, rsi=flags, rdx=mode)  -> rax = fd or -errno
; sys_read(rdi=fd, rsi=buf, rdx=len)         -> rax = bytes or -errno
; sys_write(rdi=fd, rsi=buf, rdx=len)        -> rax = bytes or -errno
; sys_close(rdi=fd)                          -> rax = 0 or -errno

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; open /dev/zero RDONLY
    lea     rdi, [path_zero]
    mov     esi, O_RDONLY
    xor     edx, edx
    call    sys_open
    ; rax should be a non-negative fd
    test    rax, rax
    jns     .opened
    mov     edi, EXIT_FAIL
    mov     eax, SYS_exit
    syscall
.opened:
    mov     r12, rax                 ; save fd

    ; pre-fill buffer with 0xAA
    mov     byte [zbuf + 0], 0xAA
    mov     byte [zbuf + 1], 0xAA
    mov     byte [zbuf + 2], 0xAA
    mov     byte [zbuf + 3], 0xAA

    ; read 4 bytes from /dev/zero
    mov     rdi, r12
    lea     rsi, [zbuf]
    mov     edx, 4
    call    sys_read
    assert_eq rax, 4
    movzx   eax, byte [zbuf + 0]
    assert_eq rax, 0
    movzx   eax, byte [zbuf + 3]
    assert_eq rax, 0

    ; close
    mov     rdi, r12
    call    sys_close
    assert_eq rax, 0

    ; sys_write(1, msg, 0) — len 0 is a valid no-op, must return 0
    mov     edi, 1
    lea     rsi, [zbuf]
    xor     edx, edx
    call    sys_write
    assert_eq rax, 0

    test_pass

include '../src/lib.inc'

segment readable
path_zero db '/dev/zero', 0

segment readable writeable
zbuf rb 8
