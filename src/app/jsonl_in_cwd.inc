; app/jsonl_in_cwd.inc — fallback: latest *.jsonl in encoded-cwd dir.
;
; Claude encodes cwd by replacing '/' and '.' with '-'.

STAT_MTIME_OFFSET = 88       ; struct stat.st_mtime.tv_sec (s64)
STAT_MTIME_NS_OFFSET = 96    ; struct stat.st_mtime.tv_nsec (s64)


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

; find_recent_jsonl_in_cwd(rdi=cwd, rsi=cwd_len, rdx=outbuf, rcx=cap,
;                          r8=claimed_buf, r9=claimed_count)
;   -> rax = path_len, r8 = 0/-1
;
; claimed_buf is a flat array of CLAIM_SLOT-byte slots; each slot starts with a
; UUID stem (the JSONL filename minus ".jsonl"), NUL-padded. claimed_count
; is the number of populated slots. Pass r8=NULL/r9=0 to skip exclusion.
;
; Behavior: scan all *.jsonl in $HOME/.claude/projects/<encoded_cwd>/, exclude
; entries whose stem matches any claimed slot, return the path of the most
; recently modified remaining one.
CLAIM_SLOT = 40

find_recent_jsonl_in_cwd:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    ; stash claimed args before we clobber r8/r9 in the dent loop
    mov     qword [_iw_claimed_buf], r8
    mov     qword [_iw_claimed_n], r9

    mov     rbx, rdi                 ; cwd
    mov     rbp, rsi                 ; cwd_len
    mov     r12, rdx                 ; outbuf
    mov     r13, rcx                 ; cap

    ; Build dir_path into _iw_dir_buf, save its length in _iw_dir_len.
    mov     rdi, qword [_iw_dir_buf_ptr]
    mov     esi, 1024
    call    home_dir
    test    r8, r8
    js      .err
    mov     r14, rax                 ; len

    mov     rdi, qword [_iw_dir_buf_ptr]
    add     rdi, r14
    lea     rsi, [_seg_proj2]
    mov     rdx, _seg_proj2_len
    call    memcpy
    add     r14, _seg_proj2_len

    ; encode cwd into [_iw_dir_buf + r14 ..]
    mov     rdi, rbx
    mov     rsi, rbp
    mov     rdx, qword [_iw_dir_buf_ptr]
    add     rdx, r14
    mov     rcx, 1024
    sub     rcx, r14
    sub     rcx, 64                  ; reserve space
    call    encode_cwd
    add     r14, rax
    mov     r9, qword [_iw_dir_buf_ptr]
    mov     byte [r9 + r14], '/'
    inc     r14
    mov     byte [r9 + r14], 0
    mov     qword [_iw_dir_len], r14

    ; open dir
    mov     rdi, qword [_iw_dir_buf_ptr]
    xor     esi, esi
    xor     edx, edx
    call    sys_open
    test    rax, rax
    js      .err
    mov     r15, rax                 ; dir fd

    mov     qword [_iw_best_mtime], 0
    mov     qword [_iw_best_mtime_ns], 0
    mov     qword [_iw_best_name_len], 0

.refill:
    mov     rdi, r15
    arena_lea rsi, ARENA_OFF_IW_DENT_BUF
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
    arena_lea r8, ARENA_OFF_IW_DENT_BUF
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

    ; --- check the stem against the claimed list, skip on match ---
    ; stem = [rdi, rdi + (rsi - 6))
    push    rdi
    push    rsi
    mov     r10, rsi
    sub     r10, _seg_jsonl2_len     ; stem length
    mov     r11, qword [_iw_claimed_n]
    test    r11, r11
    jz      .not_claimed
    mov     rcx, qword [_iw_claimed_buf]
    xor     edx, edx                 ; index
.claim_loop:
    cmp     rdx, r11
    jae     .not_claimed
    ; compare claimed[i] (CLAIM_SLOT bytes) with stem (r10 bytes)
    push    rdi
    push    rsi
    push    rdx
    push    rcx
    push    r10
    push    r11
    mov     rsi, rcx
    mov     rdx, r10
    call    memcmp
    pop     r11
    pop     r10
    pop     rcx
    pop     rdx
    pop     rsi
    pop     rdi
    test    rax, rax
    jnz     .claim_next
    ; matched — but only if claimed slot's NEXT byte (stem_len) is NUL,
    ; i.e. the stems are equal length. Otherwise it's a prefix match.
    cmp     byte [rcx + r10], 0
    jne     .claim_next
    ; matched fully — skip this dent
    pop     rsi
    pop     rdi
    jmp     .next_dent
.claim_next:
    add     rcx, CLAIM_SLOT
    inc     rdx
    jmp     .claim_loop
