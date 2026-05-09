; fast_recon.asm — TUI dashboard for Claude Code tmux sessions, all in pure FASM.
;
; Columns: # | Session | Project | Status | Model | Context | Last Activity
; Hotkeys: j/k navigate, x SIGTERM selected pid, q/Ctrl-C quit.
;
; Per refresh tick, we run `tmux list-panes`, filter to claude rows, and for
; each row resolve cwd via /proc/{pid}/cwd, sessionId via the small claude
; session metafile, then locate + parse the JSONL for tokens / model / time.

format ELF64 executable 3
include 'common.inc'

OUT_BUF_BYTES = 65536
MAX_PANES     = 64
REFRESH_MS    = 200
KEY_TICK_MS   = 100

; JSONL tail window: read_file_tail mmaps at most this many bytes from the
; end of each session's JSONL — long sessions can be tens or hundreds of MB.
; We only care about the most recent assistant record, but in active sessions
; a single tool-result line can be megabytes, so the window has to comfortably
; outsize the largest plausible non-assistant line plus the assistant line
; that follows it.
JSONL_TAIL_BYTES = 1048576

; kill grace: SIGTERM, poll every KILL_GRACE_MS ms up to KILL_GRACE_TICKS
; times for the pid to exit. If still alive, send SIGKILL and wait
; KILL_REAP_MS for the kernel to reap before refreshing.
KILL_GRACE_MS    = 100
KILL_GRACE_TICKS = 5
KILL_REAP_MS     = 100

; sum of visible column widths (kept fixed via right-pad in each cell)
ROW_VISIBLE_COLS = 4 + 17 + 25 + 11 + 13 + 15 + 12

; row layout (ROW_OFF_*, ROW_BYTES) lives in app/row_layout.inc, pulled in
; via lib.inc at the bottom of this file.

segment readable executable
entry $
    ; --- mmap big buffers (out_buf, row_data, env, dent, cps_out, …) ---
    call    arena_init
    test    rax, rax
    js      .die_noterm

    ; (arena_init populates the runtime arena pointers _iw_dir_buf_ptr /
    ;  _iw_full_buf_ptr / _gi_buf_ptr / _gi_argv_ptr.)

    ; cache terminal width for row-padding (highlight bg fills)
    call    term_get_cols
    mov     dword [term_cols], eax

    ; render the static column-header line into hdr_cols once
    arena_lea rdi, ARENA_OFF_HDR_COLS
    mov     esi, HDR_BUF_CAP
    call    build_header
    mov     qword [hdr_cols_len], rax

    ; --- terminal setup ---
    xor     edi, edi
    lea     rsi, [tsave]
    call    term_enter_raw
    test    rax, rax
    js      .die_noterm
    mov     byte [in_raw], 1
    call    term_alt_on
    call    term_cursor_hide

    call    refresh_panes
    call    redraw

.event_loop:
    mov     dword [tick_remaining], REFRESH_MS
.poll_loop:
    mov     eax, dword [tick_remaining]
    test    eax, eax
    jle     .do_refresh
    cmp     eax, KEY_TICK_MS
    jl      .small
    mov     ecx, KEY_TICK_MS
    jmp     .got_tick
.small:
    mov     ecx, eax
.got_tick:
    sub     dword [tick_remaining], ecx

    xor     edi, edi
    movsxd  rsi, ecx
    call    read_key
    cmp     rax, -1
    je      .poll_loop

    cmp     al, 'q'
    je      .quit
    cmp     al, 0x03
    je      .quit
    cmp     al, 'j'
    je      .key_down
    cmp     al, 'k'
    je      .key_up
    cmp     al, 'x'
    je      .key_kill
    jmp     .poll_loop

.key_down:
    mov     eax, dword [n_panes]
    test    eax, eax
    jz      .poll_loop
    mov     ecx, dword [sel]
    inc     ecx
    cmp     ecx, eax
    jl      .set_sel
    xor     ecx, ecx
    jmp     .set_sel
.key_up:
    mov     eax, dword [n_panes]
    test    eax, eax
    jz      .poll_loop
    mov     ecx, dword [sel]
    dec     ecx
    jns     .set_sel
    mov     ecx, eax
    dec     ecx
.set_sel:
    mov     dword [sel], ecx
    call    redraw
    jmp     .poll_loop

.key_kill:
    mov     eax, dword [n_panes]
    test    eax, eax
    jz      .poll_loop
    ; sel is a DISPLAY index — map through sort_order to real row.
    movsxd  rax, dword [sel]
    arena_lea r9, ARENA_OFF_SORT_ORDER
    movsxd  rax, dword [r9 + rax*4]
    imul    rax, PANE_REC_BYTES
    arena_lea rcx, ARENA_OFF_PANE_RECS
    mov     rdi, qword [rcx + rax + PANE_OFF_PID]
    mov     qword [kill_pid], rdi
    mov     esi, SIGTERM
    call    sys_kill

    ; poll up to KILL_GRACE_TICKS x KILL_GRACE_MS for the pid to exit, then
    ; escalate to SIGKILL. kill(pid, 0) returns 0 while the process is alive
    ; and -ESRCH (-3) once it's gone.
    mov     ebx, KILL_GRACE_TICKS
