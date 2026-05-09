; panes.inc — parse `tmux list-panes -a -F` output, '|'-delimited fields.
;
; Format string used by the caller:
;   "#{pane_id}|#{session_name}|#{pane_pid}|#{pane_current_command}"
; A '|' is unlikely to appear inside session names or command names, which
; lets us split each line into exactly four fields without ambiguity.

PANE_REC_BYTES       = 56
PANE_OFF_NAME_PTR    = 0
PANE_OFF_NAME_LEN    = 8
PANE_OFF_PID         = 16
PANE_OFF_CMD_PTR     = 24
PANE_OFF_CMD_LEN     = 32
PANE_OFF_PANE_ID_PTR = 40
PANE_OFF_PANE_ID_LEN = 48


; _split_field_pipe(rdi=cursor, rsi=line_end) -> rax = pos_of_pipe (or rsi).
_split_field_pipe:
    mov     rax, rdi
.loop:
    cmp     rax, rsi
    jae     .done
    cmp     byte [rax], '|'
    je      .done
    inc     rax
    jmp     .loop
.done:
    ret

; parse_claude_panes(rdi=buf, rsi=len, rdx=out_recs, rcx=out_cap) -> rax = n
parse_claude_panes:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    mov     rbx, rdi                 ; cursor
    mov     rbp, rdi
    add     rbp, rsi                 ; buf_end
    mov     r12, rdx                 ; out
    mov     r13, rcx                 ; out_cap
    xor     r14, r14                 ; count

.line_loop:
    cmp     rbx, rbp
    jae     .done
    cmp     r14, r13
    jae     .done

    mov     r15, rbx                 ; line_start
.find_eol:
    cmp     rbx, rbp
    jae     .have_line
    cmp     byte [rbx], 0x0A
    je      .have_line
    inc     rbx
    jmp     .find_eol
.have_line:
    ; line: [r15, rbx)
    ; field 1: pane_id
    mov     rdi, r15
    mov     rsi, rbx
    call    _split_field_pipe
    cmp     rax, rbx
    jae     .next_line
    ; pane_id range = [r15, rax). save into _pi_pane_id_*
    mov     qword [_pi_pane_id_ptr], r15
    mov     rdx, rax
    sub     rdx, r15
    mov     qword [_pi_pane_id_len], rdx

    lea     rdi, [rax + 1]           ; cursor after first '|'

    ; field 2: session_name
    mov     rsi, rbx
    call    _split_field_pipe
    cmp     rax, rbx
    jae     .next_line
    mov     qword [_pi_name_ptr], rdi
    mov     rdx, rax
    sub     rdx, rdi
    mov     qword [_pi_name_len], rdx
    lea     rdi, [rax + 1]

    ; field 3: pid
    mov     rsi, rbx
    call    _split_field_pipe
    cmp     rax, rbx
    jae     .next_line
    mov     qword [_pi_pid_ptr], rdi
    mov     rdx, rax
    sub     rdx, rdi
    mov     qword [_pi_pid_len], rdx
    lea     rdi, [rax + 1]

    ; field 4: cmd (rest of line up to rbx)
    mov     rsi, rbx
    sub     rsi, rdi                 ; cmd_len
    cmp     rsi, 6
    jne     .next_line
    push    rdi
    push    rsi
    mov     rdx, 6
    lea     rsi, [str_claude]
    call    memcmp
    pop     rsi
    pop     rdi
    test    rax, rax
    jnz     .next_line
    mov     qword [_pi_cmd_ptr], rdi

    ; parse pid
    push    rdi
    mov     rdi, qword [_pi_pid_ptr]
    mov     rsi, qword [_pi_pid_len]
    call    parse_u64
    pop     rdi
    mov     qword [_pi_pid_val], rax

    ; --- write record ---
    mov     rax, qword [_pi_name_ptr]
    mov     qword [r12 + PANE_OFF_NAME_PTR], rax
    mov     rax, qword [_pi_name_len]
    mov     qword [r12 + PANE_OFF_NAME_LEN], rax
    mov     rax, qword [_pi_pid_val]
    mov     qword [r12 + PANE_OFF_PID], rax
    mov     rax, qword [_pi_cmd_ptr]
    mov     qword [r12 + PANE_OFF_CMD_PTR], rax
    mov     qword [r12 + PANE_OFF_CMD_LEN], 6
    mov     rax, qword [_pi_pane_id_ptr]
    mov     qword [r12 + PANE_OFF_PANE_ID_PTR], rax
    mov     rax, qword [_pi_pane_id_len]
    mov     qword [r12 + PANE_OFF_PANE_ID_LEN], rax

    add     r12, PANE_REC_BYTES
    inc     r14

.next_line:
    cmp     rbx, rbp
    jae     .done
    inc     rbx
    jmp     .line_loop

.done:
    mov     rax, r14
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

