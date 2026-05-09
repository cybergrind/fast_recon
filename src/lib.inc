; lib.inc — aggregate of all fast_recon library modules.
; Tests and the main binary `include` this once after `format ELF64 executable 3`.

; --- layouts: pure constants the rest of the code depends on. Loaded first
;     so arena.inc can size its slices using ROW_BYTES / PCACHE_BYTES. ---
include 'app/row_layout.inc'
include 'app/cache_layout.inc'

; --- core: pure primitives, no OS ---
include 'core/str.inc'
include 'core/num.inc'
include 'core/json.inc'
include 'core/sort.inc'

; --- os: syscall wrappers and OS resources ---
include 'os/sys.inc'
include 'os/io.inc'
include 'os/proc.inc'
include 'os/term.inc'
include 'os/proc_paths.inc'
include 'os/arena.inc'
include 'os/env.inc'

; --- app: domain logic (tmux, claude session files) ---
include 'app/panes.inc'
include 'app/sessions.inc'
include 'app/jsonl_locate.inc'
include 'app/jsonl_in_cwd.inc'
include 'app/model.inc'
include 'app/jsonl_parse.inc'
include 'app/timefmt.inc'
include 'app/format_tokens.inc'
include 'app/status.inc'
include 'app/status_hold.inc'
include 'app/git_info.inc'
include 'app/header.inc'
include 'app/order.inc'
include 'app/pane_cache.inc'
include 'app/jsonl_first_ts.inc'
include 'app/reconcile.inc'

; All read-only and writeable data from every .inc file is consolidated
; here so FASM emits one program header per attribute set instead of
; one per `segment` directive.
segment readable
include '_data.inc'

segment readable writeable
include '_state.inc'
