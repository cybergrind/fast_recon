; test_pid_cwd.asm — pid_cwd(rdi=pid, rsi=outbuf, rdx=outcap) -> rax=len, rcx=0/-errno.
;
; We test against our own pid via readlink('/proc/self/cwd') comparison —
; reading /proc/getpid()/cwd should give the same answer as /proc/self/cwd.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    call    arena_init
    ; -- reference: /proc/self/cwd via direct readlink --
    lea     rdi, [path_self_cwd]
    lea     rsi, [refbuf]
    mov     edx, 1024
    call    sys_readlink
    test    rax, rax
    js      .die
    mov     r12, rax                 ; reference length

    ; -- pid_cwd(getpid(), outbuf, 1024) --
    mov     eax, SYS_getpid
    syscall
    mov     r13, rax                 ; pid
    mov     rdi, r13
    lea     rsi, [outbuf]
    mov     edx, 1024
    call    pid_cwd
    assert_eq_n 1, rcx, 0
    assert_eq_n 2, rax, r12

    ; -- bytes match the reference --
    lea     rdi, [outbuf]
    lea     rsi, [refbuf]
    mov     rdx, r12
    call    memcmp
    assert_eq_n 3, rax, 0

    test_pass
.die:
    mov     edi, EXIT_FAIL
    mov     eax, SYS_exit
    syscall

include '../src/lib.inc'

segment readable
path_self_cwd db '/proc/self/cwd', 0

segment readable writeable
refbuf rb 1024
outbuf rb 1024
