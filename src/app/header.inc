; app/header.inc — column header construction.
;
; The column widths declared here are the same right-pad widths emitted by
; draw_row, so labels rendered through build_header land directly above the
; data cells beneath them.

COL_NUM_W      = 4
COL_SESSION_W  = 17
COL_PROJECT_W  = 25
COL_STATUS_W   = 11
COL_MODEL_W    = 13
COL_CONTEXT_W  = 15
COL_LAST_W     = 12

COL_NUM_OFF      = 0
COL_SESSION_OFF  = COL_NUM_OFF + COL_NUM_W
COL_PROJECT_OFF  = COL_SESSION_OFF + COL_SESSION_W
COL_STATUS_OFF   = COL_PROJECT_OFF + COL_PROJECT_W
COL_MODEL_OFF    = COL_STATUS_OFF + COL_STATUS_W
COL_CONTEXT_OFF  = COL_MODEL_OFF + COL_MODEL_W
COL_LAST_OFF     = COL_CONTEXT_OFF + COL_CONTEXT_W
COL_TOTAL_W      = COL_LAST_OFF + COL_LAST_W

HDR_BUF_CAP    = 128


; pad_label_to(rdi=dst, rsi=label, rdx=label_len, rcx=width) -> rax = width
;   Copies label_len bytes from src then fills the remainder of the field
;   with 0x20 spaces. Caller guarantees label_len <= width.
pad_label_to:
    push    rbx
    push    r12
    push    r13
    mov     rbx, rdi                 ; saved dst
    mov     r12, rdx                 ; saved label_len
    mov     r13, rcx                 ; saved width

    ; memcpy(dst, label, label_len)
    call    memcpy

    ; memset(dst + label_len, ' ', width - label_len)
    lea     rdi, [rbx + r12]
    mov     esi, ' '
    mov     rdx, r13
    sub     rdx, r12
    call    memset

    mov     rax, r13
    pop     r13
    pop     r12
    pop     rbx
    ret

; build_header(rdi=dst, rsi=cap) -> rax = bytes written (0 if cap too small)
;   Renders all columns into dst by walking _col_table.
build_header:
    push    rbx
    push    r12
    push    r13

    cmp     rsi, COL_TOTAL_W
    jb      .too_small

    mov     r12, rdi                 ; cursor
    lea     r13, [_col_table]
    xor     ebx, ebx                 ; column index
.loop:
    cmp     ebx, _col_table_n
    jae     .done
    mov     rdi, r12
    mov     rsi, qword [r13]         ; label_ptr
    mov     rdx, qword [r13 + 8]     ; label_len
    mov     rcx, qword [r13 + 16]    ; width
    call    pad_label_to
    add     r12, rax
    add     r13, 24
    inc     ebx
    jmp     .loop
.done:
    mov     eax, COL_TOTAL_W
    pop     r13
    pop     r12
    pop     rbx
    ret
.too_small:
    xor     eax, eax
    pop     r13
    pop     r12
    pop     rbx
    ret

