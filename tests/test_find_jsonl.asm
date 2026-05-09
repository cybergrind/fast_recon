; test_find_jsonl.asm
;
; find_jsonl_path(rdi=sid, rsi=sid_len, rdx=outbuf, rcx=cap)
;   -> rax = path_len, r8 = 0/-1
;
; Strategy: create fixture
;   $HOME/.claude/projects/-fastrecon-test-X/<SID>.jsonl
; then call find_jsonl_path with SID, expect a path that ends with the
; right filename. Cleanup deletes both file and dir.

format ELF64 executable 3
include '../src/common.inc'

SYS_mkdir = 83
SYS_rmdir = 84

segment readable executable
entry $
    call    arena_init
    ; build $HOME -> dir_path
    lea     rdi, [dir_path]
    mov     esi, 256
    call    home_dir
    test    r8, r8
    js      .die
    mov     r13, rax                 ; len so far

    ; append fixed dir suffix
    lea     rdi, [dir_path + r13]
    lea     rsi, [seg_dir]
    mov     edx, seg_dir_len
    call    memcpy
    add     r13, seg_dir_len
    mov     byte [dir_path + r13], 0

    ; mkdir(dir_path, 0755) — ignore EEXIST
    lea     rdi, [dir_path]
    mov     esi, 0755o
    mov     eax, SYS_mkdir
    syscall

    ; build dir_path/<SID>.jsonl into file_path
    lea     rdi, [file_path]
    lea     rsi, [dir_path]
    mov     rdx, r13
    call    memcpy
    mov     byte [file_path + r13], '/'
    inc     r13
    lea     rdi, [file_path + r13]
    lea     rsi, [test_sid]
    mov     edx, test_sid_len
    call    memcpy
    add     r13, test_sid_len
    lea     rdi, [file_path + r13]
    lea     rsi, [ext]
    mov     edx, ext_len
    call    memcpy
    add     r13, ext_len
    mov     byte [file_path + r13], 0

    ; create empty file
    lea     rdi, [file_path]
    mov     esi, O_WRONLY or O_CREAT or O_TRUNC
    mov     edx, 0644o
    mov     eax, SYS_open
    syscall
    test    rax, rax
    js      .die
    mov     rdi, rax
    call    sys_close

    ; --- find_jsonl_path ---
    lea     rdi, [test_sid]
    mov     esi, test_sid_len
    lea     rdx, [outbuf]
    mov     ecx, 1024
    call    find_jsonl_path
    assert_eq_n 1, r8, 0
    cmp     rax, 0
    jle     .die_assert
    ; sanity: rax should be reasonable path length
    cmp     rax, 1024
    jae     .die_assert
    mov     r14, rax                 ; save length


    ; the returned path should end with /<SID>.jsonl — compare last bytes
    lea     rdi, [outbuf]
    add     rdi, r14
    sub     rdi, test_sid_len + ext_len
    lea     rsi, [test_sid]
    mov     edx, test_sid_len + ext_len
    call    memcmp
    assert_eq_n 2, rax, 0

    ; cleanup
    lea     rdi, [file_path]
    mov     eax, SYS_unlink
    syscall
    lea     rdi, [dir_path]
    mov     eax, SYS_rmdir
    syscall

    test_pass
.die:
    mov     edi, 90
    mov     eax, SYS_exit
    syscall
.die_assert:
    mov     edi, 91
    mov     eax, SYS_exit
    syscall

include '../src/lib.inc'

segment readable
seg_dir     db '/.claude/projects/-fastrecon-test-find-jsonl'
seg_dir_len = $ - seg_dir
test_sid    db '00000000-aaaa-bbbb-cccc-fastrecontest'
test_sid_len = $ - test_sid
ext         db '.jsonl'
ext_len     = $ - ext
dbg_pre     db 'path='
dbg_lf      db 0x0A

segment readable writeable
dbg_num     rb 24

segment readable writeable
dir_path  rb 512
file_path rb 1024
outbuf    rb 1024
