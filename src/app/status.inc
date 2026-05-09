; app/status.inc — pane status classification.
;
; Status codes (mirrors recon's ordinals):
;   0 = New      (zero tokens — caller decides)
;   1 = Working  (any line whose first non-WS char is a Claude spinner glyph
;                 and that line contains the U+2026 ellipsis)
;   2 = Input    (last non-empty line contains "Esc to cancel", or any line
;                 contains "❯ <digit>")
;   3 = Idle     (anything else)

STATUS_NEW     = 0
STATUS_WORKING = 1
STATUS_INPUT   = 2
STATUS_IDLE    = 3


; is_spinner_utf8(rdi=ptr, rsi=remaining_bytes) -> rax: 1 if the prefix at
; ptr decodes to one of the Claude Code activity glyphs:
;   U+2720..U+2767  (Dingbats spinners ✽✢✳✶✻ etc.) → E2 9C A0..E2 9D A7
;   U+23FA          (record glyph ⏺)               → E2 8F BA
;   U+00B7          (middle dot ·)                 → C2 B7
is_spinner_utf8:
    test    rsi, rsi
    jz      .no
    movzx   eax, byte [rdi]
    cmp     al, 0xC2
    je      .check_c2
    cmp     al, 0xE2
    jne     .no
    cmp     rsi, 3
    jb      .no
    movzx   eax, byte [rdi + 1]
    cmp     al, 0x9C
    je      .check_e29c
    cmp     al, 0x9D
    je      .check_e29d
    cmp     al, 0x8F
    je      .check_e28f
    jmp     .no
.check_e29c:
    movzx   eax, byte [rdi + 2]
    cmp     al, 0xA0
    jb      .no
    cmp     al, 0xBF
    ja      .no
    jmp     .yes
.check_e29d:
    movzx   eax, byte [rdi + 2]
    cmp     al, 0x80
    jb      .no
    cmp     al, 0xA7
    ja      .no
    jmp     .yes
.check_e28f:
    cmp     byte [rdi + 2], 0xBA
    je      .yes
    jmp     .no
.check_c2:
    cmp     rsi, 2
    jb      .no
    cmp     byte [rdi + 1], 0xB7
    je      .yes
    jmp     .no
.yes:
    mov     eax, 1
    ret
.no:
    xor     eax, eax
    ret

; pane_status_from_buf(rdi=buf, rsi=len) -> rax = STATUS_*
;
; Splits the buffer into newline-separated lines. For each line, after
; stripping leading whitespace, records whether any line contains
; (spinner-prefix + ellipsis) (Working) or (❯ + digit) (Input-select).
; After the scan, if the last non-empty line contains "Esc to cancel" the
; result is Input regardless. Otherwise Working > Input-select > Idle.
pane_status_from_buf:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    mov     r12, rdi                 ; buf base
    mov     r13, rsi                 ; len
    xor     r14d, r14d               ; saw_working
    xor     r15d, r15d               ; saw_input_select
    mov     qword [_psf_last_start], 0
    mov     qword [_psf_last_end], 0

    xor     rcx, rcx                 ; cursor
.next_line:
    cmp     rcx, r13
    jae     .done
    mov     rbx, rcx                 ; line start
.find_eol:
    cmp     rcx, r13
    jae     .got_eol
    cmp     byte [r12 + rcx], 0x0A
    je      .got_eol
    inc     rcx
    jmp     .find_eol
.got_eol:
    ; line range = [rbx, rcx); trim leading WS
    mov     rdx, rbx                 ; trim_start
.trim_lead:
    cmp     rdx, rcx
    jae     .line_advance            ; empty after trim
    movzx   eax, byte [r12 + rdx]
    cmp     al, ' '
    je      .trim_step
    cmp     al, 0x09
    je      .trim_step
    cmp     al, 0x0D
    je      .trim_step
    jmp     .trim_done
.trim_step:
    inc     rdx
    jmp     .trim_lead
.trim_done:
    mov     qword [_psf_last_start], rdx
    mov     qword [_psf_last_end], rcx

    ; --- Working test: spinner at trim_start AND ellipsis somewhere ---
    push    rcx
    push    rdx
    lea     rdi, [r12 + rdx]
    mov     rsi, rcx
    sub     rsi, rdx
    call    is_spinner_utf8
    pop     rdx
    pop     rcx
    test    eax, eax
    jz      .no_spinner
    push    rcx
    push    rdx
    lea     rdi, [r12 + rdx]
    mov     rsi, rcx
    sub     rsi, rdx
    lea     rdx, [_psf_ellipsis]
    mov     ecx, 3
    call    _strstr
    pop     rdx
    pop     rcx
    test    rax, rax
    jz      .no_spinner
    mov     r14d, 1
.no_spinner:

    ; --- Input-select: "❯" then a digit (skipping spaces) ---
    push    rcx
    push    rdx
    lea     rdi, [r12 + rdx]
    mov     rsi, rcx
    sub     rsi, rdx
    lea     rdx, [_psf_chevron]
    mov     ecx, 3
    call    _strstr
    pop     rdx
    pop     rcx
    test    rax, rax
    jz      .no_chevron
    add     rax, 3                   ; skip past the 3-byte ❯
    lea     r8, [r12 + rcx]          ; line end ptr
.skip_chev_ws:
    cmp     rax, r8
    jae     .no_chevron
    movzx   r9d, byte [rax]
    cmp     r9b, ' '
    je      .chev_step_ws
    cmp     r9b, 0x09
    je      .chev_step_ws
    jmp     .chev_check
.chev_step_ws:
    inc     rax
    jmp     .skip_chev_ws
.chev_check:
    sub     r9d, '0'
    cmp     r9d, 9
    ja      .no_chevron
    mov     r15d, 1
.no_chevron:

.line_advance:
    cmp     rcx, r13
    jae     .done
    inc     rcx
    jmp     .next_line

.done:
    ; if last non-empty contains "Esc to cancel" → INPUT
    mov     rax, qword [_psf_last_end]
    sub     rax, qword [_psf_last_start]
    test    rax, rax
    jz      .no_esc
    mov     rdi, r12
    add     rdi, qword [_psf_last_start]
    mov     rsi, rax
    lea     rdx, [_psf_marker_esc]
    mov     ecx, _psf_marker_esc_len
    call    _strstr
    test    rax, rax
    jnz     .ret_input
.no_esc:
    test    r14d, r14d
    jnz     .ret_working
    test    r15d, r15d
    jnz     .ret_input
    mov     eax, STATUS_IDLE
    jmp     .epilogue
.ret_working:
    mov     eax, STATUS_WORKING
    jmp     .epilogue
.ret_input:
    mov     eax, STATUS_INPUT
.epilogue:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

; classify_pane_status(rdi=pane_id_ptr, rsi=pane_id_len) -> rax = status code.
classify_pane_status:
    push    rbx
    push    rbp
    push    r12
    push    r13
    mov     rbx, rdi                 ; pane_id ptr
    mov     rbp, rsi                 ; pane_id len

    ; --- copy pane_id into NUL-terminated buffer ---
    mov     rdi, _cps_id_buf
    mov     rsi, rbx
    mov     rdx, rbp
    call    memcpy
    mov     byte [_cps_id_buf + rbp], 0

    ; argv: ["/usr/bin/tmux", "capture-pane", "-t", "<id>", "-p", NULL]
    mov     qword [_cps_argv + 0], _cps_path_tmux
    mov     qword [_cps_argv + 8], _cps_arg_cap
    mov     qword [_cps_argv + 16], _cps_arg_t
    mov     qword [_cps_argv + 24], _cps_id_buf
    mov     qword [_cps_argv + 32], _cps_arg_p
    mov     qword [_cps_argv + 40], 0

    ; exec_capture into the arena's CPS_OUT slice
    lea     rdi, [_cps_path_tmux]
    lea     rsi, [_cps_argv]
    arena_lea rdx, ARENA_OFF_CPS_OUT
    mov     rcx, _CPS_OUT_BYTES
    push    rdx
    call    exec_capture
    pop     rbx                      ; rbx ← buffer base
    test    rax, rax
    js      .idle
    mov     rdi, rbx
    mov     rsi, rax
    call    pane_status_from_buf
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret
.idle:
    mov     eax, STATUS_IDLE
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

