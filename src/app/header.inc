; app/header.inc — column header string and column-width constants.
;
; Tests and draw_row both consult these so that the on-screen labels stay
; aligned with the data cells beneath them.

; field widths exactly match the right-pads emitted by draw_row in fast_recon.asm
COL_NUM_W      = 4
COL_SESSION_W  = 17
COL_PROJECT_W  = 25
COL_STATUS_W   = 11
COL_MODEL_W    = 13
COL_CONTEXT_W  = 15
COL_LAST_W     = 12

COL_NUM_OFF      = 0
COL_SESSION_OFF  = COL_NUM_OFF + COL_NUM_W
COL_PROJECT_OFF  = COL_SESSION_OFF + COL_SESSION_W
COL_STATUS_OFF   = COL_PROJECT_OFF + COL_PROJECT_W
COL_MODEL_OFF    = COL_STATUS_OFF + COL_STATUS_W
COL_CONTEXT_OFF  = COL_MODEL_OFF + COL_MODEL_W
COL_LAST_OFF     = COL_CONTEXT_OFF + COL_CONTEXT_W
COL_TOTAL_W      = COL_LAST_OFF + COL_LAST_W

segment readable
hdr_cols:
    db '#   '                              ;  4 — # field
    db 'Session          '                 ; 17 — Session field
    db 'Project                  '         ; 25 — Project field
    db 'Status     '                       ; 11 — Status field
    db 'Model        '                     ; 13 — Model field
    db 'Context        '                   ; 15 — Context field
    db 'Last        '                      ; 12 — Last activity field
hdr_cols_len = $ - hdr_cols
