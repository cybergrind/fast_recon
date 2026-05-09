; term.inc — terminal raw mode + ANSI escapes + key reader + signal sender.

segment readable executable

; ioctl(rdi=fd, rsi=req, rdx=arg) -> rax
sys_ioctl:
    mov     eax, SYS_ioctl
    syscall
    ret

; sys_kill(rdi=pid, rsi=sig) -> rax
sys_kill:
    mov     eax, SYS_kill
    syscall
    ret

; sys_poll(rdi=pollfds, rsi=nfds, rdx=timeout_ms) -> rax
sys_poll:
    mov     eax, SYS_poll
    syscall
    ret

; term_enter_raw(rdi=fd, rsi=save_buf[60]) -> rax = 0 ok, -errno on fail.
; Snapshots current termios into save_buf, then sets fd to raw mode.
term_enter_raw:
    push    rbx
    push    r12
    push    r13
    mov     r12, rdi                 ; fd
    mov     r13, rsi                 ; save_buf

    ; TCGETS into save_buf
    mov     rdx, r13
    mov     rsi, TCGETS
    call    sys_ioctl
    test    rax, rax
    js      .done

    ; copy save_buf into a working buffer (raw_buf), modify, push back.
    ; We do this in-place on a separate scratch (raw_buf) so save_buf stays
    ; pristine for restore.
    mov     rdi, rax                 ; rax=0 here, used as discardable
    lea     rdi, [_term_raw_buf]
    mov     rsi, r13
    mov     edx, TERMIOS_BYTES
    call    memcpy

    ; clear input flags
    mov     edx, dword [_term_raw_buf + TIO_OFF_IFLAG]
    and     edx, not (T_BRKINT or T_ICRNL or T_INPCK or T_ISTRIP or T_IXON)
    mov     dword [_term_raw_buf + TIO_OFF_IFLAG], edx
    ; clear output post-processing
    mov     edx, dword [_term_raw_buf + TIO_OFF_OFLAG]
    and     edx, not T_OPOST
    mov     dword [_term_raw_buf + TIO_OFF_OFLAG], edx
    ; cflag: clear CSIZE+PARENB, set CS8
    mov     edx, dword [_term_raw_buf + TIO_OFF_CFLAG]
    and     edx, not (T_CSIZE or T_PARENB)
    or      edx, T_CS8
    mov     dword [_term_raw_buf + TIO_OFF_CFLAG], edx
    ; lflag: clear ECHO|ICANON|IEXTEN|ISIG
    mov     edx, dword [_term_raw_buf + TIO_OFF_LFLAG]
    and     edx, not (T_ECHO or T_ICANON or T_IEXTEN or T_ISIG)
    mov     dword [_term_raw_buf + TIO_OFF_LFLAG], edx
    ; VMIN=0, VTIME=0 — non-blocking single-byte reads (poll governs waiting)
    mov     byte [_term_raw_buf + TIO_OFF_CC + TIO_VMIN_IDX], 0
    mov     byte [_term_raw_buf + TIO_OFF_CC + TIO_VTIME_IDX], 0

    ; TCSETS
    mov     rdi, r12
    mov     rsi, TCSETS
    lea     rdx, [_term_raw_buf]
    call    sys_ioctl

.done:
    pop     r13
    pop     r12
    pop     rbx
    ret

; term_restore(rdi=fd, rsi=save_buf) -> rax = ioctl result
term_restore:
    mov     rdx, rsi
    mov     rsi, TCSETS
    call    sys_ioctl
    ret

; term_write_str(rdi=ptr, rsi=len) -> rax (sys_write)
; Writes to stdout (fd 1).
term_write_str:
    mov     rdx, rsi
    mov     rsi, rdi
    mov     edi, 1
    call    sys_write
    ret

; term_clear -> writes "ESC[2J ESC[H" (clear screen + home cursor)
term_clear:
    lea     rdi, [_esc_clear]
    mov     rsi, _esc_clear_len
    jmp     term_write_str

; term_alt_on -> CSI ?1049h
term_alt_on:
    lea     rdi, [_esc_alt_on]
    mov     rsi, _esc_alt_on_len
    jmp     term_write_str

; term_alt_off -> CSI ?1049l
term_alt_off:
    lea     rdi, [_esc_alt_off]
    mov     rsi, _esc_alt_off_len
    jmp     term_write_str

; term_cursor_hide -> CSI ?25l
term_cursor_hide:
    lea     rdi, [_esc_cur_hide]
    mov     rsi, _esc_cur_hide_len
    jmp     term_write_str

; term_cursor_show -> CSI ?25h
term_cursor_show:
    lea     rdi, [_esc_cur_show]
    mov     rsi, _esc_cur_show_len
    jmp     term_write_str

; term_inverse -> CSI 7m
term_inverse:
    lea     rdi, [_esc_inv]
    mov     rsi, _esc_inv_len
    jmp     term_write_str

; term_reset -> CSI 0m
term_reset:
    lea     rdi, [_esc_reset]
    mov     rsi, _esc_reset_len
    jmp     term_write_str

; term_select_bg_on -> CSI 48;5;238m  (subtle dark-grey background)
term_select_bg_on:
    lea     rdi, [_esc_sel_bg_on]
    mov     rsi, _esc_sel_bg_on_len
    jmp     term_write_str

; term_select_bg_off -> CSI 49m (default background, fg untouched)
term_select_bg_off:
    lea     rdi, [_esc_sel_bg_off]
    mov     rsi, _esc_sel_bg_off_len
    jmp     term_write_str

; term_get_cols() -> rax = terminal columns (defaults to 80 on failure)
term_get_cols:
    sub     rsp, 16                  ; struct winsize { u16 row, col, xp, yp }
    mov     edi, 1
    mov     rsi, TIOCGWINSZ
    mov     rdx, rsp
    call    sys_ioctl
    test    rax, rax
    js      .fallback
    movzx   eax, word [rsp + 2]
    test    eax, eax
    jnz     .out
.fallback:
    mov     eax, 80
.out:
    add     rsp, 16
    ret

; term_move(rdi=row1, rsi=col1) — emit CSI <row>;<col>H. 1-based.
term_move:
    push    rbx
    push    r12
    mov     rbx, rdi                 ; row
    mov     r12, rsi                 ; col
    lea     rdi, [_move_buf]
    mov     byte [rdi], 0x1B
    mov     byte [rdi + 1], '['
    add     rdi, 2
    mov     rsi, rbx
    call    format_u64
    add     rdi, rax
    mov     byte [rdi], ';'
    inc     rdi
    mov     rsi, r12
    call    format_u64
    add     rdi, rax
    mov     byte [rdi], 'H'
    inc     rdi
    lea     rax, [_move_buf]
    sub     rdi, rax                 ; len
    mov     rsi, rdi
    lea     rdi, [_move_buf]
    call    term_write_str
    pop     r12
    pop     rbx
    ret

; read_key(rdi=fd, rsi=timeout_ms) -> rax: byte (0..255) or -1 timeout / no data.
; Uses poll() to wait, then read 1 byte. If poll says POLLIN, we read; if
; the read returns 0 (EOF) or <0, we report -1.
read_key:
    push    r12
    push    r13
    mov     r12, rdi                 ; fd
    mov     r13, rsi                 ; timeout_ms

    ; build pollfd on stack: { int fd; short events; short revents; }
    sub     rsp, 16
    mov     dword [rsp], r12d
    mov     word  [rsp + 4], POLLIN
    mov     word  [rsp + 6], 0

    mov     rdi, rsp                 ; pollfds
    mov     esi, 1                   ; nfds
    mov     rdx, r13                 ; timeout
    call    sys_poll
    test    rax, rax
    jle     .timeout_or_err
    movzx   eax, word [rsp + 6]
    test    eax, POLLIN
    jz      .timeout_or_err

    ; read 1 byte into [rsp + 8..]
    mov     rdi, r12
    lea     rsi, [rsp + 8]
    mov     edx, 1
    call    sys_read
    cmp     rax, 1
    jne     .timeout_or_err
    movzx   eax, byte [rsp + 8]
    add     rsp, 16
    pop     r13
    pop     r12
    ret

.timeout_or_err:
    mov     rax, -1
    add     rsp, 16
    pop     r13
    pop     r12
    ret

segment readable
_esc_clear     db 0x1B,'[2J',0x1B,'[H'
_esc_clear_len = $ - _esc_clear
_esc_alt_on    db 0x1B,'[?1049h'
_esc_alt_on_len = $ - _esc_alt_on
_esc_alt_off   db 0x1B,'[?1049l'
_esc_alt_off_len = $ - _esc_alt_off
_esc_cur_hide  db 0x1B,'[?25l'
_esc_cur_hide_len = $ - _esc_cur_hide
_esc_cur_show  db 0x1B,'[?25h'
_esc_cur_show_len = $ - _esc_cur_show
_esc_inv       db 0x1B,'[7m'
_esc_inv_len   = $ - _esc_inv
_esc_reset     db 0x1B,'[0m'
_esc_reset_len = $ - _esc_reset
_esc_sel_bg_on  db 0x1B,'[48;5;238m'
_esc_sel_bg_on_len = $ - _esc_sel_bg_on
_esc_sel_bg_off db 0x1B,'[49m'
_esc_sel_bg_off_len = $ - _esc_sel_bg_off

segment readable writeable
_term_raw_buf  rb TERMIOS_BYTES
_move_buf      rb 32
