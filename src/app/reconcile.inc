; app/reconcile.inc — reconcile pane status and last-activity against the
; JSONL file's mtime, the authoritative "is this session writing right now"
; signal.
;
; Spinner detection in pane_status_from_buf can produce false positives
; (a paragraph in the visible pane just happens to contain a glyph that
; looks like a spinner). The mtime of the JSONL file Claude appends to is
; a much stronger signal — if it hasn't been touched in FRESH_SECS, the
; session is not writing.

FRESH_SECS = 30

segment readable executable

; reconcile_status_last(rdi=row_ptr, rsi=mtime_or_neg, rdx=now_secs)
;
;   rsi < 0 ⇒ no mtime available (no JSONL); leave row alone.
;
; Otherwise, with age = now - mtime:
;   age <= FRESH_SECS  → status = WORKING; ts_epoch = max(ts_epoch, mtime).
;   age >  FRESH_SECS  AND status == WORKING → status = IDLE; ts_epoch unchanged.
;   age >  FRESH_SECS  AND status != WORKING → leave alone.
;
; The IDLE-flip on stale mtime corrects spinner-detection false positives:
; if the file isn't being written, the pane isn't really working regardless
; of what the visible pane buffer suggests.
reconcile_status_last:
    test    rsi, rsi
    js      .ret                         ; no mtime → no change

    mov     rax, rdx
    sub     rax, rsi                     ; age = now - mtime
    cmp     rax, FRESH_SECS
    ja      .stale

    ; fresh: force WORKING, bump ts_epoch up to mtime
    mov     qword [rdi + ROW_OFF_STATUS], STATUS_WORKING
    mov     rax, qword [rdi + ROW_OFF_TS_EPOCH]
    cmp     rsi, rax
    jbe     .ret
    mov     qword [rdi + ROW_OFF_TS_EPOCH], rsi
    ret

.stale:
    mov     rax, qword [rdi + ROW_OFF_STATUS]
    cmp     rax, STATUS_WORKING
    jne     .ret
    mov     qword [rdi + ROW_OFF_STATUS], STATUS_IDLE
.ret:
    ret
