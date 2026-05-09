; os/env.inc — environment variable lookup via /proc/self/environ.
;
; The contents of /proc/self/environ are NUL-separated "name=value" entries.
; We slurp the whole thing once into a static buffer, then scan on each call.

_env_buf_cap = 8192

segment readable executable

; _env_load() -> rax = 0 ok / -errno.
; Idempotent: only reads /proc/self/environ on the first invocation.
_env_load:
    cmp     dword [_env_loaded], 0
    je      .do_load
    xor     eax, eax
    ret
.do_load:
    push    r12
    push    r13
    lea     rdi, [_path_environ]
    xor     esi, esi                 ; O_RDONLY
    xor     edx, edx
    call    sys_open
    test    rax, rax
    js      .out
    mov     r12, rax                 ; fd
    xor     r13d, r13d               ; bytes read so far
.read_loop:
    mov     rax, _env_buf_cap
    sub     rax, r13
    test    rax, rax
    jz      .done
    mov     rdi, r12
    lea     rsi, [_env_buf]
    add     rsi, r13
    mov     rdx, rax
    call    sys_read
    test    rax, rax
    jle     .done
    add     r13, rax
    jmp     .read_loop
.done:
    mov     qword [_env_len], r13
    mov     dword [_env_loaded], 1
    mov     rdi, r12
    call    sys_close
    xor     eax, eax
.out:
    pop     r13
    pop     r12
    ret

; read_env_var(rdi=name, rsi=name_len, rdx=outbuf, rcx=cap)
;   -> rax = value_len, r8 = 0 ok / -1 not found.
read_env_var:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    mov     rbx, rdi                 ; name
    mov     rbp, rsi                 ; name_len
    mov     r12, rdx                 ; outbuf
    mov     r13, rcx                 ; cap

    call    _env_load
    test    rax, rax
    js      .err

    lea     r14, [_env_buf]
    mov     r15, [_env_len]
    add     r15, r14                 ; end

.next_entry:
    cmp     r14, r15
    jae     .err
    ; bytes left at this entry must be > name_len for "name="
    mov     rax, r15
    sub     rax, r14
    cmp     rax, rbp
    jbe     .skip
    ; memcmp(r14, name, name_len)
    mov     rdi, r14
    mov     rsi, rbx
    mov     rdx, rbp
    call    memcmp
    test    rax, rax
    jnz     .skip
    cmp     byte [r14 + rbp], '='
    jne     .skip

    ; value: [r14 + rbp + 1, next_nul)
    lea     rdi, [r14 + rbp + 1]     ; value_start
    mov     rsi, rdi
.find_nul:
    cmp     rsi, r15
    jae     .got_value
    cmp     byte [rsi], 0
    je      .got_value
    inc     rsi
    jmp     .find_nul
.got_value:
    mov     rdx, rsi
    sub     rdx, rdi                 ; raw value length
    cmp     rdx, r13
    jbe     .copy
    mov     rdx, r13                 ; truncate
.copy:
    push    rdx
    mov     rsi, rdi                 ; src = value_start
    mov     rdi, r12                 ; dst = outbuf
    call    memcpy
    pop     rax                      ; rax = length
    xor     r8, r8
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

.skip:
    ; advance past next NUL terminator
.skip_loop:
    cmp     r14, r15
    jae     .err
    mov     al, byte [r14]
    inc     r14
    test    al, al
    jnz     .skip_loop
    jmp     .next_entry

.err:
    mov     r8, -1
    xor     eax, eax
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

; home_dir(rdi=outbuf, rsi=cap) -> rax = len, r8 = 0/-1
home_dir:
    mov     rdx, rdi
    mov     rcx, rsi
    lea     rdi, [_str_HOME]
    mov     esi, 4
    call    read_env_var
    ret

segment readable
_path_environ db '/proc/self/environ', 0
_str_HOME     db 'HOME'

segment readable writeable
_env_buf     rb _env_buf_cap
_env_len     rq 1
_env_loaded  rd 1
