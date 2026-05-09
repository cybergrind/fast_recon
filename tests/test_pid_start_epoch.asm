; test_pid_start_epoch.asm
;
; pid_start_epoch(rdi=pid) -> rax = wall-clock epoch seconds, rcx = 0/-errno.
;
; Reads /proc/{pid}/stat field 22 (start jiffies since boot) and combines
; with /proc/stat btime + HZ. The result is the absolute time the process
; was started.
;
; Self-test: invoking on the running pid must return a value <= now (CLOCK_REALTIME)
; and >= 1700000000 (Nov 2023). Bogus pid must return rcx<0.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; --- self pid ---
    mov     eax, SYS_getpid
    syscall
    mov     r12, rax                     ; pid

    ; now (epoch secs)
    call    now_epoch_secs
    mov     r13, rax                     ; now

    mov     rdi, r12
    call    pid_start_epoch
    assert_eq_n 1, rcx, 0

    ; rax should be in [1700000000, now]
    mov     rcx, 1700000000
    cmp     rax, rcx
    jbe     .die_low
    cmp     rax, r13
    ja      .die_high

    ; bogus pid (impossibly large) returns rcx < 0
    mov     rdi, 0x7FFFFFFE
    call    pid_start_epoch
    test    rcx, rcx
    jns     .die_bogus

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
