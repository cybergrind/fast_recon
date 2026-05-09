; test_pane_status.asm
;
; pane_status_from_buf(rdi=buf, rsi=len) -> rax = STATUS_*
;
; Mirrors the Working/Input/Idle classification recon already uses:
;   - Working: any non-empty line whose first non-whitespace character is a
;     Claude Code spinner glyph (✽ ✢ ✳ ✶ ✻ ⏺ · ...) and that contains the
;     ellipsis U+2026 ("…").
;   - Input: last non-empty line contains "Esc to cancel", or any line
;     contains "❯ <digit>".
;   - Idle: anything else.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; --- 1. plain idle pane ---
    lea     rdi, [pane_idle]
    mov     rsi, pane_idle_len
    call    pane_status_from_buf
    assert_eq_n 1, rax, STATUS_IDLE

    ; --- 2. "Esc to cancel" on the last non-empty line → Input ---
    lea     rdi, [pane_input_esc]
    mov     rsi, pane_input_esc_len
    call    pane_status_from_buf
    assert_eq_n 2, rax, STATUS_INPUT

    ; --- 3. ✽ spinner + ellipsis at end → Working ---
    lea     rdi, [pane_working_bottom]
    mov     rsi, pane_working_bottom_len
    call    pane_status_from_buf
    assert_eq_n 3, rax, STATUS_WORKING

    ; --- 4. ⏺ record glyph + ellipsis high in the pane → Working ---
    lea     rdi, [pane_working_top]
    mov     rsi, pane_working_top_len
    call    pane_status_from_buf
    assert_eq_n 4, rax, STATUS_WORKING

    ; --- 5. spinner without ellipsis is NOT working ---
    lea     rdi, [pane_spinner_no_ellipsis]
    mov     rsi, pane_spinner_no_ellipsis_len
    call    pane_status_from_buf
    assert_eq_n 5, rax, STATUS_IDLE

    ; --- 6. ❯ <digit> selection prompt → Input ---
    lea     rdi, [pane_input_select]
    mov     rsi, pane_input_select_len
    call    pane_status_from_buf
    assert_eq_n 6, rax, STATUS_INPUT

    ; --- 7. ellipsis present without spinner is NOT working ---
    lea     rdi, [pane_ellipsis_no_spinner]
    mov     rsi, pane_ellipsis_no_spinner_len
    call    pane_status_from_buf
    assert_eq_n 7, rax, STATUS_IDLE

    ; --- 8. zero-len buffer → Idle ---
    lea     rdi, [pane_idle]
    xor     rsi, rsi
    call    pane_status_from_buf
    assert_eq_n 8, rax, STATUS_IDLE

    test_pass

include '../src/lib.inc'

segment readable
; ✽ U+273D = E2 9C BD
; ⏺ U+23FA = E2 8F BA
; … U+2026 = E2 80 A6
; ❯ U+276F = E2 9D AF

pane_idle              db '$ ls', 0x0A, 'README.md  src', 0x0A
pane_idle_len          = $ - pane_idle

pane_input_esc         db '> Some prompt', 0x0A, 'Press Esc to cancel', 0x0A
pane_input_esc_len     = $ - pane_input_esc

pane_working_bottom    db 'Some output', 0x0A
                       db 0xE2, 0x9C, 0xBD, ' Thinking', 0xE2, 0x80, 0xA6, 0x0A
pane_working_bottom_len = $ - pane_working_bottom

pane_working_top       db 0xE2, 0x8F, 0xBA, ' Generating', 0xE2, 0x80, 0xA6, 0x0A
                       db 'lots of', 0x0A, 'tool output', 0x0A
                       db '$ ', 0x0A
pane_working_top_len   = $ - pane_working_top

pane_spinner_no_ellipsis  db 0xE2, 0x9C, 0xBD, ' just a spinner', 0x0A
pane_spinner_no_ellipsis_len = $ - pane_spinner_no_ellipsis

pane_input_select      db 'Choose:', 0x0A
                       db 0xE2, 0x9D, 0xAF, ' 1. yes', 0x0A
                       db '  2. no', 0x0A
pane_input_select_len  = $ - pane_input_select

pane_ellipsis_no_spinner  db 'plain text with ellipsis ', 0xE2, 0x80, 0xA6, 0x0A
pane_ellipsis_no_spinner_len = $ - pane_ellipsis_no_spinner
