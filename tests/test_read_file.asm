; test_read_file.asm
;
; read_file_all(rdi=path0)
;   On success: rax = ptr (mmap'd, read-only), rdx = length, rcx = 0
;   On failure: rax = 0, rcx = -errno (negative)
;
; Test: write a known 13-byte payload to /tmp/fast_recon_rfa, read it back
;       via read_file_all, verify bytes, unlink.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; --- create the fixture file ---
    lea     rdi, [path]
    mov     esi, O_WRONLY or O_CREAT or O_TRUNC
    mov     edx, 0644o
    mov     eax, SYS_open
    syscall
    test    rax, rax
    js      .die
    mov     r12, rax                 ; fd

    ; write 13 bytes "hello, world!"
    mov     rdi, r12
    lea     rsi, [payload]
    mov     edx, 13
    call    sys_write
    assert_eq rax, 13

    ; close
    mov     rdi, r12
    call    sys_close
    assert_eq rax, 0

    ; --- exercise read_file_all ---
    lea     rdi, [path]
    call    read_file_all
    test    rax, rax
    jz      .die                     ; ptr should be non-null on success
    assert_eq rdx, 13
    mov     r13, rax                 ; ptr

    ; verify content via memcmp
    mov     rdi, r13
    lea     rsi, [payload]
    mov     edx, 13
    call    memcmp
    assert_eq rax, 0

    ; cleanup: unlink
    lea     rdi, [path]
    mov     eax, SYS_unlink
    syscall

    test_pass
.die:
    mov     edi, EXIT_FAIL
    mov     eax, SYS_exit
    syscall

include '../src/lib.inc'

segment readable
path    db '/tmp/fast_recon_rfa_test', 0
payload db 'hello, world!'