.not_claimed:
    pop     rsi
    pop     rdi

    ; build full_path = dir_path + name into _iw_full_buf
    push    rdi                      ; name ptr
    push    rsi                      ; name_len
    mov     rdi, qword [_iw_full_buf_ptr]
    mov     rsi, qword [_iw_dir_buf_ptr]
    mov     rdx, qword [_iw_dir_len]
    call    memcpy
    pop     rsi
    pop     rdi
    push    rsi                      ; saved name_len for later
    mov     rdx, qword [_iw_dir_len]
    push    rdi                      ; saved name_ptr
    mov     rdi, qword [_iw_full_buf_ptr]
    add     rdi, rdx
    mov     rsi, [rsp]               ; name_ptr
    mov     rdx, [rsp + 8]           ; name_len
    call    memcpy
    pop     rdi                      ; name_ptr
    pop     rsi                      ; name_len

    ; NUL-terminate full_path at end
    mov     rax, qword [_iw_dir_len]
    add     rax, rsi
    mov     r9, qword [_iw_full_buf_ptr]
    mov     byte [r9 + rax], 0

    ; stat full_path -> _iw_statbuf
    push    rdi
    push    rsi
    mov     rdi, qword [_iw_full_buf_ptr]
    arena_lea rsi, ARENA_OFF_IW_STATBUF
    call    sys_stat
    pop     rsi
    pop     rdi
    test    rax, rax
    js      .next_dent

    ; (sec_new, nsec_new) > (sec_best, nsec_best) ?  — full nanosecond compare
    arena_lea r10, ARENA_OFF_IW_STATBUF
    mov     rax, qword [r10 + STAT_MTIME_OFFSET]
    cmp     rax, qword [_iw_best_mtime]
    ja      .new_best
    jb      .next_dent
    mov     rax, qword [r10 + STAT_MTIME_NS_OFFSET]
    cmp     rax, qword [_iw_best_mtime_ns]
    jbe     .next_dent
.new_best:
    arena_lea r10, ARENA_OFF_IW_STATBUF
    mov     rax, qword [r10 + STAT_MTIME_OFFSET]
    mov     qword [_iw_best_mtime], rax
    mov     rax, qword [r10 + STAT_MTIME_NS_OFFSET]
    mov     qword [_iw_best_mtime_ns], rax
    mov     qword [_iw_best_name_len], rsi
    mov     rdx, rsi
    push    rsi
    mov     rsi, rdi
    arena_lea rdi, ARENA_OFF_IW_BEST_NAME
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
    mov     rsi, qword [_iw_dir_buf_ptr]
    mov     rdx, qword [_iw_dir_len]
    call    memcpy
    mov     r14, rdx
    lea     rdi, [r12 + r14]
    arena_lea rsi, ARENA_OFF_IW_BEST_NAME
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

