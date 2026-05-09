; app/jsonl_locate.inc — find a session JSONL by sessionId.
;
; ~/.claude/projects/<encoded-cwd>/<session_id>.jsonl
; We don't know <encoded-cwd> for a given session, so we walk all immediate
; subdirectories of ~/.claude/projects/ and probe each for the file.

DT_DIR = 4
DENT_RECLEN_OFF = 16
DENT_TYPE_OFF   = 18
DENT_NAME_OFF   = 19
DENTBUF_BYTES   = 8192

; We pin frequently-needed pointers in callee-saved registers and use a
; dedicated 1KB stack scratch for the base path.
;
;   rbx = sid_ptr            r12 = outbuf
;   rbp = sid_len            r13 = outbuf cap
;   r14 = base_len           r15 = dir_fd
;   [rsp]  ..  [rsp+1023]    base path (NUL-terminated within base_len bytes)

PATH_FRAME = 1024

segment readable executable

find_jsonl_path:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    mov     rbx, rdi                 ; sid
    mov     rbp, rsi                 ; sid_len
    mov     r12, rdx                 ; outbuf
    mov     r13, rcx                 ; cap

    sub     rsp, PATH_FRAME

    ; --- build base path: HOME + "/.claude/projects/" ---
    mov     rdi, rsp
    mov     esi, PATH_FRAME
    call    home_dir
    test    r8, r8
    js      .err
    mov     r14, rax                 ; base_len = HOME length

    lea     rdi, [rsp + r14]
    lea     rsi, [_seg_proj]
    mov     edx, _seg_proj_len
    call    memcpy
    add     r14, _seg_proj_len
    mov     byte [rsp + r14], 0

    ; --- open the directory ---
    mov     rdi, rsp
    xor     esi, esi
    xor     edx, edx
    call    sys_open
    test    rax, rax
    js      .err
    mov     r15, rax                 ; dir fd

.fill_buf:
    mov     rdi, r15
    lea     rsi, [_dent_buf]
    mov     edx, DENTBUF_BYTES
    call    sys_getdents64
    test    rax, rax
    jle     .err_close               ; 0 = end, <0 = error
    mov     qword [_dent_size], rax
    mov     qword [_dent_cur], 0

.next_entry:
    mov     rcx, qword [_dent_cur]
    cmp     rcx, qword [_dent_size]
    jae     .fill_buf
    lea     r9, [_dent_buf]
    add     r9, rcx
    movzx   eax, word [r9 + DENT_RECLEN_OFF]
    add     rcx, rax
    mov     qword [_dent_cur], rcx

    ; skip "." and ".."
    lea     rcx, [r9 + DENT_NAME_OFF]
    cmp     byte [rcx], '.'
    jne     .try_candidate
    cmp     byte [rcx + 1], 0
    je      .next_entry
    cmp     byte [rcx + 1], '.'
    jne     .try_candidate
    cmp     byte [rcx + 2], 0
    je      .next_entry

.try_candidate:
    ; Build candidate path: base + entry_name + "/" + sid + ".jsonl"
    ; Stash entry's d_name pointer in a static — we need it across calls.
    lea     rax, [r9 + DENT_NAME_OFF]
    mov     qword [_cand_name], rax

    ; copy base into outbuf
    mov     rdi, r12
    mov     rsi, rsp                 ; base path
    mov     rdx, r14
    call    memcpy
    mov     qword [_cand_pos], r14

    ; copy d_name (NUL terminated)
    mov     rsi, qword [_cand_name]
    mov     rdi, r12
    add     rdi, qword [_cand_pos]
.copy_name:
    mov     al, byte [rsi]
    test    al, al
    jz      .name_copied
    mov     [rdi], al
    inc     rsi
    inc     rdi
    inc     qword [_cand_pos]
    jmp     .copy_name
.name_copied:

    ; "/"
    mov     rax, qword [_cand_pos]
    mov     byte [r12 + rax], '/'
    inc     qword [_cand_pos]

    ; sid
    mov     rdi, r12
    add     rdi, qword [_cand_pos]
    mov     rsi, rbx
    mov     rdx, rbp
    call    memcpy
    mov     rax, qword [_cand_pos]
    add     rax, rbp
    mov     qword [_cand_pos], rax

    ; ".jsonl"
    mov     rdi, r12
    add     rdi, qword [_cand_pos]
    lea     rsi, [_seg_jsonl]
    mov     edx, _seg_jsonl_len
    call    memcpy
    mov     rax, qword [_cand_pos]
    add     rax, _seg_jsonl_len
    mov     qword [_cand_pos], rax

    ; NUL terminator (just past the end)
    mov     rax, qword [_cand_pos]
    mov     byte [r12 + rax], 0

    ; probe via open
    mov     rdi, r12
    xor     esi, esi
    xor     edx, edx
    call    sys_open
    test    rax, rax
    js      .next_entry
    ; success — close it, then close dir, return
    mov     rdi, rax
    call    sys_close
    mov     rdi, r15
    call    sys_close
    add     rsp, PATH_FRAME
    mov     rax, qword [_cand_pos]
    xor     r8, r8
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

.err_close:
    mov     rdi, r15
    call    sys_close
.err:
    add     rsp, PATH_FRAME
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
_seg_proj    db '/.claude/projects/'
_seg_proj_len = $ - _seg_proj
_seg_jsonl   db '.jsonl'
_seg_jsonl_len = $ - _seg_jsonl

segment readable writeable
align 8
_dent_buf    rb DENTBUF_BYTES
_dent_size   rq 1
_dent_cur    rq 1
_cand_name   rq 1
_cand_pos    rq 1
