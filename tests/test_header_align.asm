; test_header_align.asm
;
; The static `hdr_cols` string is what we paint at the top of the TUI; it must
; line up with the columns that `draw_row` emits below it. The cell widths
; live in app/header.inc as COL_*_W / COL_*_OFF; here we assert that each
; label starts at the offset its column begins, so a row's data sits directly
; under its label.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    call    arena_init
    ; populate hdr_cols at runtime (lives in arena now)
    arena_lea r12, ARENA_OFF_HDR_COLS
    mov     rdi, r12
    mov     esi, HDR_BUF_CAP
    call    build_header
    mov     qword [hdr_cols_len], rax

    ; total width matches the sum of column widths
    mov     rax, qword [hdr_cols_len]
    assert_eq_n 1, rax, COL_TOTAL_W

    ; col 0 is "#"
    movzx   eax, byte [r12 + COL_NUM_OFF]
    assert_eq_n 2, rax, '#'

    ; col 4: "Session"
    movzx   eax, byte [r12 + COL_SESSION_OFF]
    assert_eq_n 10, rax, 'S'
    movzx   eax, byte [r12 + COL_SESSION_OFF - 1]
    assert_eq_n 11, rax, ' '

    ; col 21: "Project"
    movzx   eax, byte [r12 + COL_PROJECT_OFF]
    assert_eq_n 20, rax, 'P'
    movzx   eax, byte [r12 + COL_PROJECT_OFF - 1]
    assert_eq_n 21, rax, ' '

    ; col 46: "Status"
    movzx   eax, byte [r12 + COL_STATUS_OFF]
    assert_eq_n 30, rax, 'S'
    movzx   eax, byte [r12 + COL_STATUS_OFF - 1]
    assert_eq_n 31, rax, ' '

    ; col 57: "Model"
    movzx   eax, byte [r12 + COL_MODEL_OFF]
    assert_eq_n 40, rax, 'M'
    movzx   eax, byte [r12 + COL_MODEL_OFF - 1]
    assert_eq_n 41, rax, ' '

    ; col 70: "Context"
    movzx   eax, byte [r12 + COL_CONTEXT_OFF]
    assert_eq_n 50, rax, 'C'
    movzx   eax, byte [r12 + COL_CONTEXT_OFF - 1]
    assert_eq_n 51, rax, ' '

    ; col 85: "Last"
    movzx   eax, byte [r12 + COL_LAST_OFF]
    assert_eq_n 60, rax, 'L'
    movzx   eax, byte [r12 + COL_LAST_OFF - 1]
    assert_eq_n 61, rax, ' '

    test_pass

include '../src/lib.inc'
