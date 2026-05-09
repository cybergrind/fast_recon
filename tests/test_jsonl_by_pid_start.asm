; test_jsonl_by_pid_start.asm
;
; find_jsonl_by_pid_start(rdi=cwd, rsi=cwd_len, rdx=outbuf, rcx=cap,
;                         r8=pid_start_epoch) -> rax = path_len, r8 = 0/-1
;
; Scans $HOME/.claude/projects/<encoded_cwd>/*.jsonl, opens each, reads its
; first-line "timestamp" via jsonl_first_ts_epoch. Returns the path whose
; first-ts is closest to pid_start_epoch, requiring |delta| <= 300 s. Files
; with no parseable first line are ignored. Returns r8=-1 if none qualify.
;
; Fixtures:
;   A.jsonl  first-ts = 1768478400  (2026-01-15T12:00:00Z)
;   B.jsonl  first-ts = 1778329598  (2026-05-09T12:26:38Z) ← target
;   C.jsonl  first-ts = 1788329598  (2026-09-01...)
;
; pid_start_epoch = 1778329600 (2 s after B). Expect B.

format ELF64 executable 3
include '../src/common.inc'

SYS_mkdir = 83
SYS_rmdir = 84

segment readable executable
entry $
    call    arena_init

    ; ---- build fixture dir path ----
    lea     rdi, [dir_path]
    mov     esi, 256
    call    home_dir
    mov     r13, rax
    lea     rdi, [dir_path + r13]
    lea     rsi, [seg_dir]
    mov     edx, seg_dir_len
    call    memcpy
    add     r13, seg_dir_len
    mov     byte [dir_path + r13], 0

    lea     rdi, [dir_path]
    mov     esi, 0755o
    mov     eax, SYS_mkdir
    syscall

    mov     byte [dir_path + r13], '/'
    inc     r13
    mov     byte [dir_path + r13], 0
    mov     qword [dir_path_len], r13

    ; ---- write A.jsonl, B.jsonl, C.jsonl with distinct first-line ts ----
    lea     rdi, [name_a]
    lea     rsi, [docA]
    mov     rdx, docA_end - docA
    call    write_jsonl
    lea     rdi, [name_b]
    lea     rsi, [docB]
    mov     rdx, docB_end - docB
    call    write_jsonl
    lea     rdi, [name_c]
    lea     rsi, [docC]
    mov     rdx, docC_end - docC
    call    write_jsonl

    ; ---- pid_start_epoch = 1778329600 → must pick B ----
    lea     rdi, [seg_cwd]
    mov     esi, seg_cwd_len
    lea     rdx, [outbuf]
    mov     ecx, 1024
    mov     r8, 1778329600
    call    find_jsonl_by_pid_start
    assert_eq_n 1, r8, 0
    mov     r12, rax
    test    r12, r12
    jz      .die
    ; tail must be "B.jsonl" (7 bytes)
    lea     rdi, [outbuf + r12 - 7]
    lea     rsi, [name_b]
    mov     edx, 7
    call    memcmp
    assert_eq_n 2, rax, 0

    ; ---- pid_start_epoch nowhere near any file → r8 = -1 ----
    lea     rdi, [seg_cwd]
    mov     esi, seg_cwd_len
    lea     rdx, [outbuf]
    mov     ecx, 1024
    mov     r8, 1500000000               ; mid-2017, > 300s from any
    call    find_jsonl_by_pid_start
    assert_eq_n 10, r8, -1

    ; ---- cleanup ----
    lea     rdi, [name_a]
    call    unlink_in_dir
    lea     rdi, [name_b]
    call    unlink_in_dir
    lea     rdi, [name_c]
    call    unlink_in_dir
    lea     rdi, [dir_path]
    mov     eax, SYS_rmdir
    syscall

    test_pass
.die:
    mov     edi, 90
    mov     eax, SYS_exit
    syscall

; --- helpers ---
; write_jsonl(rdi=name_ptr_7bytes, rsi=content_ptr, rdx=content_len)
write_jsonl:
    push    rbx
    push    r12
    push    r13
    mov     rbx, rdi
    mov     r12, rsi
    mov     r13, rdx

    lea     rdi, [file_path]
    lea     rsi, [dir_path]
    mov     rdx, qword [dir_path_len]
    call    memcpy
    mov     rax, qword [dir_path_len]
    lea     rdi, [file_path + rax]
    mov     rsi, rbx
    mov     edx, 7
    call    memcpy
    mov     rax, qword [dir_path_len]
    add     rax, 7
    mov     byte [file_path + rax], 0

    lea     rdi, [file_path]
    mov     esi, O_WRONLY or O_CREAT or O_TRUNC
    mov     edx, 0644o
    mov     eax, SYS_open
    syscall
    mov     rdi, rax
    push    rax
    mov     rsi, r12
    mov     rdx, r13
    mov     eax, SYS_write
    syscall
    pop     rdi
    call    sys_close
    pop     r13
    pop     r12
    pop     rbx
    ret

; unlink_in_dir(rdi=name_ptr_7)
unlink_in_dir:
    push    rbx
    mov     rbx, rdi
    lea     rdi, [file_path]
    lea     rsi, [dir_path]
    mov     rdx, qword [dir_path_len]
    call    memcpy
    mov     rax, qword [dir_path_len]
    lea     rdi, [file_path + rax]
    mov     rsi, rbx
    mov     edx, 7
    call    memcpy
    mov     rax, qword [dir_path_len]
    add     rax, 7
    mov     byte [file_path + rax], 0
    lea     rdi, [file_path]
    mov     eax, SYS_unlink
    syscall
    pop     rbx
    ret

include '../src/lib.inc'

segment readable
seg_dir     db '/.claude/projects/-fastrecon-test-pidanchor'
seg_dir_len = $ - seg_dir
seg_cwd     db '/fastrecon/test/pidanchor'
seg_cwd_len = $ - seg_cwd
name_a      db 'A.jsonl'
name_b      db 'B.jsonl'
name_c      db 'C.jsonl'

docA db '{"type":"system","timestamp":"2026-01-15T12:00:00.000Z","content":"a"}', 0x0A
docA_end:
docB db '{"type":"system","timestamp":"2026-05-09T12:26:38.000Z","content":"b"}', 0x0A
docB_end:
docC db '{"type":"system","timestamp":"2026-09-01T00:00:00.000Z","content":"c"}', 0x0A
docC_end:

segment readable writeable
align 8
dir_path     rb 256
dir_path_len rq 1
file_path    rb 512
outbuf       rb 1024
