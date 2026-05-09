; _state.inc — writeable state extracted from every .inc file.
; Included from fast_recon.asm inside `segment readable writeable`.

; --- from app/git_info.inc ---
align 8
; _gi_argv (64 B) and _gi_buf (4 KB) live in the arena; these pointers
; are populated at startup.
_gi_argv_ptr rq 1
_gi_buf_ptr  rq 1

; --- from app/header.inc ---
align 8
; hdr_cols lives in arena (ARENA_OFF_HDR_COLS); this just stores the length.
hdr_cols_len rq 1

; --- from app/jsonl_in_cwd.inc ---
align 8
; _iw_dir_buf and _iw_full_buf live in arena (ARENA_OFF_IW_DIR_BUF /
; ARENA_OFF_IW_FULL_BUF). _iw_dir_buf_ptr / _iw_full_buf_ptr below hold
; their runtime addresses.
_iw_dir_len      rq 1
_iw_dent_size    rq 1
_iw_dent_cur     rq 1
; statbuf, best_name, pa_best_name now in arena (ARENA_OFF_IW_*).
_iw_best_mtime    rq 1
_iw_best_mtime_ns rq 1
_iw_best_name_len rq 1
_iw_claimed_buf   rq 1
_iw_claimed_n     rq 1
_iw_pid_start         rq 1
_iw_pa_best_delta     rq 1
_iw_pa_best_name_len  rq 1
_iw_pa_name_ptr       rq 1
_iw_pa_name_len       rq 1
; Pointers populated at startup (arena_base + ARENA_OFF_IW_*_BUF) so we
; don't carry 2 KB of zero-fill statics on disk.
_iw_dir_buf_ptr   rq 1
_iw_full_buf_ptr  rq 1
_dbg_lf_iw        db 0x0A
_dbg_sp_iw        db ' '

; --- from app/jsonl_locate.inc ---
align 8
_dent_size   rq 1
_dent_cur    rq 1
_cand_name   rq 1
_cand_pos    rq 1

; --- from app/jsonl_parse.inc ---
align 8
_msg_start    rq 1
_msg_end      rq 1
_usage_start  rq 1
_usage_end    rq 1

; --- from app/panes.inc ---
align 8
_pi_pane_id_ptr rq 1
_pi_pane_id_len rq 1
_pi_name_ptr    rq 1
_pi_name_len    rq 1
_pi_pid_ptr     rq 1
_pi_pid_len     rq 1
_pi_cmd_ptr     rq 1
_pi_pid_val     rq 1

; --- from app/status.inc ---
align 8
_cps_id_buf      rb 32
_cps_argv        rq 6
_psf_last_start  rq 1
_psf_last_end    rq 1

; --- from app/status_hold.inc ---
align 8
_hold_pid rq HOLD_ENTRIES         ; pid; 0 means "free slot"
_hold_ts  rq HOLD_ENTRIES         ; epoch seconds of last observed Working

; --- from core/num.inc ---
; 24-byte scratch buffer used by format_u64 (max u64 is 20 digits).
align 16
_fmt_tmp:     db 24 dup (?)
_fmt_tmp_end:

; --- from os/arena.inc ---
align 8
arena_base rq 1

; --- from os/env.inc ---
_env_len     rq 1
_env_loaded  rd 1

; --- from os/term.inc ---
_term_raw_buf  rb TERMIOS_BYTES
_move_buf      rb 32
