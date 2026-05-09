; lib.inc — aggregate of all fast_recon library modules.
; Tests and the main binary `include` this once after `format ELF64 executable 3`.

; --- core: pure primitives, no OS ---
include 'core/str.inc'
include 'core/num.inc'
include 'core/json.inc'

; --- os: syscall wrappers and OS resources ---
include 'os/sys.inc'
include 'os/io.inc'
include 'os/proc.inc'
include 'os/term.inc'
include 'os/proc_paths.inc'
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
include 'app/git_info.inc'