.kill_wait:
    test    ebx, ebx
    jz      .kill_escalate
    mov     edi, KILL_GRACE_MS
    call    sys_sleep_ms
    mov     rdi, qword [kill_pid]
    xor     esi, esi
    call    sys_kill
    test    rax, rax
    jnz     .kill_done               ; non-zero → gone (or unreachable)
    dec     ebx
    jmp     .kill_wait
.kill_escalate:
    mov     rdi, qword [kill_pid]
    mov     esi, SIGKILL
    call    sys_kill
    mov     edi, KILL_REAP_MS
    call    sys_sleep_ms
.kill_done:
    call    refresh_panes
    call    redraw
    jmp     .poll_loop

.do_refresh:
    call    refresh_panes
    call    redraw
    jmp     .event_loop

.quit:
    call    term_cursor_show
    call    term_alt_off
    cmp     byte [in_raw], 0
    je      .skip_restore
    xor     edi, edi
    lea     rsi, [tsave]
    call    term_restore
.skip_restore:
    xor     edi, edi
    mov     eax, SYS_exit
    syscall
.die_noterm:
    mov     edi, 2
    mov     eax, SYS_exit
    syscall

; claim_jsonl_path — record the stem (basename minus ".jsonl") of the
; just-resolved jsonl_path into claimed_buf, so the next pane's fallback
; lookup will skip this file. Bounded by MAX_PANES slots.
claim_jsonl_path:
    mov     eax, dword [claimed_n]
    cmp     eax, MAX_PANES
    jge     .full

    ; strlen(jsonl_path) -> r10
    arena_lea rdi, ARENA_OFF_JSONL_PATH
    call    strlen
    mov     r10, rax                 ; full path length

    ; find last '/'
    arena_lea rcx, ARENA_OFF_JSONL_PATH
    mov     r11, rcx
    add     r11, r10                 ; one-past-end
.find_slash:
    cmp     r11, rcx
    je      .full                    ; no slash found
    cmp     byte [r11 - 1], '/'
    je      .got_slash
    dec     r11
    jmp     .find_slash
.got_slash:
    ; stem = [r11, jsonl_path + r10 - 6)
    arena_lea rdx, ARENA_OFF_JSONL_PATH
    add     rdx, r10
    sub     rdx, 6                   ; drop ".jsonl"
    cmp     rdx, r11
    jbe     .full

    ; slot = claimed_buf + claimed_n*CLAIM_SLOT
    mov     eax, dword [claimed_n]
    imul    rax, CLAIM_SLOT
    arena_lea rdi, ARENA_OFF_CLAIMED_BUF
    add     rdi, rax
    ; zero the slot (so the trailing-NUL check in find_recent works)
    push    rdi
    push    rdx
    push    r11
    xor     esi, esi
    mov     edx, CLAIM_SLOT
    call    memset
    pop     r11
    pop     rdx
    pop     rdi

    mov     rsi, r11                 ; stem start
    mov     r9, rdx
    sub     r9, r11                  ; stem length
    cmp     r9, CLAIM_SLOT - 1
    jbe     .copy_ok
    mov     r9, CLAIM_SLOT - 1
.copy_ok:
    mov     rdx, r9
    call    memcpy
    inc     dword [claimed_n]
.full:
    ret

; -------------------------------------------------------------------
; refresh_panes — exec tmux, parse, then for each row enrich with cwd
; + JSONL-derived data into row_data[i].
; -------------------------------------------------------------------
refresh_panes:
    push    rbx
    push    rbp
    push    r12
    push    r13

    ; cache "now" for status-debounce + later relative-time formatting
    call    now_epoch_secs
    mov     qword [now_secs], rax

    lea     rdi, [path_tmux]
    lea     rsi, [argv_tmux]
    arena_lea rdx, ARENA_OFF_OUT_BUF
    mov     rcx, OUT_BUF_BYTES
    call    exec_capture
    test    rax, rax
    js      .empty
    mov     rbx, rax

    arena_lea rdi, ARENA_OFF_OUT_BUF
    mov     rsi, rbx
    arena_lea rdx, ARENA_OFF_PANE_RECS
    mov     ecx, MAX_PANES
    call    parse_claude_panes
    mov     dword [n_panes], eax

    ; tmux's pane_pid is the shell, not the claude command running in it.
    ; Descend each pane record's PID to the actual claude child so all
    ; downstream pid lookups (sessions, /proc, JSONL anchoring) hit the
    ; right process.
    xor     ecx, ecx
