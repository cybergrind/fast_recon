; app/cache_layout.inc — per-pid stable-fields cache slot layout.
;
; The cache holds the bits we don't want to recompute every refresh tick:
;   - cwd (readlink /proc/{pid}/cwd)
;   - git_info (two `git` subprocess calls)
;   - JSONL session id and JSONL absolute path (directory walk)
;
; Status, JSONL contents, and timestamps are NOT cached — they actually
; change tick-to-tick.

PCACHE_OFF_PID       = 0       ; q — 0 means free slot
PCACHE_OFF_CWD_LEN   = 8       ; q
PCACHE_OFF_CWD       = 16      ; 256 — NUL-terminated within
PCACHE_OFF_GIT       = 272     ; 256 — mirrors ROW_OFF_GIT layout
PCACHE_OFF_UUID_LEN  = 528     ; q
PCACHE_OFF_UUID      = 536     ; 64
PCACHE_OFF_JPATH_LEN = 600     ; q
PCACHE_OFF_JPATH     = 608     ; 1024
PCACHE_BYTES         = 1632
