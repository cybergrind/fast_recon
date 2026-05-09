; app/format_tokens.inc — "Nk / Mk" or "Nk / 1M" formatting for Context col.


; format_tokens(rdi=outbuf, rsi=cap, rdx=used, rcx=window) -> rax = len.
format_tokens:
    push    rbx
    push    rbp
    push    r12
    push    r13
    mov     rbp, rdi                 ; original outbuf
    mov     rbx, rdi                 ; cursor
    mov     r12, rdx                 ; used
    mov     r13, rcx                 ; window

    ; "{used/1000}"
    mov     rax, r12
    xor     edx, edx
    mov     rcx, 1000
    div     rcx
    mov     rdi, rbx
    mov     rsi, rax
    call    format_u64
    add     rbx, rax
    mov     byte [rbx], 'k'
    mov     byte [rbx + 1], ' '
    mov     byte [rbx + 2], '/'
    mov     byte [rbx + 3], ' '
    add     rbx, 4

    ; window
    mov     rax, r13
    mov     rcx, 1000000
    cmp     rax, rcx
    jb      .small
    xor     edx, edx
    div     rcx
    mov     rdi, rbx
    mov     rsi, rax
    call    format_u64
    add     rbx, rax
    mov     byte [rbx], 'M'
    inc     rbx
    jmp     .done
.small:
    xor     edx, edx
    mov     rcx, 1000
    div     rcx
    mov     rdi, rbx
    mov     rsi, rax
    call    format_u64
    add     rbx, rax
    mov     byte [rbx], 'k'
    inc     rbx
.done:
    mov     rax, rbx
    sub     rax, rbp
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret
