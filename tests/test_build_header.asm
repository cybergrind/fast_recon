; test_build_header.asm
;
; build_header(rdi=dst, rsi=cap) -> rax = bytes written
;   Renders the column header into dst by walking the column table
;   defined in app/header.inc. Each column emits its label followed by
;   spaces up to its field width, so labels land at COL_*_OFF.
;
;   Returns COL_TOTAL_W on success, or 0 if cap < COL_TOTAL_W.
;
; Tests:
;   1. Returned length equals COL_TOTAL_W.
;   2. Each column label starts at its declared offset.
;   3. Every byte in [0, COL_TOTAL_W) is a printable byte (no embedded NULs)
;      and gaps between labels are exactly 0x20 (space).
;   4. The byte at offset COL_TOTAL_W is left untouched (sentinel preserved).
;   5. cap < COL_TOTAL_W returns 0 and leaves the buffer unchanged.

format ELF64 executable 3
include '../src/common.inc'

SENTINEL = 0xCC

segment readable executable
entry $
    ; --- short-cap path: fill buf with sentinel, call with cap=10 ---
    lea     rdi, [buf]
    mov     esi, SENTINEL
    mov     edx, 256
    call    memset

    lea     rdi, [buf]
    mov     esi, 10
    call    build_header
    assert_eq_n 1, rax, 0
    movzx   eax, byte [buf + 0]
    assert_eq_n 2, rax, SENTINEL

    ; --- happy path: fill buf with sentinel, call with cap=256 ---
    lea     rdi, [buf]
    mov     esi, SENTINEL
    mov     edx, 256
    call    memset

    lea     rdi, [buf]
    mov     esi, 256
    call    build_header
    assert_eq_n 3, rax, COL_TOTAL_W

    ; first byte of each label
    movzx   eax, byte [buf + COL_NUM_OFF]
    assert_eq_n 10, rax, '#'
    movzx   eax, byte [buf + COL_SESSION_OFF]
    assert_eq_n 11, rax, 'S'
    movzx   eax, byte [buf + COL_PROJECT_OFF]
    assert_eq_n 12, rax, 'P'
    movzx   eax, byte [buf + COL_STATUS_OFF]
    assert_eq_n 13, rax, 'S'
    movzx   eax, byte [buf + COL_MODEL_OFF]
    assert_eq_n 14, rax, 'M'
    movzx   eax, byte [buf + COL_CONTEXT_OFF]
    assert_eq_n 15, rax, 'C'
    movzx   eax, byte [buf + COL_LAST_OFF]
    assert_eq_n 16, rax, 'L'

    ; byte just before each non-first label is a space
    movzx   eax, byte [buf + COL_SESSION_OFF - 1]
    assert_eq_n 20, rax, ' '
    movzx   eax, byte [buf + COL_PROJECT_OFF - 1]
    assert_eq_n 21, rax, ' '
    movzx   eax, byte [buf + COL_STATUS_OFF - 1]
    assert_eq_n 22, rax, ' '
    movzx   eax, byte [buf + COL_MODEL_OFF - 1]
    assert_eq_n 23, rax, ' '
    movzx   eax, byte [buf + COL_CONTEXT_OFF - 1]
    assert_eq_n 24, rax, ' '
    movzx   eax, byte [buf + COL_LAST_OFF - 1]
    assert_eq_n 25, rax, ' '

    ; trailing sentinel preserved
    movzx   eax, byte [buf + COL_TOTAL_W]
    assert_eq_n 30, rax, SENTINEL

    ; every byte in [0, COL_TOTAL_W) is printable (>= 0x20, <= 0x7E)
    xor     ecx, ecx
.scan:
    cmp     ecx, COL_TOTAL_W
    jae     .ok
    movzx   eax, byte [buf + rcx]
    cmp     eax, 0x20
    jb      .bad
    cmp     eax, 0x7E
    ja      .bad
    inc     ecx
    jmp     .scan
.bad:
    mov     edi, 40
    mov     eax, SYS_exit
    syscall
.ok:
    test_pass

include '../src/lib.inc'

segment readable writeable
align 8
buf  rb 256
