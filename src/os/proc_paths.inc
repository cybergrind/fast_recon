; proc_paths.inc — /proc/{pid}/* helpers and small path-builder utilities.

segment readable executable

; pid_cwd(rdi=pid, rsi=outbuf, rdx=outcap) -> rax = len, rcx = 0 or -errno.
; Builds "/proc/{pid}/cwd" and readlinks it into outbuf. NO trailing NUL —
; readlink doesn't write one. Caller can use rax as the length.
pid_cwd:
    push    rbx
    push    r12
    push    r13
    mov     rbx, rsi                 ; outbuf
    mov     r12, rdx                 ; outcap

    ; build path in scratch: "/proc/" + decimal(pid) + "/cwd" + 0
    lea     r13, [_pp_scratch]
    mov     dword [r13], '/pro'
    mov     word [r13 + 4], 'c/'
    add     r13, 6
    mov     rsi, rdi                 ; pid
    mov     rdi, r13
    call    format_u64               ; rax = digits
    add     r13, rax
    mov     dword [r13], '/cwd'
    mov     byte [r13 + 4], 0

    ; readlink(scratch, outbuf, outcap)
    lea     rdi, [_pp_scratch]
    mov     rsi, rbx
    mov     rdx, r12
    call    sys_readlink
    test    rax, rax
    js      .err
    xor     ecx, ecx
    pop     r13
    pop     r12
    pop     rbx
    ret
.err:
    mov     rcx, rax
    xor     eax, eax
    pop     r13
    pop     r12
    pop     rbx
    ret

segment readable writeable
align 8
_pp_scratch rb 64
