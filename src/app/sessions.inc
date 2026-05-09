; app/sessions.inc — read ~/.claude/sessions/{pid}.json and related helpers.


; extract_session_id(rdi=buf, rsi=len, rdx=out) -> rax = uuid_len, rcx = 0/-1
; Locates "sessionId" at the top level of the JSON object in buf and copies
; the inner string (no quotes) into `out`. Returns its length.
extract_session_id:
    push    rbx                      ; out
    push    r12                      ; saved len
    mov     rbx, rdx                 ; out

    add     rsi, rdi                 ; absolute end
    lea     rdx, [_skey_session_id]
    mov     ecx, _skey_session_id_len
    call    json_find_key
    test    rcx, rcx
    js      .miss
    ; rax = value_start (points at '"'), rdx = value_end (one past close ")
    movzx   ecx, byte [rax]
    cmp     cl, '"'
    jne     .miss
    inc     rax
    dec     rdx
    cmp     rax, rdx
    ja      .miss
    mov     r12, rdx
    sub     r12, rax                 ; inner length

    ; memcpy(out, value_inner_ptr, len)
    mov     rdi, rbx
    mov     rsi, rax
    mov     rdx, r12
    call    memcpy

    mov     rax, r12
    xor     ecx, ecx
    pop     r12
    pop     rbx
    ret

.miss:
    mov     rcx, -1
    xor     eax, eax
    pop     r12
    pop     rbx
    ret


; build_session_path(rdi=pid, rsi=outbuf, rdx=cap) -> rax = len, rcx = 0/-1
; Builds "${HOME}/.claude/sessions/{pid}.json", NUL-terminated. Caller's
; outbuf must be >= 256 bytes for safety.
build_session_path:
    push    rbx
    push    rbp
    push    r12
    push    r13
    mov     rbx, rsi                 ; outbuf
    mov     rbp, rdx                 ; cap
    mov     r13, rdi                 ; pid

    ; HOME -> outbuf
    mov     rdi, rbx
    mov     rsi, rbp
    call    home_dir
    test    r8, r8
    js      .err
    mov     r12, rax                 ; cursor

    ; append "/.claude/sessions/"
    lea     rdi, [rbx + r12]
    lea     rsi, [_seg_dot_claude]
    mov     rdx, _seg_dot_claude_len
    call    memcpy
    add     r12, _seg_dot_claude_len

    ; append decimal pid
    lea     rdi, [rbx + r12]
    mov     rsi, r13
    call    format_u64
    add     r12, rax

    ; append ".json"
    lea     rdi, [rbx + r12]
    lea     rsi, [_seg_json_ext]
    mov     rdx, _seg_json_ext_len
    call    memcpy
    add     r12, _seg_json_ext_len

    ; NUL terminate
    mov     byte [rbx + r12], 0

    mov     rax, r12
    xor     ecx, ecx
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret
.err:
    mov     rcx, -1
    xor     eax, eax
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

; pid_session_id(rdi=pid, rsi=out_uuid_buf64) -> rax = len, rcx = 0/-1.
; Resolves ${HOME}/.claude/sessions/{pid}.json, mmaps it, extracts the
; "sessionId" string into out_uuid_buf64. Releases the mmap before returning.
pid_session_id:
    push    rbx
    push    rbp
    push    r12
    push    r13
    mov     rbp, rsi                 ; out_uuid

    sub     rsp, 256                 ; path on stack
    lea     rsi, [rsp]
    mov     edx, 256
    call    build_session_path
    test    rcx, rcx
    js      .out_path_fail

    lea     rdi, [rsp]
    call    read_file_all
    test    rcx, rcx
    js      .out_open_fail
    test    rax, rax
    jz      .out_open_fail
    mov     rbx, rax                 ; ptr
    mov     r12, rdx                 ; len

    ; extract_session_id(buf, len, out)
    mov     rdi, rbx
    mov     rsi, r12
    mov     rdx, rbp
    call    extract_session_id
    mov     r13, rax                 ; uuid len (or undefined on miss)
    push    rcx                      ; status

    ; release the mmap
    mov     rdi, rbx
    mov     rsi, r12
    call    release_file
    pop     rcx
    test    rcx, rcx
    js      .out_open_fail
    mov     rax, r13
    xor     ecx, ecx
    add     rsp, 256
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

.out_path_fail:
.out_open_fail:
    add     rsp, 256
    mov     rcx, -1
    xor     eax, eax
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret
