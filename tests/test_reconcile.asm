; test_reconcile.asm
;
; reconcile_status_last(rdi=row_ptr, rsi=mtime_or_neg, rdx=now_secs)
;
; Mutates row[STATUS] and row[TS_EPOCH] so the Status and Last columns
; agree with the JSONL file's mtime, which is the authoritative "is this
; session writing right now" signal.
;
; Rules:
;   1. If mtime>=0 AND now-mtime <= FRESH_SECS:
;        row.status   = STATUS_WORKING
;        row.ts_epoch = max(row.ts_epoch, mtime)
;   2. Else if mtime>=0 AND now-mtime > FRESH_SECS AND row.status==WORKING:
;        row.status   = STATUS_IDLE   (spinner was a false positive)
;        row.ts_epoch unchanged
;   3. Else: no change.
;
; FRESH_SECS = 30.

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

    ; --- case C: mtime stale + status WORKING → flip to IDLE, ts unchanged ---
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
    assert_eq_n 20, rax, STATUS_IDLE
    mov     rax, qword [row + ROW_OFF_TS_EPOCH]
    mov     rcx, NOW - 3600
    assert_eq_n 21, rax, rcx                              ; unchanged

    ; --- case D: mtime stale + status IDLE → unchanged ---
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
