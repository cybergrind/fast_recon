; app/row_layout.inc — per-row enriched record layout (row_data slot).
;
; Each row is ROW_BYTES wide and lives in the mmap'd arena (ARENA_OFF_ROW_DATA).
; Refresh code populates the fields below; render code reads them through the
; same offsets.

ROW_OFF_CWD       = 0       ; 256 bytes of cwd (NUL-terminated)
ROW_OFF_CWD_LEN   = 256
ROW_OFF_INFO      = 264     ; 128 bytes of jsonl_info
ROW_OFF_STATUS    = 392
ROW_OFF_GIT       = 400     ; 256 bytes of git_info
ROW_OFF_TS_EPOCH  = 656     ; q — parsed from INFO_OFF_TS_BUF; sort key
ROW_BYTES         = 664
