; app/jsonl_in_cwd.inc — fallback: latest *.jsonl in encoded-cwd dir.
;
; Claude encodes cwd by replacing '/' and '.' with '-'.

STAT_MTIME_OFFSET = 88

segment readable executable

; encode_cwd(rdi=src, rsi=src_len, rdx=dst, rcx=cap) -> rax = len
encode_cwd:
    mov     r8, rsi
    cmp     r8, rcx
    jbe     .ok
    mov     r8, rcx
.ok:
    xor     eax, eax
.loop:
    cmp     rax, r8
    jae     .done
    movzx   r9d, byte [rdi + rax]
    cmp     r9b, '/'
    je      .dash
    cmp     r9b, '.'
    je      .dash
    mov     [rdx + rax], r9b
    jmp     .step
.dash:
    mov     byte [rdx + rax], '-'
.step:
    inc     rax
    jmp     .loop
.done:
    ret

; find_recent_jsonl_in_cwd(rdi=cwd, rsi=cwd_len, rdx=outbuf, rcx=cap)
;   -> rax = path_len, r8 = 0/-1
find_recent_jsonl_in_cwd:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    mov     rbx, rdi                 ; cwd
    mov     rbp, rsi                 ; cwd_len
    mov     r12, rdx                 ; outbuf
    mov     r13, rcx                 ; cap

    ; Build dir_path into _iw_dir_buf, save its length in _iw_dir_len.
    lea     rdi, [_iw_dir_buf]
    mov     esi, 1024
    call    home_dir
    test    r8, r8
    js      .err
    mov     r14, rax                 ; len

    lea     rdi, [_iw_dir_buf + r14]
    lea     rsi, [_seg_proj2]
    mov     rdx, _seg_proj2_len
    call    memcpy
    add     r14, _seg_proj2_len

    ; encode cwd into [_iw_dir_buf + r14 ..]
    mov     rdi, rbx
    mov     rsi, rbp
    lea     rdx, [_iw_dir_buf + r14]
    mov     rcx, 1024
    sub     rcx, r14
    sub     rcx, 64                  ; reserve space
    call    encode_cwd
    add     r14, rax
    mov     byte [_iw_dir_buf + r14], '/'
    inc     r14
    mov     byte [_iw_dir_buf + r14], 0
    mov     qword [_iw_dir_len], r14

    ; open dir
    lea     rdi, [_iw_dir_buf]
    xor     esi, esi
    xor     edx, edx
    call    sys_open
    test    rax, rax
    js      .err
    mov     r15, rax                 ; dir fd

    mov     qword [_iw_best_mtime], 0
    mov     qword [_iw_best_name_len], 0

.refill:
    mov     rdi, r15
    lea     rsi, [_iw_dent_buf]
    mov     edx, 8192
    call    sys_getdents64
    test    rax, rax
    jle     .scan_done
    mov     qword [_iw_dent_size], rax
    mov     qword [_iw_dent_cur], 0

.next_dent:
    mov     rax, qword [_iw_dent_cur]
    cmp     rax, qword [_iw_dent_size]
    jae     .refill
    lea     r8, [_iw_dent_buf]
    add     r8, rax
    movzx   ecx, word [r8 + DENT_RECLEN_OFF]
    add     rax, rcx
    mov     qword [_iw_dent_cur], rax

    ; skip "." / ".." / hidden
    lea     rdi, [r8 + DENT_NAME_OFF]
    cmp     byte [rdi], '.'
    je      .next_dent

    ; find name length
    mov     rcx, rdi
.namelen:
    cmp     byte [rcx], 0
    je      .got_name_len
    inc     rcx
    jmp     .namelen
.got_name_len:
    mov     rsi, rcx
    sub     rsi, rdi                 ; name length
    cmp     rsi, _seg_jsonl2_len
    jbe     .next_dent

    ; check tail == ".jsonl"
    push    rdi
    push    rsi
    lea     rcx, [rdi + rsi]
    sub     rcx, _seg_jsonl2_len
    push    rcx
    mov     rdi, rcx
    lea     rsi, [_seg_jsonl2]
    mov     rdx, _seg_jsonl2_len
    call    memcmp
    pop     rcx
    pop     rsi
    pop     rdi
    test    rax, rax
    jnz     .next_dent

    ; build full_path = dir_path + name into _iw_full_buf
    push    rdi                      ; name ptr
    push    rsi                      ; name_len
    mov     rdi, _iw_full_buf
    lea     rsi, [_iw_dir_buf]
    mov     rdx, qword [_iw_dir_len]
    call    memcpy
    pop     rsi
    pop     rdi
    push    rsi                      ; saved name_len for later
    mov     rdx, qword [_iw_dir_len]
    push    rdi                      ; saved name_ptr
    lea     rdi, [_iw_full_buf + rdx]
    mov     rsi, [rsp]               ; name_ptr
    mov     rdx, [rsp + 8]           ; name_len
    call    memcpy
    pop     rdi                      ; name_ptr
    pop     rsi                      ; name_len

    ; NUL-terminate full_path at end
    mov     rax, qword [_iw_dir_len]
    add     rax, rsi
    mov     byte [_iw_full_buf + rax], 0

    ; stat full_path -> _iw_statbuf
    push    rdi
    push    rsi
    lea     rdi, [_iw_full_buf]
    lea     rsi, [_iw_statbuf]
    call    sys_stat
    pop     rsi
    pop     rdi
    test    rax, rax
    js      .next_dent

    ; mtime > best?
    mov     rax, qword [_iw_statbuf + STAT_MTIME_OFFSET]
    cmp     rax, qword [_iw_best_mtime]
    jbe     .next_dent

    ; new best — copy name to _iw_best_name_buf
    mov     qword [_iw_best_mtime], rax
    mov     qword [_iw_best_name_len], rsi
    mov     rdx, rsi
    push    rsi
    mov     rsi, rdi
    lea     rdi, [_iw_best_name_buf]
    call    memcpy
    pop     rsi
    jmp     .next_dent

.scan_done:
    mov     rdi, r15
    call    sys_close

    cmp     qword [_iw_best_name_len], 0
    je      .err

    ; assemble outbuf = dir_path + best_name
    mov     rdi, r12
    lea     rsi, [_iw_dir_buf]
    mov     rdx, qword [_iw_dir_len]
    call    memcpy
    mov     r14, rdx
    lea     rdi, [r12 + r14]
    lea     rsi, [_iw_best_name_buf]
    mov     rdx, qword [_iw_best_name_len]
    call    memcpy
    add     r14, rdx
    mov     byte [r12 + r14], 0

    mov     rax, r14
    xor     r8, r8
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

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

segment readable
_seg_proj2      db '/.claude/projects/'
_seg_proj2_len  = $ - _seg_proj2
_seg_jsonl2     db '.jsonl'
_seg_jsonl2_len = 6

segment readable writeable
align 8
_iw_dir_buf      rb 1024
_iw_dir_len      rq 1
_iw_full_buf     rb 1024
_iw_dent_buf     rb 8192
_iw_dent_size    rq 1
_iw_dent_cur     rq 1
_iw_statbuf      rb 144
_iw_best_mtime   rq 1
_iw_best_name_len rq 1
_iw_best_name_buf rb 256
