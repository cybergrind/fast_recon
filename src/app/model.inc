; app/model.inc — model id → display name and context window.
;
; Layout: a static table of {len, id_ptr, display_len, display_ptr, window_lo, window_hi}.
; We keep it tiny — only the few model ids we actually surface.


; _model_lookup(rdi=id, rsi=id_len) -> rax = ptr to record, or 0 if not found.
_model_lookup:
    push    rbx
    push    rbp
    mov     rbx, rdi
    mov     rbp, rsi
    lea     r8, [_model_table]
    lea     r9, [_model_table_end]
.loop:
    cmp     r8, r9
    jae     .nope
    cmp     qword [r8 + 8], rbp
    jne     .next
    push    r8
    push    r9
    mov     rdi, rbx
    mov     rsi, qword [r8]
    mov     rdx, rbp
    call    memcmp
    pop     r9
    pop     r8
    test    rax, rax
    jz      .hit
.next:
    add     r8, _MODEL_REC_BYTES
    jmp     .loop
.hit:
    mov     rax, r8
    pop     rbp
    pop     rbx
    ret
.nope:
    xor     eax, eax
    pop     rbp
    pop     rbx
    ret

; model_display_name(rdi=id, rsi=id_len, rdx=outbuf, rcx=cap) -> rax = len.
; If id is unknown, copies id verbatim (truncated to cap).
model_display_name:
    push    rbx
    push    r12
    push    r13
    push    r14
    mov     rbx, rdi                 ; id_ptr
    mov     r12, rsi                 ; id_len
    mov     r13, rdx                 ; outbuf
    mov     r14, rcx                 ; cap

    call    _model_lookup
    test    rax, rax
    jz      .fallback
    mov     r8, qword [rax + 16]     ; disp_ptr
    mov     r9, qword [rax + 24]     ; disp_len
    cmp     r9, r14
    jbe     .copy
    mov     r9, r14
.copy:
    mov     rdi, r13
    mov     rsi, r8
    mov     rdx, r9
    call    memcpy
    mov     rax, r9
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret

.fallback:
    mov     rdx, r12
    cmp     rdx, r14
    jbe     .fb_copy
    mov     rdx, r14
.fb_copy:
    mov     rdi, r13
    mov     rsi, rbx
    push    rdx
    call    memcpy
    pop     rax
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret

; model_context_window(rdi=id, rsi=id_len) -> rax = window (default 200000).
model_context_window:
    call    _model_lookup
    test    rax, rax
    jz      .default
    mov     rax, qword [rax + 32]
    ret
.default:
    mov     rax, 200000
    ret