; find_jsonl_by_pid_start(rdi=cwd, rsi=cwd_len, rdx=outbuf, rcx=cap,
;                         r8=pid_start_epoch) -> rax = path_len, r8 = 0/-1
;
; Pid-anchored fallback. Lists $HOME/.claude/projects/<encoded_cwd>/*.jsonl,
; reads each file's first-line "timestamp" via jsonl_first_ts_epoch, and
; returns the path whose first-ts has the smallest |delta| from pid_start_epoch
; with |delta| <= JSONL_PID_TOLERANCE (300 s).
;
; Doesn't take a claimed list — pid-anchoring is meant to disambiguate even
; when several pids share a cwd; their start times differ enough that the
; closest-match rule rarely collides. If two pids match the same JSONL
; within tolerance, the caller's pcache pinning still keeps each pane stable
; once the assignment is recorded.
JSONL_PID_TOLERANCE = 300

find_jsonl_by_pid_start:
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
    mov     qword [_iw_pid_start], r8

    ; Build dir_path into _iw_dir_buf (same recipe as find_recent_jsonl_in_cwd)
    mov     rdi, qword [_iw_dir_buf_ptr]
    mov     esi, 1024
    call    home_dir
    test    r8, r8
    js      .pa_err
    mov     r14, rax

    mov     rdi, qword [_iw_dir_buf_ptr]
    add     rdi, r14
    lea     rsi, [_seg_proj2]
    mov     rdx, _seg_proj2_len
    call    memcpy
    add     r14, _seg_proj2_len

    mov     rdi, rbx
    mov     rsi, rbp
    mov     rdx, qword [_iw_dir_buf_ptr]
    add     rdx, r14
    mov     rcx, 1024
    sub     rcx, r14
    sub     rcx, 64
    call    encode_cwd
    add     r14, rax
    mov     r9, qword [_iw_dir_buf_ptr]
    mov     byte [r9 + r14], '/'
    inc     r14
    mov     byte [r9 + r14], 0
    mov     qword [_iw_dir_len], r14

    mov     rdi, qword [_iw_dir_buf_ptr]
    xor     esi, esi
    xor     edx, edx
    call    sys_open
    test    rax, rax
    js      .pa_err
    mov     r15, rax                 ; dir fd

    mov     qword [_iw_pa_best_delta], JSONL_PID_TOLERANCE + 1
    mov     qword [_iw_pa_best_name_len], 0

.pa_refill:
    mov     rdi, r15
    arena_lea rsi, ARENA_OFF_IW_DENT_BUF
    mov     edx, 8192
    call    sys_getdents64
    test    rax, rax
    jle     .pa_scan_done
    mov     qword [_iw_dent_size], rax
    mov     qword [_iw_dent_cur], 0

.pa_next_dent:
    mov     rax, qword [_iw_dent_cur]
    cmp     rax, qword [_iw_dent_size]
    jae     .pa_refill
    arena_lea r8, ARENA_OFF_IW_DENT_BUF
    add     r8, rax
    movzx   ecx, word [r8 + DENT_RECLEN_OFF]
    add     rax, rcx
    mov     qword [_iw_dent_cur], rax

    lea     rdi, [r8 + DENT_NAME_OFF]
    cmp     byte [rdi], '.'
    je      .pa_next_dent

    mov     rcx, rdi
.pa_namelen:
    cmp     byte [rcx], 0
    je      .pa_got_name_len
    inc     rcx
    jmp     .pa_namelen
.pa_got_name_len:
    mov     rsi, rcx
    sub     rsi, rdi                 ; name_len
    cmp     rsi, _seg_jsonl2_len
    jbe     .pa_next_dent

    ; tail == ".jsonl"?
    push    rdi
    push    rsi
    lea     rcx, [rdi + rsi]
    sub     rcx, _seg_jsonl2_len
    mov     rdi, rcx
    lea     rsi, [_seg_jsonl2]
    mov     rdx, _seg_jsonl2_len
    call    memcmp
    pop     rsi
    pop     rdi
    test    rax, rax
    jnz     .pa_next_dent

    ; build full path = dir_path + name into _iw_full_buf
    ; stash name_ptr / name_len in static slots (no good free callee-saved
    ; left in this loop — rbx..r15 are all in use).
    mov     qword [_iw_pa_name_ptr], rdi
    mov     qword [_iw_pa_name_len], rsi
    mov     rdi, qword [_iw_full_buf_ptr]
    mov     rsi, qword [_iw_dir_buf_ptr]
    mov     rdx, qword [_iw_dir_len]
    call    memcpy
    mov     rdx, qword [_iw_dir_len]
    mov     rdi, qword [_iw_full_buf_ptr]
    add     rdi, rdx
    mov     rsi, qword [_iw_pa_name_ptr]
    mov     rdx, qword [_iw_pa_name_len]
    call    memcpy

    mov     rax, qword [_iw_dir_len]
    add     rax, qword [_iw_pa_name_len]
    mov     r9, qword [_iw_full_buf_ptr]
    mov     byte [r9 + rax], 0

    ; jsonl_first_ts_epoch(full_path)
    mov     rdi, qword [_iw_full_buf_ptr]
    call    jsonl_first_ts_epoch
    mov     r9, rax                  ; ts (only valid if rcx==0)
    test    rcx, rcx
    js      .pa_next_dent

    ; delta = abs(ts - pid_start)
    mov     rax, qword [_iw_pid_start]
    sub     r9, rax
    jns     .pa_pos
    neg     r9
.pa_pos:
    cmp     r9, JSONL_PID_TOLERANCE
    ja      .pa_next_dent
    cmp     r9, qword [_iw_pa_best_delta]
    jae     .pa_next_dent
    ; new best
    mov     qword [_iw_pa_best_delta], r9
    mov     rax, qword [_iw_pa_name_len]
    mov     qword [_iw_pa_best_name_len], rax
    mov     rsi, qword [_iw_pa_name_ptr]
    mov     rdx, rax
    arena_lea rdi, ARENA_OFF_IW_PA_BEST
    call    memcpy
    jmp     .pa_next_dent

.pa_scan_done:
    mov     rdi, r15
    call    sys_close
    cmp     qword [_iw_pa_best_name_len], 0
    je      .pa_err

    mov     rdi, r12
    mov     rsi, qword [_iw_dir_buf_ptr]
    mov     rdx, qword [_iw_dir_len]
    call    memcpy
    mov     r14, rdx
    lea     rdi, [r12 + r14]
    arena_lea rsi, ARENA_OFF_IW_PA_BEST
    mov     rdx, qword [_iw_pa_best_name_len]
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

.pa_err:
    mov     r8, -1
    xor     eax, eax
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

