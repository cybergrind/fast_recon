; app/jsonl_first_ts.inc — read epoch of the JSONL's first line.
;
; Used to anchor JSONL files to the claude pid that created them: each
; new session writes its first line within milliseconds of the process
; starting, so the first-line timestamp ≈ pid_start_epoch.

JFT_BUF_BYTES = 4096

segment readable executable

; jsonl_first_ts_epoch(rdi=path0) -> rax = epoch_secs, rcx = 0/-errno
; Opens path, reads up to 4 KB, finds first '\n', extracts top-level
; "timestamp" string and converts via iso8601_to_epoch.
jsonl_first_ts_epoch:
    push    rbx
    push    r12
    push    r13

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

    ; first '\n' in [rsp, rsp+r13)
    mov     rax, rsp
    lea     rcx, [rsp + r13]
.scan_nl:
    cmp     rax, rcx
    jae     .have_end
    cmp     byte [rax], 0x0A
    je      .have_end
    inc     rax
    jmp     .scan_nl
.have_end:
    mov     rbx, rax                     ; line end (exclusive)

    ; json_find_key(buf, line_end, "timestamp", 9)
    mov     rdi, rsp
    mov     rsi, rbx
    lea     rdx, [_jft_key_ts]
    mov     ecx, _jft_key_ts_len
    call    json_find_key
    test    rcx, rcx
    js      .err_buf
    cmp     byte [rax], '"'
    jne     .err_buf
    inc     rax
    dec     rdx
    cmp     rax, rdx
    ja      .err_buf
    mov     rdi, rax
    mov     rsi, rdx
    sub     rsi, rdi                     ; len
    cmp     rsi, 19                      ; need YYYY-MM-DDTHH:MM:SS
    jb      .err_buf
    call    iso8601_to_epoch
    add     rsp, JFT_BUF_BYTES
    xor     ecx, ecx
    pop     r13
    pop     r12
    pop     rbx
    ret

.err_buf:
    add     rsp, JFT_BUF_BYTES
.err_open:
    mov     rcx, -1
    xor     eax, eax
    pop     r13
    pop     r12
    pop     rbx
    ret

segment readable
_jft_key_ts     db 'timestamp'
_jft_key_ts_len = $ - _jft_key_ts
