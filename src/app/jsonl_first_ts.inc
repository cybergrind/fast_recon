; app/jsonl_first_ts.inc — read epoch of the JSONL's earliest timestamped line.
;
; Used to anchor JSONL files to the claude pid that created them. Some
; sessions begin with a `permission-mode` line that has no timestamp, so we
; scan up to JFT_MAX_LINES lines and return the timestamp of the first one
; that has it.

JFT_BUF_BYTES  = 4096
JFT_MAX_LINES  = 32


; jsonl_first_ts_epoch(rdi=path0) -> rax = epoch_secs, rcx = 0/-errno
jsonl_first_ts_epoch:
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15

    mov     rsi, O_RDONLY
    xor     edx, edx
    mov     eax, SYS_open
    syscall
    test    rax, rax
    js      .err_open
    mov     r12, rax                     ; fd

    sub     rsp, JFT_BUF_BYTES
    mov     rdi, r12
    mov     rsi, rsp
    mov     edx, JFT_BUF_BYTES
    mov     eax, SYS_read
    syscall
    mov     r13, rax                     ; bytes
    mov     rdi, r12
    mov     eax, SYS_close
    syscall
    test    r13, r13
    js      .err_buf
    jz      .err_buf

    mov     rbx, rsp                     ; line start
    lea     r14, [rsp + r13]             ; buf end
    mov     r15d, JFT_MAX_LINES

.line_loop:
    cmp     rbx, r14
    jae     .err_buf
    test    r15d, r15d
    jz      .err_buf

    ; find this line's end (next '\n' or buf end) into r13
    mov     r13, rbx
.scan_nl:
    cmp     r13, r14
    jae     .have_end
    cmp     byte [r13], 0x0A
    je      .have_end
    inc     r13
    jmp     .scan_nl
.have_end:
    ; line span = [rbx, r13). Try to extract "timestamp".
    mov     rdi, rbx
    mov     rsi, r13
    lea     rdx, [_jft_key_ts]
    mov     ecx, _jft_key_ts_len
    call    json_find_key
    test    rcx, rcx
    js      .next_line                   ; key not in this line — try next
    cmp     byte [rax], '"'
    jne     .next_line
    inc     rax                          ; past opening quote
    dec     rdx                          ; before closing quote
    cmp     rax, rdx
    ja      .next_line
    mov     rdi, rax
    mov     rsi, rdx
    sub     rsi, rdi                     ; inner len
    cmp     rsi, 19
    jb      .next_line
    call    iso8601_to_epoch
    add     rsp, JFT_BUF_BYTES
    xor     ecx, ecx
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret

.next_line:
    ; advance rbx to start of next line (one past the '\n' if present)
    mov     rbx, r13
    cmp     rbx, r14
    jae     .err_buf
    inc     rbx                          ; step over '\n'
    dec     r15d
    jmp     .line_loop

.err_buf:
    add     rsp, JFT_BUF_BYTES
.err_open:
    mov     rcx, -1
    xor     eax, eax
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret

