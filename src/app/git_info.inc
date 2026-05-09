; app/git_info.inc — repo / branch / relative-dir resolution via `git`.
;
; struct git_info (256 bytes):
;   +0    repo_name_len      q
;   +8    relative_dir_len   q
;   +16   branch_len         q
;   +24   repo_name_buf[64]
;   +88   relative_dir_buf[128]
;   +216  branch_buf[40]
;
; Note: a real recon caches per-cwd for 30s. We don't bother yet — the cost
; is two fork/execve per pane per refresh, which is acceptable at 2s cadence.

GIT_OFF_REPO_LEN  = 0
GIT_OFF_DIR_LEN   = 8
GIT_OFF_BR_LEN    = 16
GIT_OFF_REPO_BUF  = 24
GIT_OFF_DIR_BUF   = 88
GIT_OFF_BR_BUF    = 216
GIT_INFO_BYTES    = 256

segment readable executable

; _trim_lf(rdi=ptr, rsi=len) -> rax = trimmed_len.
; Drop trailing CR/LF/whitespace.
_trim_lf:
    mov     rax, rsi
.loop:
    test    rax, rax
    jz      .done
    movzx   ecx, byte [rdi + rax - 1]
    cmp     cl, 0x0A
    je      .step
    cmp     cl, 0x0D
    je      .step
    cmp     cl, ' '
    je      .step
    jmp     .done
.step:
    dec     rax
    jmp     .loop
.done:
    ret

; _basename(rdi=ptr, rsi=len) -> rax = basename_ptr, rdx = basename_len.
; Returns the substring after the last '/'. If no slash, returns the whole.
_basename:
    mov     rax, rdi                 ; default: whole
    mov     rdx, rsi
    mov     rcx, rsi
.scan:
    test    rcx, rcx
    jz      .done
    cmp     byte [rdi + rcx - 1], '/'
    je      .found
    dec     rcx
    jmp     .scan
.found:
    lea     rax, [rdi + rcx]
    sub     rdx, rcx
.done:
    ret

; git_project_info(rdi=cwd_zero_terminated, rsi=&info_out) -> rax = 0/-1
; Caller must NUL-terminate cwd.
git_project_info:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    mov     rbx, rdi                 ; cwd0
    mov     rbp, rsi                 ; &info

    ; --- argv for `git -C <cwd> rev-parse --show-toplevel` ---
    mov     qword [_gi_argv + 0],  _gi_path_git
    mov     qword [_gi_argv + 8],  _gi_arg_C
    mov     qword [_gi_argv + 16], rbx
    mov     qword [_gi_argv + 24], _gi_arg_revparse
    mov     qword [_gi_argv + 32], _gi_arg_toplevel
    mov     qword [_gi_argv + 40], 0

    lea     rdi, [_gi_path_git]
    lea     rsi, [_gi_argv]
    lea     rdx, [_gi_buf]
    mov     rcx, _GI_BUF_BYTES
    call    exec_capture
    test    rax, rax
    js      .err
    mov     r12, rax                 ; bytes
    test    rdx, rdx
    jnz     .not_git                 ; nonzero exit -> not a repo

    ; trim trailing
    lea     rdi, [_gi_buf]
    mov     rsi, r12
    call    _trim_lf
    mov     r12, rax                 ; toplevel_len

    ; basename → repo name
    lea     rdi, [_gi_buf]
    mov     rsi, r12
    call    _basename
    cmp     rdx, 64
    jbe     .repo_ok
    mov     rdx, 64
.repo_ok:
    mov     qword [rbp + GIT_OFF_REPO_LEN], rdx
    push    rdx
    mov     rsi, rax
    lea     rdi, [rbp + GIT_OFF_REPO_BUF]
    call    memcpy
    pop     rdx

    ; --- relative_dir = cwd minus toplevel prefix ---
    ;   if cwd starts with toplevel and len > toplevel_len + 1, take rest after '/'
    ;   else relative_dir is empty
    push    rdx                      ; will rebalance with two saves
    mov     rdx, qword [rbp + GIT_OFF_REPO_LEN]   ; placeholder (not used here)
    pop     rdx

    ; compute strlen(rbx)
    mov     rdi, rbx
    call    strlen
    mov     r13, rax                 ; cwd len

    cmp     r13, r12
    jbe     .no_reldir
    ; check prefix match
    mov     rdi, rbx
    lea     rsi, [_gi_buf]
    mov     rdx, r12
    call    memcmp
    test    rax, rax
    jnz     .no_reldir
    ; ensure next byte is '/'
    cmp     byte [rbx + r12], '/'
    jne     .no_reldir

    lea     rdi, [rbx + r12 + 1]     ; rel_dir_start
    mov     rax, r13
    sub     rax, r12
    dec     rax                      ; rel_dir_len
    cmp     rax, 128
    jbe     .dir_ok
    mov     rax, 128
.dir_ok:
    mov     qword [rbp + GIT_OFF_DIR_LEN], rax
    mov     rsi, rdi
    lea     rdi, [rbp + GIT_OFF_DIR_BUF]
    mov     rdx, rax
    push    rax
    call    memcpy
    pop     rax
    jmp     .branch
.no_reldir:
    mov     qword [rbp + GIT_OFF_DIR_LEN], 0

.branch:
    ; --- argv for `git -C <cwd> rev-parse --abbrev-ref HEAD` ---
    mov     qword [_gi_argv + 24], _gi_arg_revparse
    mov     qword [_gi_argv + 32], _gi_arg_abbrev
    mov     qword [_gi_argv + 40], _gi_arg_head
    mov     qword [_gi_argv + 48], 0

    lea     rdi, [_gi_path_git]
    lea     rsi, [_gi_argv]
    lea     rdx, [_gi_buf]
    mov     rcx, _GI_BUF_BYTES
    call    exec_capture
    test    rax, rax
    js      .br_skip
    mov     r12, rax
    test    rdx, rdx
    jnz     .br_skip

    lea     rdi, [_gi_buf]
    mov     rsi, r12
    call    _trim_lf
    cmp     rax, 40
    jbe     .br_ok
    mov     rax, 40
.br_ok:
    mov     qword [rbp + GIT_OFF_BR_LEN], rax
    push    rax
    mov     rsi, _gi_buf
    lea     rdi, [rbp + GIT_OFF_BR_BUF]
    mov     rdx, rax
    call    memcpy
    pop     rax
    xor     eax, eax
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

.br_skip:
    mov     qword [rbp + GIT_OFF_BR_LEN], 0
    xor     eax, eax
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

.not_git:
.err:
    mov     qword [rbp + GIT_OFF_REPO_LEN], 0
    mov     qword [rbp + GIT_OFF_DIR_LEN], 0
    mov     qword [rbp + GIT_OFF_BR_LEN], 0
    xor     eax, eax
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

segment readable
_gi_path_git     db '/usr/bin/git', 0
_gi_arg_C        db '-C', 0
_gi_arg_revparse db 'rev-parse', 0
_gi_arg_toplevel db '--show-toplevel', 0
_gi_arg_abbrev   db '--abbrev-ref', 0
_gi_arg_head     db 'HEAD', 0

_GI_BUF_BYTES = 4096

segment readable writeable
align 8
_gi_argv  rq 8
_gi_buf   rb _GI_BUF_BYTES
