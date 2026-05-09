; app/status.inc — pane status classification.
;
; Status codes (mirrors recon's ordinals):
;   0 = New      (zero tokens — caller decides)
;   1 = Working  ("esc to interrupt" in last pane line)
;   2 = Input    ("Esc to cancel" in last pane line)
;   3 = Idle     (anything else)

STATUS_NEW     = 0
STATUS_WORKING = 1
STATUS_INPUT   = 2
STATUS_IDLE    = 3

segment readable executable

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
    pop     rbx                      ; ← rbx now holds the buffer base
    test    rax, rax
    js      .idle
    mov     r12, rax                 ; bytes captured

    ; Find last non-empty line in [rbx, rbx + r12).
    lea     r13, [rbx + r12]
.skip_trail:
    cmp     r13, rbx
    jbe     .idle
    movzx   eax, byte [r13 - 1]
    cmp     al, 0x0A
    je      .stepback
    cmp     al, ' '
    je      .stepback
    cmp     al, 0x09
    je      .stepback
    jmp     .find_start
.stepback:
    dec     r13
    jmp     .skip_trail
.find_start:
    mov     rdi, r13
    dec     rdi
.scan_back:
    cmp     rdi, rbx
    jbe     .got_line
    movzx   eax, byte [rdi - 1]
    cmp     al, 0x0A
    je      .got_line
    dec     rdi
    jmp     .scan_back
.got_line:
    ; line range = [rdi, r13). Search for the markers.
    mov     rsi, r13
    sub     rsi, rdi                 ; line len
    push    rdi
    push    rsi
    lea     rdx, [_cps_marker_working]
    mov     ecx, _cps_marker_working_len
    call    _strstr
    pop     rsi
    pop     rdi
    test    rax, rax
    jnz     .working

    push    rdi
    push    rsi
    lea     rdx, [_cps_marker_input]
    mov     ecx, _cps_marker_input_len
    call    _strstr
    pop     rsi
    pop     rdi
    test    rax, rax
    jnz     .input

.idle:
    mov     eax, STATUS_IDLE
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret
.working:
    mov     eax, STATUS_WORKING
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret
.input:
    mov     eax, STATUS_INPUT
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

segment readable
_cps_path_tmux        db '/usr/bin/tmux', 0
_cps_arg_cap          db 'capture-pane', 0
_cps_arg_t            db '-t', 0
_cps_arg_p            db '-p', 0
_cps_marker_working   db 'esc to interrupt'
_cps_marker_working_len = $ - _cps_marker_working
_cps_marker_input     db 'Esc to cancel'
_cps_marker_input_len = $ - _cps_marker_input

_CPS_OUT_BYTES = 16384

segment readable writeable
align 8
_cps_id_buf rb 32
_cps_argv   rq 6