.descend_loop:
    cmp     ecx, eax
    jge     .descend_done
    mov     edx, ecx
    imul    edx, PANE_REC_BYTES
    push    rax
    push    rcx
    arena_lea r9, ARENA_OFF_PANE_RECS
    mov     rdi, qword [r9 + rdx + PANE_OFF_PID]
    call    pid_descend_to_claude
    pop     rcx
    mov     edx, ecx
    imul    edx, PANE_REC_BYTES
    arena_lea r9, ARENA_OFF_PANE_RECS
    mov     qword [r9 + rdx + PANE_OFF_PID], rax
    pop     rax
    inc     ecx
    jmp     .descend_loop
.descend_done:

    ; clamp sel
    mov     ecx, dword [sel]
    cmp     ecx, eax
    jl      .clamp_done
    test    eax, eax
    jz      .reset
    dec     eax
    mov     dword [sel], eax
    jmp     .clamp_done
.reset:
    mov     dword [sel], 0
.clamp_done:

    ; reset the claimed-uuid list (used as we resolve JSONL fallbacks below
    ; so two panes sharing a cwd don't collide on the same JSONL file).
    mov     dword [claimed_n], 0

    ; build alive_pids[] from pane_recs and let pcache drop entries for
    ; pids that no longer have a pane.
    xor     ecx, ecx
.alive_fill:
    cmp     ecx, dword [n_panes]
    jge     .alive_fill_done
    mov     rax, rcx
    imul    rax, PANE_REC_BYTES
    arena_lea r9, ARENA_OFF_PANE_RECS
    mov     rax, qword [r9 + rax + PANE_OFF_PID]
    arena_lea r9, ARENA_OFF_ALIVE_PIDS
    mov     dword [r9 + rcx*4], eax
    inc     ecx
    jmp     .alive_fill
.alive_fill_done:
    arena_lea rdi, ARENA_OFF_ALIVE_PIDS
    movsxd  rsi, dword [n_panes]
    call    pcache_evict_missing

    ; PRE-PASS: claim every cached pane's jsonl_path so cold misses below
    ; won't collide with a warm pane that hasn't been visited yet.
    xor     ebp, ebp
.pre_claim_loop:
    mov     ecx, dword [n_panes]
    cmp     ebp, ecx
    jge     .pre_claim_done
    mov     rax, rbp
    imul    rax, PANE_REC_BYTES
    arena_lea r13, ARENA_OFF_PANE_RECS
    add     r13, rax
    mov     rdi, qword [r13 + PANE_OFF_PID]
    call    pcache_lookup
    test    rax, rax
    jz      .pre_claim_next
    mov     r14, rax
    mov     rcx, qword [r14 + PCACHE_OFF_JPATH_LEN]
    test    rcx, rcx
    jz      .pre_claim_next
    push    rcx
    arena_lea rdi, ARENA_OFF_JSONL_PATH
    lea     rsi, [r14 + PCACHE_OFF_JPATH]
    mov     rdx, rcx
    call    memcpy
    pop     rcx
    arena_lea r9, ARENA_OFF_JSONL_PATH
    mov     byte [r9 + rcx], 0
    call    claim_jsonl_path
.pre_claim_next:
    inc     ebp
    jmp     .pre_claim_loop
.pre_claim_done:

    ; --- enrich every row ---
    xor     ebp, ebp
.enrich_loop:
    mov     ecx, dword [n_panes]
    cmp     ebp, ecx
    jge     .enrich_done

    ; row pointer
    mov     rax, rbp
    imul    rax, ROW_BYTES
    arena_lea r12, ARENA_OFF_ROW_DATA
    add     r12, rax
    ; pane record pointer
    mov     rax, rbp
    imul    rax, PANE_REC_BYTES
    arena_lea r13, ARENA_OFF_PANE_RECS
    add     r13, rax

    ; zero the row
    mov     rdi, r12
    xor     esi, esi
    mov     edx, ROW_BYTES
    call    memset

    ; pane status via tmux capture-pane, then debounce Working→Idle
    mov     rdi, qword [r13 + PANE_OFF_PANE_ID_PTR]
    mov     rsi, qword [r13 + PANE_OFF_PANE_ID_LEN]
    call    classify_pane_status
    mov     rsi, rax                 ; raw status
    mov     rdi, qword [r13 + PANE_OFF_PID]
    mov     rdx, qword [now_secs]
    call    status_with_hold
    mov     qword [r12 + ROW_OFF_STATUS], rax

    ; --- resolve stable identity (cwd / git / uuid / jsonl_path) via cache ---
    ; cwd + git_info are populated once on cold miss and reused. The JSONL
    ; lookup is retried every tick whenever PCACHE_OFF_JPATH_LEN is still 0,
    ; because brand-new claude pids have no session file or JSONL yet.
    mov     rdi, qword [r13 + PANE_OFF_PID]
    call    pcache_lookup
    test    rax, rax
    jnz     .cache_warm
    ; cold miss: alloc + populate cwd/git, then fall into .resolve_jsonl
    mov     rdi, qword [r13 + PANE_OFF_PID]
    call    pcache_alloc
    mov     r14, rax

    mov     rdi, qword [r13 + PANE_OFF_PID]
    lea     rsi, [r14 + PCACHE_OFF_CWD]
    mov     edx, 256
    call    pid_cwd
    test    rcx, rcx
    jns     .cold_cwd_ok
    mov     qword [r14 + PCACHE_OFF_PID], 0
    jmp     .copy_to_row
.cold_cwd_ok:
    mov     qword [r14 + PCACHE_OFF_CWD_LEN], rax
    mov     byte [r14 + PCACHE_OFF_CWD + rax], 0

    lea     rdi, [r14 + PCACHE_OFF_CWD]
    lea     rsi, [r14 + PCACHE_OFF_GIT]
    call    git_project_info
    jmp     .resolve_jsonl

.cache_warm:
    mov     r14, rax
    ; if the cached slot already has a JSONL path, just stage it and
    ; skip the (expensive) discovery step.
    mov     rcx, qword [r14 + PCACHE_OFF_JPATH_LEN]
    test    rcx, rcx
    jz      .resolve_jsonl
    push    rcx
    arena_lea rdi, ARENA_OFF_JSONL_PATH
    lea     rsi, [r14 + PCACHE_OFF_JPATH]
    mov     rdx, rcx
    call    memcpy
    pop     rcx
    arena_lea r9, ARENA_OFF_JSONL_PATH
    mov     byte [r9 + rcx], 0
    jmp     .copy_to_row

.resolve_jsonl:
    ; need a cwd to do anything useful
    cmp     qword [r14 + PCACHE_OFF_CWD_LEN], 0
    je      .copy_to_row

    mov     rdi, qword [r13 + PANE_OFF_PID]
    lea     rsi, [r14 + PCACHE_OFF_UUID]
    call    pid_session_id
    test    rcx, rcx
    js      .resolve_fallback
    mov     qword [r14 + PCACHE_OFF_UUID_LEN], rax
    mov     r8, rax
    lea     rdi, [r14 + PCACHE_OFF_UUID]
    mov     rsi, r8
    arena_lea rdx, ARENA_OFF_JSONL_PATH
    mov     ecx, 1024
    call    find_jsonl_path
    test    r8, r8
    jns     .resolve_got
.resolve_fallback:
    ; Pid-anchored fallback: when {PID}.json is missing, match a JSONL
    ; whose earliest line timestamp lines up with the pid's wall-clock
    ; start time. We deliberately do NOT fall through to mtime-newest:
    ; that would attribute an unrelated sibling JSONL to the pane and
    ; show ghost data for new sessions. Better to leave Model/Context/
    ; Last blank than to display a wrong pairing.
    mov     rdi, qword [r13 + PANE_OFF_PID]
    call    pid_start_epoch
    test    rcx, rcx
    js      .copy_to_row
    lea     rdi, [r14 + PCACHE_OFF_CWD]
    mov     rsi, qword [r14 + PCACHE_OFF_CWD_LEN]
    arena_lea rdx, ARENA_OFF_JSONL_PATH
    mov     ecx, 1024
    mov     r8, rax                  ; pid_start_epoch
    call    find_jsonl_by_pid_start
    test    r8, r8
    js      .copy_to_row
.resolve_got:
    mov     qword [r14 + PCACHE_OFF_JPATH_LEN], rax
    push    rax
    lea     rdi, [r14 + PCACHE_OFF_JPATH]
    arena_lea rsi, ARENA_OFF_JSONL_PATH
    mov     rdx, rax
    call    memcpy
    pop     rax
    mov     byte [r14 + PCACHE_OFF_JPATH + rax], 0
    call    claim_jsonl_path

.copy_to_row:
    ; mirror cached fields into the per-tick row (cwd + git_info)
    lea     rdi, [r12 + ROW_OFF_CWD]
    lea     rsi, [r14 + PCACHE_OFF_CWD]
    mov     edx, 256
    call    memcpy
    mov     rax, qword [r14 + PCACHE_OFF_CWD_LEN]
    mov     qword [r12 + ROW_OFF_CWD_LEN], rax
    lea     rdi, [r12 + ROW_OFF_GIT]
    lea     rsi, [r14 + PCACHE_OFF_GIT]
    mov     edx, 256
    call    memcpy

    ; refresh JSONL each tick (tokens + timestamp change). Only the tail of
    ; the file is mapped — the JSONL grows unbounded and the parser only
    ; cares about the last assistant line.
    mov     rcx, qword [r14 + PCACHE_OFF_JPATH_LEN]
    test    rcx, rcx
    jz      .skip_jsonl
    arena_lea rdi, ARENA_OFF_JSONL_PATH
    mov     rsi, JSONL_TAIL_BYTES
    call    read_file_tail
    test    rcx, rcx
    js      .skip_jsonl
    test    rax, rax
    jz      .skip_jsonl
    test    rdx, rdx
    jz      .skip_jsonl

    ; if mmap started mid-file, skip the (likely partial) first line
    test    r8, r8
    jz      .tail_no_skip
    push    rax
    push    rdx
    mov     rsi, rax                 ; cursor
    add     rdx, rax                 ; end ptr
.tail_skip:
    cmp     rsi, rdx
    jae     .tail_no_newline
    cmp     byte [rsi], 0x0A
    je      .tail_after_nl
    inc     rsi
    jmp     .tail_skip
.tail_after_nl:
    inc     rsi                      ; past the '\n'
    pop     rdx                      ; old len
    pop     rax                      ; old ptr
    push    rax                      ; preserve original ptr/len for release
    push    rdx
    mov     r9, rsi                  ; new cursor
    sub     r9, rax                  ; bytes skipped
    sub     rdx, r9                  ; remaining len
    mov     rax, rsi                 ; ptr → first complete line
    jmp     .tail_parse
.tail_no_newline:
    pop     rdx
    pop     rax
    jmp     .tail_release            ; nothing complete to parse
.tail_no_skip:
    push    rax                      ; preserve original ptr/len for release
    push    rdx
.tail_parse:
    push    rax                      ; ptr (parse arg)
    push    rdx                      ; len (parse arg)
    mov     rdi, rax
    mov     rsi, rdx
    lea     rdx, [r12 + ROW_OFF_INFO]
    call    parse_jsonl_buf
    pop     rdx
    pop     rdi
.tail_release:
    pop     rsi                      ; original len
    pop     rdi                      ; original ptr
    call    release_file

.skip_jsonl:
    ; compute ts_epoch from INFO_OFF_TS_BUF if present
    mov     rax, qword [r12 + ROW_OFF_INFO + INFO_OFF_TS_LEN]
    test    rax, rax
    jz      .ts_zero
    lea     rdi, [r12 + ROW_OFF_INFO + INFO_OFF_TS_BUF]
    mov     rsi, rax
    call    iso8601_to_epoch
    mov     qword [r12 + ROW_OFF_TS_EPOCH], rax
    jmp     .ts_after_parse
.ts_zero:
    mov     qword [r12 + ROW_OFF_TS_EPOCH], 0
.ts_after_parse:

    ; Reconcile Status and ts_epoch against the JSONL file's mtime — the
    ; authoritative "is the session writing" signal. mtime stale + spinner
    ; says Working ⇒ flip to Idle (false-positive correction).
    mov     rcx, qword [r14 + PCACHE_OFF_JPATH_LEN]
    test    rcx, rcx
    jz      .skip_reconcile
    arena_lea rdi, ARENA_OFF_JSONL_PATH
    call    path_mtime
    test    rcx, rcx
    js      .skip_reconcile
    mov     rsi, rax                     ; mtime
    jmp     .do_reconcile
.skip_reconcile:
    mov     rsi, -1
.do_reconcile:
    mov     rdi, r12
    mov     rdx, qword [now_secs]
    call    reconcile_status_last

.ts_done:
    inc     ebp
    jmp     .enrich_loop

.enrich_done:
    ; build sort_keys + sort_order from per-row ts_epoch
    arena_lea rdi, ARENA_OFF_ROW_DATA
    movsxd  rsi, dword [n_panes]
    arena_lea rdx, ARENA_OFF_SORT_ORDER
    arena_lea rcx, ARENA_OFF_SORT_KEYS
    call    compute_pane_order

    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

.empty:
    mov     dword [n_panes], 0
    mov     dword [sel], 0
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

; -------------------------------------------------------------------
; redraw — paint the table.
; -------------------------------------------------------------------
redraw:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    call    term_clear

    ; Column header
    mov     edi, 1
    mov     esi, 1
    call    term_move
    call    term_fg_turquoise
    arena_lea rdi, ARENA_OFF_HDR_COLS
    mov     rsi, qword [hdr_cols_len]
    call    term_write_str
    call    term_fg_default

    ; Empty?
    mov     eax, dword [n_panes]
    test    eax, eax
    jnz     .rows
    mov     edi, 3
    mov     esi, 3
    call    term_move
    lea     rdi, [msg_empty]
    mov     rsi, msg_empty_len
    call    term_write_str
    jmp     .footer

.rows:
    ; current epoch
    call    now_epoch_secs
    mov     qword [now_secs], rax

    xor     ebp, ebp                 ; idx
.row_loop:
    cmp     ebp, dword [n_panes]
    jge     .footer

    mov     rax, rbp
    add     rax, 2
    mov     rdi, rax
    mov     esi, 1
    call    term_move

    cmp     ebp, dword [sel]
    jne     .draw
    call    term_select_bg_on
.draw:
    call    draw_row
    ; pad to terminal edge — for selected rows this extends the highlight,
    ; for unselected rows the spaces are invisible.
    mov     eax, dword [term_cols]
    sub     eax, ROW_VISIBLE_COLS
    jle     .reset_bg
    cmp     eax, 256
    jbe     .pad_ok
    mov     eax, 256
.pad_ok:
    mov     edi, 1
    lea     rsi, [pad_sp_long]
    movsxd  rdx, eax
    call    sys_write
.reset_bg:
    cmp     ebp, dword [sel]
    jne     .row_done
    call    term_select_bg_off
.row_done:
    inc     ebp
    jmp     .row_loop

.footer:
    ; place hint one blank line under the rows; when empty leave room for
    ; the "(no claude panes)" message at row 4.
    mov     eax, dword [n_panes]
    test    eax, eax
    jnz     .footer_have_rows
    mov     eax, 1                   ; pretend 1 row so footer = 4
.footer_have_rows:
    add     eax, 3
    cdqe
    mov     rdi, rax
    mov     esi, 1
    call    term_move
    lea     rdi, [hint]
    mov     rsi, hint_len
    call    term_write_str

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

; -------------------------------------------------------------------
; draw_row — render one row (called with rbp = index, cursor positioned).
;   Uses pane_recs[rbp] and row_data[rbp].
;   Columns (left-to-right, with 1-space gutters):
;     # (3) | Session (16) | Project (24) | Status (10) | Model (12) |
;     Context (14) | Last Activity (12)
; -------------------------------------------------------------------
draw_row:
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15

    ; resolve display→real via sort_order; rbp stays as the display index
    arena_lea r9, ARENA_OFF_SORT_ORDER
    movsxd  rbx, dword [r9 + rbp*4]
    mov     rax, rbx
    imul    rax, ROW_BYTES
    arena_lea r14, ARENA_OFF_ROW_DATA
    add     r14, rax
    mov     rax, rbx
    imul    rax, PANE_REC_BYTES
    arena_lea r15, ARENA_OFF_PANE_RECS
    add     r15, rax

    ; --- # (3 chars right-padded) ---
    arena_lea rdi, ARENA_OFF_SCRATCH
    mov     esi, ebp
    inc     rsi                      ; 1-based
    call    format_u64
    mov     r12, rax                 ; n digits
    mov     edi, 1
    arena_lea rsi, ARENA_OFF_SCRATCH
    mov     rdx, r12
    call    sys_write
    mov     ecx, 4
    sub     rcx, r12
    jle     .col_session
    mov     edi, 1
    lea     rsi, [pad_sp]
    mov     rdx, rcx
    call    sys_write

.col_session:
    ; truncate to 16
    mov     r12, qword [r15 + PANE_OFF_NAME_LEN]
    cmp     r12, 16
    jbe     .sn_ok
    mov     r12, 16
.sn_ok:
    mov     edi, 1
    mov     rsi, qword [r15 + PANE_OFF_NAME_PTR]
    mov     rdx, r12
    call    sys_write
    mov     ecx, 17
    sub     rcx, r12                 ; pad to 16+1 gutter
    jle     .col_project
    mov     edi, 1
    lea     rsi, [pad_sp]
    mov     rdx, rcx
    call    sys_write

.col_project:
    ; renders "{repo}::{dir}::{branch}" with ANSI color, padded to 25 visible.
    ; If repo unknown (cwd not in a git repo) fall back to cwd basename.
    mov     r13, qword [r14 + ROW_OFF_GIT + GIT_OFF_REPO_LEN]
    test    r13, r13
    jnz     .have_git

    ; no git info — emit basename(cwd)
    mov     r12, qword [r14 + ROW_OFF_CWD_LEN]
    test    r12, r12
    jz      .proj_dash
    lea     rdi, [r14 + ROW_OFF_CWD]
    mov     rsi, r12
    call    _basename
    mov     r10, rdx                 ; bn_len
    cmp     r10, 24
    jbe     .pn_ok
    mov     r10, 24
.pn_ok:
    push    r10
    mov     rsi, rax
    mov     edi, 1
    mov     rdx, r10
    call    sys_write
    pop     r10
    mov     rcx, 25
    sub     rcx, r10
    jle     .col_status
    mov     edi, 1
    lea     rsi, [pad_sp]
    mov     rdx, rcx
    call    sys_write
    jmp     .col_status

.proj_dash:
    mov     edi, 1
    lea     rsi, [pad_dash]
    mov     edx, 25
    call    sys_write
    jmp     .col_status

.have_git:
    ; visible width tracker in r10
    xor     r10, r10

    ; repo name
    mov     edi, 1
    lea     rsi, [r14 + ROW_OFF_GIT + GIT_OFF_REPO_BUF]
    mov     rdx, r13
    call    sys_write
    add     r10, r13

    ; "::"
    mov     rdx, qword [r14 + ROW_OFF_GIT + GIT_OFF_DIR_LEN]
    test    rdx, rdx
    jz      .git_after_dir
    mov     edi, 1
    lea     rsi, [_proj_dimcolon]
    mov     rdx, _proj_dimcolon_len
    call    sys_write
    add     r10, 2
    ; dir in cyan
    mov     edi, 1
    lea     rsi, [_proj_cyan]
    mov     rdx, _proj_cyan_len
    call    sys_write
    mov     rdx, qword [r14 + ROW_OFF_GIT + GIT_OFF_DIR_LEN]
    mov     edi, 1
    lea     rsi, [r14 + ROW_OFF_GIT + GIT_OFF_DIR_BUF]
    push    rdx
    call    sys_write
    pop     rdx
    add     r10, rdx
    mov     edi, 1
    lea     rsi, [_proj_reset]
    mov     rdx, _proj_reset_len
    call    sys_write
.git_after_dir:

    ; "::{branch}" in green if branch known
    mov     rdx, qword [r14 + ROW_OFF_GIT + GIT_OFF_BR_LEN]
    test    rdx, rdx
    jz      .git_after_branch
    mov     edi, 1
    lea     rsi, [_proj_dimcolon]
    mov     rdx, _proj_dimcolon_len
    call    sys_write
    add     r10, 2
    mov     edi, 1
    lea     rsi, [_proj_green]
    mov     rdx, _proj_green_len
    call    sys_write
    mov     rdx, qword [r14 + ROW_OFF_GIT + GIT_OFF_BR_LEN]
    push    rdx
    mov     edi, 1
    lea     rsi, [r14 + ROW_OFF_GIT + GIT_OFF_BR_BUF]
    call    sys_write
    pop     rdx
    add     r10, rdx
    mov     edi, 1
    lea     rsi, [_proj_reset]
    mov     rdx, _proj_reset_len
    call    sys_write
.git_after_branch:

    ; pad to visible 25
    mov     rcx, 25
    sub     rcx, r10
    jle     .col_status
    mov     edi, 1
    lea     rsi, [pad_sp]
    mov     rdx, rcx
    call    sys_write

.col_status:
    ; "<color>● Label<reset>" padded to 11 chars
    mov     rax, qword [r14 + ROW_OFF_STATUS]
    cmp     rax, STATUS_WORKING
    je      .stat_working
    cmp     rax, STATUS_INPUT
    je      .stat_input
    cmp     rax, STATUS_NEW
    je      .stat_new
    ; idle
    mov     edi, 1
    lea     rsi, [_st_idle_str]
    mov     rdx, _st_idle_len
    call    sys_write
    jmp     .col_model
.stat_working:
    mov     edi, 1
    lea     rsi, [_st_working_str]
    mov     rdx, _st_working_len
    call    sys_write
    jmp     .col_model
.stat_input:
    mov     edi, 1
    lea     rsi, [_st_input_str]
    mov     rdx, _st_input_len
    call    sys_write
    jmp     .col_model
.stat_new:
    mov     edi, 1
    lea     rsi, [_st_new_str]
    mov     rdx, _st_new_len
    call    sys_write

.col_model:
    ; render model display via lookup
    mov     r12, qword [r14 + ROW_OFF_INFO + INFO_OFF_MODEL_LEN]
    test    r12, r12
    jz      .model_dash
    lea     rdi, [r14 + ROW_OFF_INFO + INFO_OFF_MODEL_BUF]
    mov     rsi, r12
    arena_lea rdx, ARENA_OFF_SCRATCH
    mov     ecx, 32
    call    model_display_name
    mov     r12, rax
    cmp     r12, 12
    jbe     .mn_ok
    mov     r12, 12
.mn_ok:
    mov     edi, 1
    arena_lea rsi, ARENA_OFF_SCRATCH
    mov     rdx, r12
    call    sys_write
    mov     ecx, 13
    sub     rcx, r12
    jle     .col_context
    mov     edi, 1
    lea     rsi, [pad_sp]
    mov     rdx, rcx
    call    sys_write
    jmp     .col_context
.model_dash:
    mov     edi, 1
    lea     rsi, [pad_dash]
    mov     edx, 13
    call    sys_write

.col_context:
    ; tokens = input + output; window = lookup(model_id)
    mov     rax, qword [r14 + ROW_OFF_INFO + INFO_OFF_INPUT]
    add     rax, qword [r14 + ROW_OFF_INFO + INFO_OFF_OUTPUT]
    mov     r12, rax                 ; used
    mov     r13, qword [r14 + ROW_OFF_INFO + INFO_OFF_MODEL_LEN]
    test    r13, r13
    jz      .ctx_default_window
    lea     rdi, [r14 + ROW_OFF_INFO + INFO_OFF_MODEL_BUF]
    mov     rsi, r13
    call    model_context_window
    mov     r13, rax
    jmp     .ctx_have_window
.ctx_default_window:
    mov     r13, 200000
.ctx_have_window:
    test    r12, r12
    jnz     .ctx_render
    test    qword [r14 + ROW_OFF_INFO + INFO_OFF_TS_LEN], -1
    jz      .ctx_dash
.ctx_render:
    arena_lea rdi, ARENA_OFF_SCRATCH
    mov     esi, 32
    mov     rdx, r12
    mov     rcx, r13
    call    format_tokens
    mov     r12, rax
    cmp     r12, 14
    jbe     .ct_ok
    mov     r12, 14
.ct_ok:
    mov     edi, 1
    arena_lea rsi, ARENA_OFF_SCRATCH
    mov     rdx, r12
    call    sys_write
    mov     ecx, 15
    sub     rcx, r12
    jle     .col_activity
    mov     edi, 1
    lea     rsi, [pad_sp]
    mov     rdx, rcx
    call    sys_write
    jmp     .col_activity
.ctx_dash:
    mov     edi, 1
    lea     rsi, [pad_dash]
    mov     edx, 15
    call    sys_write

.col_activity:
    ; Last reads ROW_OFF_TS_EPOCH (the same value the sort key uses) so
    ; the column and the row order can never disagree.
    mov     r13, qword [r14 + ROW_OFF_TS_EPOCH]
    test    r13, r13
    jz      .act_dash
    arena_lea rdi, ARENA_OFF_SCRATCH
    mov     esi, 16
    mov     rdx, qword [now_secs]
    mov     rcx, r13
    call    format_relative
    mov     r12, rax
    cmp     r12, 12
    jbe     .act_ok
    mov     r12, 12
.act_ok:
    mov     edi, 1
    arena_lea rsi, ARENA_OFF_SCRATCH
    mov     rdx, r12
    call    sys_write
    mov     ecx, 12
    sub     rcx, r12
    jle     .draw_done
    mov     edi, 1
    lea     rsi, [pad_sp]
    mov     rdx, rcx
    call    sys_write
    jmp     .draw_done
.act_dash:
    mov     edi, 1
    lea     rsi, [pad_sp]            ; just blanks (12 cols)
    mov     edx, 12
    call    sys_write
.draw_done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret

include 'lib.inc'

segment readable
path_tmux  db '/usr/bin/tmux', 0
arg_lp     db 'list-panes', 0
arg_a      db '-a', 0
arg_F      db '-F', 0
arg_fmt    db '#{pane_id}|#{session_name}|#{pane_pid}|#{pane_current_command}', 0
argv_tmux  dq path_tmux, arg_lp, arg_a, arg_F, arg_fmt, 0

msg_empty  db '(no claude panes)'
msg_empty_len = $ - msg_empty

hint       db ' j/k move  x kill  q quit'
hint_len   = $ - hint

pad_sp     db '                                '   ; 32 spaces
pad_dash   db '-                                '   ; dash + 31 spaces
pad_sp_long:
    times 256 db ' '
pad_sp_long_end:

; Status strings: ANSI color + "● <label>" + reset + spaces (11 visible cols).
; UTF-8 bullet "●" = E2 97 8F.
; Status strings: ANSI fg-color + dot + label + fg-reset (39m, NOT 0m, so we
; don't kill the row's bg highlight). Padded to 11 visible cols.
_st_working_str db 0x1B,'[32m',0xE2,0x97,0x8F,' Working',0x1B,'[39m  '
_st_working_len = $ - _st_working_str
_st_input_str   db 0x1B,'[33m',0xE2,0x97,0x8F,' Input',0x1B,'[39m    '
_st_input_len   = $ - _st_input_str
_st_new_str     db 0x1B,'[34m',0xE2,0x97,0x8F,' New',0x1B,'[39m      '
_st_new_len     = $ - _st_new_str
_st_idle_str    db 0x1B,'[90m',0xE2,0x97,0x8F,' Idle',0x1B,'[39m     '
_st_idle_len    = $ - _st_idle_str

_proj_dimcolon  db 0x1B,'[90m::',0x1B,'[39m'
_proj_dimcolon_len = $ - _proj_dimcolon
_proj_cyan      db 0x1B,'[36m'
_proj_cyan_len  = $ - _proj_cyan
_proj_green     db 0x1B,'[32m'
_proj_green_len = $ - _proj_green
_proj_reset     db 0x1B,'[39m'
_proj_reset_len = $ - _proj_reset

segment readable writeable
align 8
; All large reservations live in the mmap'd arena (see os/arena.inc):
;   pane_recs / claimed_buf / jsonl_path / sort_keys / sort_order /
;   alive_pids / scratch.  Only small bookkeeping qwords/dwords stay
;   on-disk here.
term_cols     rd 1
claimed_n     rd 1
kill_pid      rq 1
n_panes       rd 1
sel           rd 1
tick_remaining rd 1
in_raw        rb 1
align 8
tsave         rb TERMIOS_BYTES
now_secs      rq 1
; Note: data and state from all .inc files are consolidated and
; included near the top of lib.inc.
