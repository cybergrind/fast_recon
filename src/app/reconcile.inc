; app/reconcile.inc — reconcile pane status and last-activity against the
; JSONL file's mtime.
;
; Three bands of mtime age, each with a different rule. The middle band
; exists because Claude can spend minutes generating without flushing the
; JSONL (it appends the assistant turn at the end), so a strict "stale ⇒
; idle" rule produces false negatives during long thinking turns.
;
;   age ≤ FRESH_SECS  ............... force WORKING, bump ts to mtime.
;                                     (catches spinner false-negatives —
;                                      file is being written so the session
;                                      is definitely active.)
;
;   FRESH_SECS < age ≤ STALE_SECS ... trust the spinner classifier as-is,
;                                     ts_epoch unchanged.
;                                     (Claude is plausibly mid-turn.)
;
;   age > STALE_SECS  ............... flip WORKING → IDLE, ts_epoch unchanged.
;                                     (long-stuck pane: spinner glyph
;                                      caught in the buffer but the session
;                                      is dead.)

FRESH_SECS = 30
STALE_SECS = 1800                        ; 30 min


; reconcile_status_last(rdi=row_ptr, rsi=mtime_or_neg, rdx=now_secs)
;   rsi < 0 ⇒ no mtime available (no JSONL); leave row alone.
reconcile_status_last:
    test    rsi, rsi
    js      .ret                         ; no mtime → no change

    mov     rax, rdx
    sub     rax, rsi                     ; age = now - mtime
    cmp     rax, FRESH_SECS
    ja      .not_fresh

    ; fresh: force WORKING, bump ts_epoch up to mtime
    mov     qword [rdi + ROW_OFF_STATUS], STATUS_WORKING
    mov     rcx, qword [rdi + ROW_OFF_TS_EPOCH]
    cmp     rsi, rcx
    jbe     .ret
    mov     qword [rdi + ROW_OFF_TS_EPOCH], rsi
    ret

.not_fresh:
    cmp     rax, STALE_SECS
    jbe     .ret                         ; middle band: trust spinner

    ; very stale: only override WORKING → IDLE (can't be writing)
    mov     rax, qword [rdi + ROW_OFF_STATUS]
    cmp     rax, STATUS_WORKING
    jne     .ret
    mov     qword [rdi + ROW_OFF_STATUS], STATUS_IDLE
.ret:
    ret
