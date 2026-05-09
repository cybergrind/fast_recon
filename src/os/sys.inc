; sys.inc — thin syscall wrappers.
;
; Linux x86-64 syscall ABI: nr in rax; args in rdi, rsi, rdx, r10, r8, r9.
; Return value in rax (negative = -errno). User-side ABI is SysV: args in
; rdi, rsi, rdx, rcx, r8, r9 — so for syscalls taking >= 4 args we must
; move rcx -> r10 ourselves.
;
; All wrappers preserve callee-saved regs (the syscall instruction itself
; only clobbers rcx and r11, and we don't touch rbx/rbp/r12-r15).


; sys_open(rdi=path0, rsi=flags, rdx=mode) -> rax
sys_open:
    mov     eax, SYS_open
    syscall
    ret

; sys_close(rdi=fd) -> rax
sys_close:
    mov     eax, SYS_close
    syscall
    ret

; sys_read(rdi=fd, rsi=buf, rdx=len) -> rax
sys_read:
    mov     eax, SYS_read
    syscall
    ret

; sys_write(rdi=fd, rsi=buf, rdx=len) -> rax
sys_write:
    mov     eax, SYS_write
    syscall
    ret

; sys_fstat(rdi=fd, rsi=statbuf) -> rax
sys_fstat:
    mov     eax, SYS_fstat
    syscall
    ret

; sys_lseek(rdi=fd, rsi=off, rdx=whence) -> rax
sys_lseek:
    mov     eax, SYS_lseek
    syscall
    ret

; sys_exit(rdi=status) -> never returns
sys_exit:
    mov     eax, SYS_exit_group
    syscall
    ; unreachable
    ret

; sys_readlink(rdi=path, rsi=buf, rdx=bufsiz) -> rax = len or -errno
sys_readlink:
    mov     eax, SYS_readlink
    syscall
    ret

; sys_clock_gettime(rdi=clk_id, rsi=&timespec) -> rax
sys_clock_gettime:
    mov     eax, SYS_clock_gettime
    syscall
    ret

; sys_stat(rdi=path0, rsi=&statbuf) -> rax
sys_stat:
    mov     eax, SYS_stat
    syscall
    ret

; sys_getdents64(rdi=fd, rsi=buf, rdx=count) -> rax = bytes
sys_getdents64:
    mov     eax, SYS_getdents64
    syscall
    ret
