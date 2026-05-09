; app/timefmt.inc — ISO 8601 → epoch + relative time formatting.
;
; Inputs we accept: "YYYY-MM-DDTHH:MM:SS[.fff]Z" — at least 19 bytes; extra
; bytes (sub-seconds, timezone) are ignored.

segment readable

; cumulative days at start of each month (non-leap, leap)
align 4
_month_off_normal:
    dd  0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
_month_off_leap:
    dd  0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335

segment readable executable

; _read_uint(rdi=ptr, rsi=ndigits) -> rax = value
; reads exactly ndigits decimal digits, no validation.
_read_uint:
    xor     eax, eax
    xor     ecx, ecx
.loop:
    cmp     rcx, rsi
    jae     .done
    movzx   edx, byte [rdi + rcx]
    sub     edx, '0'
    lea     rax, [rax + rax*4]
    add     rax, rax
    add     rax, rdx
    inc     rcx
    jmp     .loop
.done:
    ret

; _is_leap(rdi=year) -> rax = 1 if leap else 0
_is_leap:
    mov     rax, rdi
    test    rax, 3
    jnz     .no
    mov     rcx, rax
    mov     r8, 100
    xor     rdx, rdx
    div     r8                       ; rax / 100 -> rax, rdx = remainder
    test    rdx, rdx
    jnz     .yes                     ; not divisible by 100 -> leap
    ; divisible by 100 — must be divisible by 400
    mov     rax, rcx
    mov     r8, 400
    xor     rdx, rdx
    div     r8
    test    rdx, rdx
    jnz     .no
.yes:
    mov     eax, 1
    ret
.no:
    xor     eax, eax
    ret

; iso8601_to_epoch(rdi=str, rsi=len) -> rax = epoch seconds.
; Format expected: YYYY-MM-DDTHH:MM:SS... at the start. No validation.
iso8601_to_epoch:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    mov     r12, rdi                 ; str

    ; Y  ←  [0..4)
    mov     rsi, 4
    call    _read_uint
    mov     r13, rax                 ; year
    ; M  ←  [5..7)
    lea     rdi, [r12 + 5]
    mov     rsi, 2
    call    _read_uint
    mov     r14, rax                 ; month
    ; D  ←  [8..10)
    lea     rdi, [r12 + 8]
    mov     rsi, 2
    call    _read_uint
    mov     r15, rax                 ; day
    ; H  ←  [11..13)
    lea     rdi, [r12 + 11]
    mov     rsi, 2
    call    _read_uint
    push    rax                      ; hour
    ; Mi ←  [14..16)
    lea     rdi, [r12 + 14]
    mov     rsi, 2
    call    _read_uint
    push    rax                      ; minute
    ; S  ←  [17..19)
    lea     rdi, [r12 + 17]
    mov     rsi, 2
    call    _read_uint
    mov     rbx, rax                 ; second
    pop     rbp                      ; minute
    pop     r11                      ; hour

    ; days = (year - 1970)*365
    lea     rax, [r13 - 1970]
    mov     rcx, 365
    mul     rcx                      ; rdx:rax = days_so_far (fits in rax)
    mov     r10, rax                 ; days

    ; leap_days_added = floor((year-1)/4) - floor((year-1)/100) + floor((year-1)/400)
    ;                  - (1969/4 - 1969/100 + 1969/400)
    ;                  = floor((year-1)/4) - floor((year-1)/100) + floor((year-1)/400) - (492 - 19 + 4)
    ;                  = floor((year-1)/4) - floor((year-1)/100) + floor((year-1)/400) - 477
    lea     rax, [r13 - 1]
    mov     rcx, 4
    xor     rdx, rdx
    div     rcx
    mov     r9, rax                  ; (year-1)/4
    lea     rax, [r13 - 1]
    mov     rcx, 100
    xor     rdx, rdx
    div     rcx
    sub     r9, rax
    lea     rax, [r13 - 1]
    mov     rcx, 400
    xor     rdx, rdx
    div     rcx
    add     r9, rax
    sub     r9, 477
    add     r10, r9                  ; days += extra_leap

    ; month offset (uses leap table if current year is leap)
    mov     rdi, r13
    push    rbx
    push    rbp
    push    r11
    push    r10
    push    r14
    push    r15
    call    _is_leap
    pop     r15
    pop     r14
    pop     r10
    pop     r11
    pop     rbp
    pop     rbx
    test    rax, rax
    jz      .normal
    lea     rcx, [_month_off_leap]
    jmp     .got_off
