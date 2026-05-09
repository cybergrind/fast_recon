; test_path_mtime.asm
;
; path_mtime(rdi=path0) -> rax = mtime_secs (tv_sec), rcx = 0/-errno
;
; Wraps SYS_stat. On success rax holds st_mtime.tv_sec; on failure rcx is
; negative (the kernel error).

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; create /tmp/test_path_mtime.bin
    lea     rdi, [path0]
    mov     esi, O_WRONLY or O_CREAT or O_TRUNC
    mov     edx, 0644o
    mov     eax, SYS_open
    syscall
    test    rax, rax
    jns     .opened
    mov     edi, 30
    mov     eax, SYS_exit
    syscall
.opened:
    mov     r12, rax
    mov     rdi, r12
    call    sys_close

    ; mtime should be > 1700000000 (Nov 2023) and < 4000000000 (year 2096)
    lea     rdi, [path0]
    call    path_mtime
    assert_eq_n 1, rcx, 0
    mov     rcx, 1700000000
    cmp     rax, rcx
    jbe     .die_low
    mov     rcx, 4000000000
    cmp     rax, rcx
    jae     .die_high

    ; bogus path returns rcx<0
    lea     rdi, [bogus_path0]
    call    path_mtime
    test    rcx, rcx
    jns     .die_bogus

    ; cleanup
    lea     rdi, [path0]
    mov     eax, SYS_unlink
    syscall

    test_pass

.die_low:
    mov     edi, 10
    mov     eax, SYS_exit
    syscall
.die_high:
    mov     edi, 11
    mov     eax, SYS_exit
    syscall
.die_bogus:
    mov     edi, 12
    mov     eax, SYS_exit
    syscall

include '../src/lib.inc'

segment readable
path0       db '/tmp/test_path_mtime.bin', 0
bogus_path0 db '/tmp/this/path/does/not/exist/zz_zz', 0
