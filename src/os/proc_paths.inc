; proc_paths.inc — /proc/{pid}/* helpers and small path-builder utilities.

segment readable executable

; pid_cwd(rdi=pid, rsi=outbuf, rdx=outcap) -> rax = len, rcx = 0 or -errno.
; Builds "/proc/{pid}/cwd" and readlinks it into outbuf. NO trailing NUL —
; readlink doesn't write one. Caller can use rax as the length.
pid_cwd:
    push    rbx
    push    r12
    push    r13
    mov     rbx, rsi                 ; outbuf
    mov     r12, rdx                 ; outcap

    ; build path in scratch: "/proc/" + decimal(pid) + "/cwd" + 0
    lea     r13, [_pp_scratch]
    mov     dword [r13], '/pro'
    mov     word [r13 + 4], 'c/'
    add     r13, 6
    mov     rsi, rdi                 ; pid
    mov     rdi, r13
    call    format_u64               ; rax = digits
    add     r13, rax
    mov     dword [r13], '/cwd'
    mov     byte [r13 + 4], 0

    ; readlink(scratch, outbuf, outcap)
    lea     rdi, [_pp_scratch]
    mov     rsi, rbx
    mov     rdx, r12
    call    sys_readlink
    test    rax, rax
    js      .err
    xor     ecx, ecx
    pop     r13
    pop     r12
    pop     rbx
    ret
.err:
    mov     rcx, rax
    xor     eax, eax
    pop     r13
    pop     r12
    pop     rbx
    ret

; pid_start_epoch(rdi=pid) -> rax = wall-clock epoch seconds, rcx = 0/-errno.
;
; Combines two /proc fields:
;   - /proc/{pid}/stat field 22 (starttime in jiffies-since-boot)
;   - /proc/stat "btime" line (boot wall-clock epoch)
;
; Result = btime + starttime / HZ. HZ is hardcoded to 100 (CONFIG_HZ_100,
; the Linux default for >= 99% of distros). Returning a value that is at
; most a second off is fine for our purposes (matching JSONL first-line
; timestamps within a 5 s window).
;
; Implementation note: comm field (process name) can contain spaces and
; parens, so we scan for the LAST ')' before counting fields.
PID_STAT_HZ          = 100
PID_STAT_BUF_BYTES   = 1024
PROC_STAT_BUF_BYTES  = 16384

pid_start_epoch:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    sub     rsp, 256                     ; path scratch
    mov     rbx, rdi                     ; pid

    ; build "/proc/{pid}/stat"
    mov     rdi, rsp
    mov     dword [rdi], '/pro'
    mov     word [rdi + 4], 'c/'
    add     rdi, 6
    mov     rsi, rbx
    call    format_u64                   ; digits
    add     rdi, rax
    mov     dword [rdi], '/sta'
    mov     byte [rdi + 4], 't'
    mov     byte [rdi + 5], 0

    ; open + read up to 1 KB
    mov     rdi, rsp
    mov     rsi, O_RDONLY
    xor     edx, edx
    mov     eax, SYS_open
    syscall
    test    rax, rax
    js      .err
    mov     r12, rax                     ; fd

    sub     rsp, PID_STAT_BUF_BYTES
    mov     rdi, r12
    mov     rsi, rsp
    mov     edx, PID_STAT_BUF_BYTES
    mov     eax, SYS_read
    syscall
    mov     r13, rax                     ; bytes_read
    mov     r14, rsp                     ; buf
    mov     rdi, r12
    push    rax
    mov     eax, SYS_close
    syscall
    pop     rax
    test    r13, r13
    js      .err_buf
    jz      .err_buf

    ; find last ')' in [r14 .. r14+r13)
    lea     rsi, [r14 + r13]             ; end
    mov     rax, rsi
    dec     rax
.scan_paren:
    cmp     rax, r14
    jb      .err_buf
    cmp     byte [rax], ')'
    je      .got_paren
    dec     rax
    jmp     .scan_paren
.got_paren:
    inc     rax                          ; cursor past ')'
    mov     r15, rax                     ; cursor

    ; skip 19 whitespace-separated fields to land on field 22 (starttime).
    ; The state field follows, so field index here = (real_field - 2).
    ; For starttime (field 22), we need 22 - 2 = 20 fields skipped... wait:
    ; immediately after ')' we are positioned BEFORE field 3 (state).
    ; So we need to consume 19 fields and then read the 20th, which is
    ; starttime.
    mov     ecx, 19
.skip_fields:
    ; skip whitespace
.sk_ws:
    cmp     r15, rsi
    jae     .err_buf
    cmp     byte [r15], ' '
    jne     .sk_field_start
    inc     r15
    jmp     .sk_ws
.sk_field_start:
    ; consume non-whitespace
.sk_nw:
    cmp     r15, rsi
    jae     .err_buf
    cmp     byte [r15], ' '
    je      .sk_done_field
    inc     r15
    jmp     .sk_nw
.sk_done_field:
    dec     ecx
    jnz     .skip_fields

    ; skip leading whitespace before starttime
.sk_ws2:
    cmp     r15, rsi
    jae     .err_buf
    cmp     byte [r15], ' '
    jne     .read_starttime
    inc     r15
    jmp     .sk_ws2
.read_starttime:
    mov     rdi, r15
    mov     rdx, rsi
    sub     rdx, rdi                     ; remaining
    mov     rsi, rdx
    call    parse_u64                    ; rax = starttime jiffies
    test    rdx, rdx
    jz      .err_buf
    mov     r13, rax                     ; starttime_jiffies
    add     rsp, PID_STAT_BUF_BYTES      ; release stat buf

    ; read /proc/stat for btime
    sub     rsp, PROC_STAT_BUF_BYTES
    mov     rdi, _pp_proc_stat_path
    mov     rsi, O_RDONLY
    xor     edx, edx
    mov     eax, SYS_open
    syscall
    test    rax, rax
    js      .err
    mov     r12, rax
    mov     rdi, r12
    mov     rsi, rsp
    mov     edx, PROC_STAT_BUF_BYTES
    mov     eax, SYS_read
    syscall
    mov     r14, rax                     ; bytes
    mov     rdi, r12
    push    rax
    mov     eax, SYS_close
    syscall
    pop     rax
    test    r14, r14
    js      .err_buf2
    jz      .err_buf2

    ; scan for newline-anchored "btime " token
    mov     r15, rsp                     ; cursor
    lea     rsi, [rsp + r14]             ; end
    ; allow first line too
    mov     rdx, r15
    jmp     .scan_btime_line
.scan_btime_next:
    inc     r15
    jmp     .scan_btime
.scan_btime:
    cmp     r15, rsi
    jae     .err_buf2
    cmp     byte [r15], 0x0A
    jne     .scan_btime_next
    inc     r15
.scan_btime_line:
    ; r15 points at line start; check prefix "btime "
    mov     rax, rsi
    sub     rax, r15
    cmp     rax, 6
    jb      .scan_btime
    cmp     dword [r15], 'btim'
    jne     .scan_btime
    cmp     byte [r15 + 4], 'e'
    jne     .scan_btime
    cmp     byte [r15 + 5], ' '
    jne     .scan_btime
    add     r15, 6
    mov     rdi, r15
    mov     rdx, rsi
    sub     rdx, rdi
    mov     rsi, rdx
    call    parse_u64                    ; rax = btime
    test    rdx, rdx
    jz      .err_buf2
    add     rsp, PROC_STAT_BUF_BYTES

    ; result = btime + starttime / HZ
    mov     rcx, r13
    mov     rdx, 0
    mov     rdi, PID_STAT_HZ
    mov     r8, rax                      ; btime
    mov     rax, rcx
    div     rdi                          ; rax = jiffies/HZ
    add     rax, r8
    xor     ecx, ecx
    add     rsp, 256
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

.err_buf2:
    add     rsp, PROC_STAT_BUF_BYTES
    jmp     .err
.err_buf:
    add     rsp, PID_STAT_BUF_BYTES
.err:
    add     rsp, 256
    mov     rcx, -1
    xor     eax, eax
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

segment readable
_pp_proc_stat_path db '/proc/stat', 0

segment readable writeable
align 8
_pp_scratch rb 64
