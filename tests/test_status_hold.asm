; test_status_hold.asm
;
; status_with_hold(rdi=pid, rsi=raw_status, rdx=now_epoch) -> rax = effective
;
; Per-pid debounce: once a pid is observed Working, the function reports
; Working for HOLD_SECS more even when the raw classifier says Idle, to
; mask the second-by-second flicker between assistant turns.
;
; INPUT/NEW pass through unchanged. Different pids tracked independently.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    call    status_hold_reset

    ; --- 1. raw=Idle with no history → Idle ---
    mov     edi, 4321
    mov     esi, STATUS_IDLE
    mov     rdx, 100
    call    status_with_hold
    assert_eq_n 1, rax, STATUS_IDLE

    ; --- 2. raw=Working stamps the pid; subsequent Idle within HOLD held → Working ---
    mov     edi, 4321
    mov     esi, STATUS_WORKING
    mov     rdx, 100
    call    status_with_hold
    assert_eq_n 2, rax, STATUS_WORKING

    mov     edi, 4321
    mov     esi, STATUS_IDLE
    mov     rdx, 110
    call    status_with_hold
    assert_eq_n 3, rax, STATUS_WORKING

    mov     edi, 4321
    mov     esi, STATUS_IDLE
    mov     rdx, 130
    call    status_with_hold
    assert_eq_n 4, rax, STATUS_WORKING       ; exactly at boundary still held

    ; --- 3. once HOLD elapsed, Idle returns Idle ---
    mov     edi, 4321
    mov     esi, STATUS_IDLE
    mov     rdx, 131
    call    status_with_hold
    assert_eq_n 5, rax, STATUS_IDLE

    ; --- 4. a fresh Working refreshes the stamp ---
    mov     edi, 4321
    mov     esi, STATUS_WORKING
    mov     rdx, 200
    call    status_with_hold
    assert_eq_n 6, rax, STATUS_WORKING
    mov     edi, 4321
    mov     esi, STATUS_IDLE
    mov     rdx, 220
    call    status_with_hold
    assert_eq_n 7, rax, STATUS_WORKING

    ; --- 5. different pids do not bleed into each other ---
    call    status_hold_reset
    mov     edi, 1111
    mov     esi, STATUS_WORKING
    mov     rdx, 100
    call    status_with_hold
    mov     edi, 2222
    mov     esi, STATUS_IDLE
    mov     rdx, 110
    call    status_with_hold
    assert_eq_n 10, rax, STATUS_IDLE         ; 2222 has no history

    mov     edi, 1111
    mov     esi, STATUS_IDLE
    mov     rdx, 110
    call    status_with_hold
    assert_eq_n 11, rax, STATUS_WORKING      ; 1111 still held

    ; --- 6. INPUT and NEW pass through verbatim ---
    call    status_hold_reset
    mov     edi, 4321
    mov     esi, STATUS_INPUT
    mov     rdx, 100
    call    status_with_hold
    assert_eq_n 20, rax, STATUS_INPUT
    mov     edi, 4321
    mov     esi, STATUS_NEW
    mov     rdx, 100
    call    status_with_hold
    assert_eq_n 21, rax, STATUS_NEW

    test_pass

include '../src/lib.inc'
