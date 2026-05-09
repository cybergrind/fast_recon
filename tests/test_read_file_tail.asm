; test_read_file_tail.asm
;
; read_file_tail(rdi=path0, rsi=tail_cap)
;   ok:  rax = ptr (mmap'd MAP_PRIVATE | PROT_READ),
;        rdx = mapped length,
;        rcx = 0,
;        r8  = file offset of byte 0 of the mapping (0 ⇒ whole-file mapping)
;   err: rax = 0, rcx = -errno (rdx, r8 undefined)
;
; The mapping always covers up to the end of file. When file_size > tail_cap
; the start offset is rounded down to a 4 KB page boundary, so the mapping
; may be slightly larger than tail_cap.
;
; Verifies: small-file path (whole), large-file path (tail aligned), and
; that the tail ptr is byte-accurate against the source file.

format ELF64 executable 3
include '../src/common.inc'

SYS_mkdir = 83
SYS_rmdir = 84
PAGE_SIZE = 4096

; choose a file size > one page so the tail path actually triggers
BIG_BYTES = 10000
TAIL_CAP  = 4096

segment readable executable
entry $
    ; --- create big_path = $TMPDIR/test_read_file_tail.bin ---
    lea     rdi, [big_path]
    lea     rsi, [seg_path]
    mov     edx, seg_path_len
    call    memcpy
    mov     byte [big_path + seg_path_len], 0

    ; open(O_WRONLY|O_CREAT|O_TRUNC, 0644)
    lea     rdi, [big_path]
    mov     esi, O_WRONLY or O_CREAT or O_TRUNC
    mov     edx, 0644o
    mov     eax, SYS_open
    syscall
    test    rax, rax
    jns     .open_ok
    mov     edi, 30
    mov     eax, SYS_exit
    syscall
.open_ok:
    mov     r12, rax                 ; fd

    ; fill content_buf with i -> 'A' + (i % 26)
    xor     ecx, ecx
.fill:
    cmp     ecx, BIG_BYTES
    jae     .fill_done
    mov     eax, ecx
    cdq
    mov     r9d, 26
    div     r9d
    add     edx, 'A'
    mov     [content_buf + rcx], dl
    inc     ecx
    jmp     .fill
.fill_done:

    ; write(fd, content_buf, BIG_BYTES)
    mov     rdi, r12
    lea     rsi, [content_buf]
    mov     rdx, BIG_BYTES
    mov     eax, SYS_write
    syscall
    mov     rdi, r12
    call    sys_close

    ; --- 1. tail with cap=TAIL_CAP on a 10000-byte file ---
    lea     rdi, [big_path]
    mov     rsi, TAIL_CAP
    call    read_file_tail
    assert_eq_n 1, rcx, 0
    ; expected file offset: floor((BIG_BYTES - TAIL_CAP) / PAGE_SIZE) * PAGE_SIZE
    ;   = floor((10000-4096)/4096)*4096 = floor(5904/4096)*4096 = 4096
    assert_eq_n 2, r8, 4096
    ; mapped length must be BIG_BYTES - 4096 = 5904
    assert_eq_n 3, rdx, BIG_BYTES - 4096

    mov     r12, rax                 ; ptr
    mov     r13, rdx                 ; len
    ; first byte at offset 4096 in file should be 'A' + (4096 % 26) = 'A' + 14 = 'O'
    movzx   eax, byte [r12]
    assert_eq_n 4, rax, 'O'
    ; last byte at offset 9999 → 'A' + (9999 % 26) = 'A' + (9999 - 384*26) = 'A' + 15 = 'P'
    movzx   eax, byte [r12 + r13 - 1]
    assert_eq_n 5, rax, 'P'

    ; release
    mov     rdi, r12
    mov     rsi, r13
    call    release_file

    ; --- 2. small file (size < cap) returns whole, offset=0 ---
    ; truncate same path to 100 bytes by re-opening and writing 100 bytes
    lea     rdi, [big_path]
    mov     esi, O_WRONLY or O_CREAT or O_TRUNC
    mov     edx, 0644o
    mov     eax, SYS_open
    syscall
    mov     r12, rax
    mov     rdi, r12
    lea     rsi, [content_buf]
    mov     rdx, 100
    mov     eax, SYS_write
    syscall
    mov     rdi, r12
    call    sys_close

    lea     rdi, [big_path]
    mov     rsi, TAIL_CAP
    call    read_file_tail
    assert_eq_n 10, rcx, 0
    assert_eq_n 11, r8, 0
    assert_eq_n 12, rdx, 100
    movzx   eax, byte [rax]
    assert_eq_n 13, rax, 'A'

    ; cleanup
    lea     rdi, [big_path]
    mov     eax, SYS_unlink
    syscall

    test_pass

include '../src/lib.inc'

segment readable
seg_path     db '/tmp/test_read_file_tail.bin'
seg_path_len = $ - seg_path

segment readable writeable
align 8
big_path     rb 256
content_buf  rb BIG_BYTES
