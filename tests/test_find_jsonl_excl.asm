; test_find_jsonl_excl.asm
;
; find_recent_jsonl_in_cwd(rdi=cwd, rsi=cwd_len, rdx=outbuf, rcx=cap,
;                          r8=claimed_buf, r9=claimed_count) -> rax=len, r8=0/-1
;
; claimed_buf is a flat array of 40-byte slots; each slot holds a UUID stem
; (filename without ".jsonl"), NUL-padded to 40 bytes.
;
; Test: create three fixture .jsonl files A/B/C with distinct mtimes.
;   call#1: claimed=[]      → C.jsonl
;   call#2: claimed=[C]     → B.jsonl
;   call#3: claimed=[B,C]   → A.jsonl
;   call#4: claimed=[A,B,C] → r8 = -1

format ELF64 executable 3
include '../src/common.inc'

SYS_mkdir = 83
SYS_rmdir = 84

CLAIM_SLOT = 40

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

    ; mkdir(dir_path, 0755)
    lea     rdi, [dir_path]
    mov     esi, 0755o
    mov     eax, SYS_mkdir
    syscall

    ; append "/" to dir_path so create_jsonl can just concat name
    mov     byte [dir_path + r13], '/'
    inc     r13
    mov     byte [dir_path + r13], 0
    mov     qword [dir_path_len], r13

    ; create A.jsonl, B.jsonl, C.jsonl in order with small sleeps for distinct mtimes
    lea     rdi, [name_a]
    call    create_jsonl
    call    sleep_5ms
    lea     rdi, [name_b]
    call    create_jsonl
    call    sleep_5ms
    lea     rdi, [name_c]
    call    create_jsonl

    ; ---- call#1: claimed=[]  → expect tail = "C.jsonl" ----
    lea     rdi, [seg_cwd]
    mov     esi, seg_cwd_len
    lea     rdx, [outbuf]
    mov     ecx, 1024
    xor     r8, r8
    xor     r9, r9
    call    find_recent_jsonl_in_cwd
    assert_eq_n 1, r8, 0
    test    rax, rax
    jz      .die
    mov     r12, rax
    ; tail must be "C.jsonl" (7 bytes)
    lea     rdi, [outbuf + r12 - 7]
    lea     rsi, [name_c]
    mov     edx, 7
    call    memcmp
    assert_eq_n 2, rax, 0

    ; ---- call#2: claimed=[C]  → expect "B.jsonl" ----
    ; copy "C" stem (1 char) into claimed[0]
    lea     rdi, [claimed]
    xor     esi, esi
    mov     edx, 2 * CLAIM_SLOT
    call    memset
    mov     byte [claimed + 0], 'C'
    lea     rdi, [seg_cwd]
    mov     esi, seg_cwd_len
    lea     rdx, [outbuf]
    mov     ecx, 1024
    lea     r8, [claimed]
    mov     r9, 1
    call    find_recent_jsonl_in_cwd
    assert_eq_n 10, r8, 0
    mov     r12, rax
    lea     rdi, [outbuf + r12 - 7]
    lea     rsi, [name_b]
    mov     edx, 7
    call    memcmp
    assert_eq_n 11, rax, 0

    ; ---- call#3: claimed=[B,C] → expect "A.jsonl" ----
    mov     byte [claimed + 0 * CLAIM_SLOT], 'B'
    mov     byte [claimed + 1 * CLAIM_SLOT], 'C'
    lea     rdi, [seg_cwd]
    mov     esi, seg_cwd_len
    lea     rdx, [outbuf]
    mov     ecx, 1024
    lea     r8, [claimed]
    mov     r9, 2
    call    find_recent_jsonl_in_cwd
    assert_eq_n 20, r8, 0
    mov     r12, rax
    lea     rdi, [outbuf + r12 - 7]
    lea     rsi, [name_a]
    mov     edx, 7
    call    memcmp
    assert_eq_n 21, rax, 0

    ; ---- call#4: claimed=[A,B,C] → r8 = -1 ----
    mov     byte [claimed + 0 * CLAIM_SLOT], 'A'
    mov     byte [claimed + 1 * CLAIM_SLOT], 'B'
    mov     byte [claimed + 2 * CLAIM_SLOT], 'C'
    lea     rdi, [seg_cwd]
    mov     esi, seg_cwd_len
    lea     rdx, [outbuf]
    mov     ecx, 1024
    lea     r8, [claimed]
    mov     r9, 3
    call    find_recent_jsonl_in_cwd
    assert_eq_n 30, r8, -1

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

; create_jsonl(rdi=name_ptr_3bytes "X.jsonl") — touches dir_path/<name>
create_jsonl:
    push    r12
    mov     r12, rdi
    ; build dir_path + "/" + name into file_path
    lea     rdi, [file_path]
    lea     rsi, [dir_path]
    mov     rdx, qword [dir_path_len]
    call    memcpy
    mov     rax, qword [dir_path_len]
    lea     rdi, [file_path + rax]
    mov     rsi, r12
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
    call    sys_close
    pop     r12
    ret

; unlink_in_dir(rdi=name_ptr) — unlink dir_path/<name>
unlink_in_dir:
    push    r12
    mov     r12, rdi
    lea     rdi, [file_path]
    lea     rsi, [dir_path]
    mov     rdx, qword [dir_path_len]
    call    memcpy
    mov     rax, qword [dir_path_len]
    lea     rdi, [file_path + rax]
    mov     rsi, r12
    mov     edx, 7
    call    memcpy
    mov     rax, qword [dir_path_len]
    add     rax, 7
    mov     byte [file_path + rax], 0
    lea     rdi, [file_path]
    mov     eax, SYS_unlink
    syscall
    pop     r12
    ret

; sleep_5ms()
sleep_5ms:
    sub     rsp, 16
    mov     qword [rsp], 0           ; tv_sec
    mov     qword [rsp + 8], 5000000 ; tv_nsec = 5 ms
    mov     rdi, rsp
    xor     rsi, rsi
    mov     eax, SYS_nanosleep
    syscall
    add     rsp, 16
    ret

include '../src/lib.inc'

segment readable
seg_dir     db '/.claude/projects/-fastrecon-test-excl'
seg_dir_len = $ - seg_dir
seg_cwd     db '/fastrecon/test/excl'
seg_cwd_len = $ - seg_cwd
name_a      db 'A.jsonl'
name_b      db 'B.jsonl'
name_c      db 'C.jsonl'
dbg_lf      db 0x0A

segment readable writeable
align 8
dir_path     rb 256
dir_path_len rq 1
file_path    rb 512
outbuf       rb 1024
claimed      rb 4 * CLAIM_SLOT