.normal:
    lea     rcx, [_month_off_normal]
.got_off:
    dec     r14                      ; 0-indexed
    movsxd  rax, dword [rcx + r14*4]
    add     r10, rax

    ; + (day - 1)
    dec     r15
    add     r10, r15

    ; epoch = days * 86400 + H*3600 + M*60 + S
    mov     rax, r10
    mov     rcx, 86400
    mul     rcx
    mov     rcx, 3600
    push    rax
    mov     rax, r11
    mul     rcx
    pop     rcx
    add     rax, rcx                 ; H*3600 + days*86400
    imul    rbp, 60
    add     rax, rbp
    add     rax, rbx

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

; format_relative(rdi=outbuf, rsi=cap, rdx=now, rcx=then) -> rax = len
format_relative:
    push    rbx
    push    rbp
    mov     rbx, rdi                 ; outbuf
    mov     rbp, rsi                 ; cap
    sub     rdx, rcx                 ; diff = now - then
    js      .lt1m                    ; future -> still "< 1m"
    cmp     rdx, 60
    jb      .lt1m
    cmp     rdx, 3600
    jb      .minutes
    cmp     rdx, 86400
    jb      .hours
    ; days
    mov     rax, rdx
    mov     rcx, 86400
    xor     rdx, rdx
    div     rcx
    mov     rsi, rax
    mov     rdi, rbx
    call    format_u64
    mov     rcx, rax
    mov     byte [rbx + rcx], 'd'
    mov     byte [rbx + rcx + 1], ' '
    mov     byte [rbx + rcx + 2], 'a'
    mov     byte [rbx + rcx + 3], 'g'
    mov     byte [rbx + rcx + 4], 'o'
    add     rax, 5
    pop     rbp
    pop     rbx
    ret

.minutes:
    mov     rax, rdx
    mov     rcx, 60
    xor     rdx, rdx
    div     rcx
    mov     rsi, rax
    mov     rdi, rbx
    call    format_u64
    mov     rcx, rax
    mov     byte [rbx + rcx], 'm'
    mov     byte [rbx + rcx + 1], ' '
    mov     byte [rbx + rcx + 2], 'a'
    mov     byte [rbx + rcx + 3], 'g'
    mov     byte [rbx + rcx + 4], 'o'
    add     rax, 5
    pop     rbp
    pop     rbx
    ret

.hours:
    mov     rax, rdx
    mov     rcx, 3600
    xor     rdx, rdx
    div     rcx
    mov     rsi, rax
    mov     rdi, rbx
    call    format_u64
    mov     rcx, rax
    mov     byte [rbx + rcx], 'h'
    mov     byte [rbx + rcx + 1], ' '
    mov     byte [rbx + rcx + 2], 'a'
    mov     byte [rbx + rcx + 3], 'g'
    mov     byte [rbx + rcx + 4], 'o'
    add     rax, 5
    pop     rbp
    pop     rbx
    ret

.lt1m:
    mov     byte [rbx + 0], '<'
    mov     byte [rbx + 1], ' '
    mov     byte [rbx + 2], '1'
    mov     byte [rbx + 3], 'm'
    mov     eax, 4
    pop     rbp
    pop     rbx
    ret

; now_epoch_secs() -> rax = current epoch seconds (CLOCK_REALTIME tv_sec)
now_epoch_secs:
    sub     rsp, 16
    xor     edi, edi                 ; CLOCK_REALTIME = 0
    mov     rsi, rsp
    call    sys_clock_gettime
    mov     rax, [rsp]
    add     rsp, 16
    ret
