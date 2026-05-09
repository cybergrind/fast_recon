; test_reconcile.asm
;
; reconcile_status_last(rdi=row_ptr, rsi=mtime_or_neg, rdx=now_secs)
;
; Three bands keyed off age = now - mtime:
;   age <= FRESH_SECS (30 s)             → WORKING, ts = max(ts, mtime)
;   FRESH_SECS < age <= STALE_SECS (30m) → trust the spinner; no change
;   age > STALE_SECS                     → flip WORKING → IDLE; ts unchanged
;
; Middle band exists because Claude can think silently for minutes during
; a long generation; we don't want to flip those to Idle.

format ELF64 executable 3
include '../src/common.inc'

NOW = 1000000

segment readable executable
entry $
    ; --- case A: mtime fresh, status WORKING already, ts old ---
    lea     rdi, [row]
    xor     esi, esi
    mov     edx, ROW_BYTES
    call    memset
    mov     qword [row + ROW_OFF_STATUS], STATUS_WORKING
    mov     qword [row + ROW_OFF_TS_EPOCH], NOW - 3600   ; 1h old
    lea     rdi, [row]
    mov     rsi, NOW - 5                                  ; mtime 5s ago
    mov     rdx, NOW
    call    reconcile_status_last
    mov     rax, qword [row + ROW_OFF_STATUS]
    assert_eq_n 1, rax, STATUS_WORKING
    mov     rax, qword [row + ROW_OFF_TS_EPOCH]
    mov     rcx, NOW - 5
    assert_eq_n 2, rax, rcx                               ; bumped to mtime

    ; --- case B: mtime fresh, status IDLE, ts old → Working+bump ---
    lea     rdi, [row]
    xor     esi, esi
    mov     edx, ROW_BYTES
    call    memset
    mov     qword [row + ROW_OFF_STATUS], STATUS_IDLE
    mov     qword [row + ROW_OFF_TS_EPOCH], NOW - 3600
    lea     rdi, [row]
    mov     rsi, NOW - 10
    mov     rdx, NOW
    call    reconcile_status_last
    mov     rax, qword [row + ROW_OFF_STATUS]
    assert_eq_n 10, rax, STATUS_WORKING
    mov     rax, qword [row + ROW_OFF_TS_EPOCH]
    mov     rcx, NOW - 10
    assert_eq_n 11, rax, rcx

    ; --- case C: middle band (300 s stale) + Working → unchanged ---
    ; Claude is plausibly mid-turn; we trust the spinner classifier.
    lea     rdi, [row]
    xor     esi, esi
    mov     edx, ROW_BYTES
    call    memset
    mov     qword [row + ROW_OFF_STATUS], STATUS_WORKING
    mov     qword [row + ROW_OFF_TS_EPOCH], NOW - 300
    lea     rdi, [row]
    mov     rsi, NOW - 300                                ; 5m stale
    mov     rdx, NOW
    call    reconcile_status_last
    mov     rax, qword [row + ROW_OFF_STATUS]
    assert_eq_n 20, rax, STATUS_WORKING                   ; spinner trusted
    mov     rax, qword [row + ROW_OFF_TS_EPOCH]
    mov     rcx, NOW - 300
    assert_eq_n 21, rax, rcx

    ; --- case C2: very stale (>STALE_SECS) + Working → flip to Idle ---
    lea     rdi, [row]
    xor     esi, esi
    mov     edx, ROW_BYTES
    call    memset
    mov     qword [row + ROW_OFF_STATUS], STATUS_WORKING
    mov     qword [row + ROW_OFF_TS_EPOCH], NOW - 3600
    lea     rdi, [row]
    mov     rsi, NOW - 2880                               ; 48m stale
    mov     rdx, NOW
    call    reconcile_status_last
    mov     rax, qword [row + ROW_OFF_STATUS]
    assert_eq_n 22, rax, STATUS_IDLE
    mov     rax, qword [row + ROW_OFF_TS_EPOCH]
    mov     rcx, NOW - 3600
    assert_eq_n 23, rax, rcx                              ; unchanged

    ; --- case D: very stale + status IDLE → unchanged ---
    lea     rdi, [row]
    xor     esi, esi
    mov     edx, ROW_BYTES
    call    memset
    mov     qword [row + ROW_OFF_STATUS], STATUS_IDLE
    mov     qword [row + ROW_OFF_TS_EPOCH], NOW - 3600
    lea     rdi, [row]
    mov     rsi, NOW - 2880
    mov     rdx, NOW
    call    reconcile_status_last
    mov     rax, qword [row + ROW_OFF_STATUS]
    assert_eq_n 30, rax, STATUS_IDLE
    mov     rax, qword [row + ROW_OFF_TS_EPOCH]
    mov     rcx, NOW - 3600
    assert_eq_n 31, rax, rcx

    ; --- case E: no mtime (-1), status WORKING → unchanged (no signal) ---
    lea     rdi, [row]
    xor     esi, esi
    mov     edx, ROW_BYTES
    call    memset
    mov     qword [row + ROW_OFF_STATUS], STATUS_WORKING
    mov     qword [row + ROW_OFF_TS_EPOCH], NOW - 60
    lea     rdi, [row]
    mov     rsi, -1
    mov     rdx, NOW
    call    reconcile_status_last
    mov     rax, qword [row + ROW_OFF_STATUS]
    assert_eq_n 40, rax, STATUS_WORKING
    mov     rax, qword [row + ROW_OFF_TS_EPOCH]
    mov     rcx, NOW - 60
    assert_eq_n 41, rax, rcx

    ; --- case F: status NEW (0) preserved regardless ---
    lea     rdi, [row]
    xor     esi, esi
    mov     edx, ROW_BYTES
    call    memset
    mov     qword [row + ROW_OFF_STATUS], STATUS_NEW
    mov     qword [row + ROW_OFF_TS_EPOCH], 0
    lea     rdi, [row]
    mov     rsi, -1
    mov     rdx, NOW
    call    reconcile_status_last
    mov     rax, qword [row + ROW_OFF_STATUS]
    assert_eq_n 50, rax, STATUS_NEW

    test_pass

include '../src/lib.inc'

segment readable writeable
align 8
row rb ROW_BYTES
