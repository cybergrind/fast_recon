; proc.inc — subprocess control: pipe, fork, execve, wait.
;
; exec_capture(rdi=argv0_ptr, rsi=argv_array, rdx=buf, rcx=buf_cap)
;   spawns child with stdout redirected to a pipe; reads up to buf_cap bytes
;   from the read end into buf; reaps child.
;   ok:  rax = bytes_read,  rdx = exit_status
;   err: rax = -errno (sign-bit set), rdx = unspecified


; --- helper: sys_pipe(rdi=int[2]) -> rax. uses SYS_pipe (22). ---
; --- helper: sys_close, sys_dup2, sys_fork, sys_execve, sys_wait4 are inline ---

exec_capture:
    push    rbx
    push    r12                      ; buf
    push    r13                      ; buf_cap
    push    r14                      ; argv0
    push    r15                      ; argv
    sub     rsp, 16                  ; pipefd[2] at [rsp]

    mov     r14, rdi
    mov     r15, rsi
    mov     r12, rdx
    mov     r13, rcx

    ; pipe(&pipefd)
    mov     rdi, rsp
    mov     eax, SYS_pipe
    syscall
    test    rax, rax
    js      .err_pipe

    ; fork()
    mov     eax, SYS_fork
    syscall
    test    rax, rax
    js      .err_fork
    jnz     .parent

    ; ---- child ----
    ; close pipe[0] (read end)
    mov     edi, [rsp]
    mov     eax, SYS_close
    syscall
    ; dup2(pipe[1], 1) — wire stdout to pipe write end
    mov     edi, [rsp + 4]
    mov     esi, 1
    mov     eax, SYS_dup2
    syscall
    ; close pipe[1] original
    mov     edi, [rsp + 4]
    mov     eax, SYS_close
    syscall
    ; execve(argv0, argv, NULL)
    mov     rdi, r14
    mov     rsi, r15
    xor     edx, edx                 ; envp = NULL
    mov     eax, SYS_execve
    syscall
    ; if exec returns, exit 127
    mov     edi, 127
    mov     eax, SYS_exit
    syscall

.parent:
    mov     rbx, rax                 ; child pid
    ; close write end so read sees EOF when child finishes
    mov     edi, [rsp + 4]
    mov     eax, SYS_close
    syscall

    ; read in a loop until full or EOF
    xor     r8, r8                   ; bytes_read
.read_loop:
    mov     rax, r13
    sub     rax, r8
    test    rax, rax
    jz      .done_read               ; buffer full
    mov     edi, [rsp]               ; read fd
    mov     rsi, r12
    add     rsi, r8
    mov     rdx, rax
    mov     eax, SYS_read
    syscall
    test    rax, rax
    jz      .done_read               ; EOF
    js      .done_read               ; error: stop
    add     r8, rax
    jmp     .read_loop
.done_read:
    ; close pipe read end
    mov     edi, [rsp]
    mov     eax, SYS_close
    syscall

    ; wait4(pid, &status, 0, NULL)
    sub     rsp, 16                  ; status int at [rsp]
    mov     dword [rsp], 0
    mov     rdi, rbx
    mov     rsi, rsp
    xor     edx, edx
    xor     r10, r10
    mov     eax, SYS_wait4
    syscall
    ; extract exit status from low byte of (status >> 8) if WIFEXITED
    mov     eax, [rsp]
    add     rsp, 16
    ; status: low byte = signal/0, bit 7 = core; if low 7 bits are zero -> WIFEXITED
    mov     ecx, eax
    and     ecx, 0x7F
    jnz     .signal_exit
    ; exited normally
    shr     eax, 8
    movzx   edx, al                  ; exit code
    jmp     .ret_ok
.signal_exit:
    ; report 128 + signal in exit field
    movzx   edx, cl
    add     edx, 128
.ret_ok:
    mov     rax, r8                  ; bytes read
    add     rsp, 16
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret

.err_pipe:
    add     rsp, 16
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret                              ; rax already -errno

.err_fork:
    ; close pipe ends to leak nothing
    mov     edi, [rsp]
    mov     eax, SYS_close
    syscall
    mov     edi, [rsp + 4]
    mov     eax, SYS_close
    syscall
    mov     rax, -1                  ; generic
    add     rsp, 16
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
