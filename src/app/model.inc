; app/model.inc — model id → display name and context window.
;
; Layout: a static table of {len, id_ptr, display_len, display_ptr, window_lo, window_hi}.
; We keep it tiny — only the few model ids we actually surface.

segment readable

_m_opus_47       db 'claude-opus-4-7'
_m_opus_47_len   = $ - _m_opus_47
_d_opus_47       db 'Opus 4.7'
_d_opus_47_len   = $ - _d_opus_47

_m_opus_46       db 'claude-opus-4-6'
_m_opus_46_len   = $ - _m_opus_46
_d_opus_46       db 'Opus 4.6'
_d_opus_46_len   = $ - _d_opus_46

_m_sonnet_46     db 'claude-sonnet-4-6'
_m_sonnet_46_len = $ - _m_sonnet_46
_d_sonnet_46     db 'Sonnet 4.6'
_d_sonnet_46_len = $ - _d_sonnet_46

_m_haiku_45      db 'claude-haiku-4-5-20251001'
_m_haiku_45_len  = $ - _m_haiku_45
_d_haiku_45      db 'Haiku 4.5'
_d_haiku_45_len  = $ - _d_haiku_45

_m_sonnet_45     db 'claude-sonnet-4-5-20250514'
_m_sonnet_45_len = $ - _m_sonnet_45
_d_sonnet_45     db 'Sonnet 4.5'
_d_sonnet_45_len = $ - _d_sonnet_45

_m_opus_4        db 'claude-opus-4-20250514'
_m_opus_4_len    = $ - _m_opus_4
_d_opus_4        db 'Opus 4'
_d_opus_4_len    = $ - _d_opus_4

_m_sonnet_4      db 'claude-sonnet-4-20250514'
_m_sonnet_4_len  = $ - _m_sonnet_4
_d_sonnet_4      db 'Sonnet 4'
_d_sonnet_4_len  = $ - _d_sonnet_4

; record: id_ptr q, id_len q, disp_ptr q, disp_len q, window q  -> 40 bytes
align 8
_model_table:
    dq _m_opus_47,    _m_opus_47_len,    _d_opus_47,    _d_opus_47_len,    1000000
    dq _m_opus_46,    _m_opus_46_len,    _d_opus_46,    _d_opus_46_len,    1000000
    dq _m_sonnet_46,  _m_sonnet_46_len,  _d_sonnet_46,  _d_sonnet_46_len,  200000
    dq _m_haiku_45,   _m_haiku_45_len,   _d_haiku_45,   _d_haiku_45_len,   200000
    dq _m_sonnet_45,  _m_sonnet_45_len,  _d_sonnet_45,  _d_sonnet_45_len,  200000
    dq _m_opus_4,     _m_opus_4_len,     _d_opus_4,     _d_opus_4_len,     200000
    dq _m_sonnet_4,   _m_sonnet_4_len,   _d_sonnet_4,   _d_sonnet_4_len,   200000
_model_table_end:

_MODEL_REC_BYTES = 40

segment readable executable

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
