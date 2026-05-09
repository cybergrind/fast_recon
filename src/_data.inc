; _data.inc — read-only data extracted from every .inc file.
; Included from fast_recon.asm inside `segment readable`.

; --- from app/git_info.inc ---
_gi_path_git     db '/usr/bin/git', 0
_gi_arg_C        db '-C', 0
_gi_arg_revparse db 'rev-parse', 0
_gi_arg_toplevel db '--show-toplevel', 0
_gi_arg_abbrev   db '--abbrev-ref', 0
_gi_arg_head     db 'HEAD', 0

_GI_BUF_BYTES = 4096


; --- from app/header.inc ---
_lbl_num      db '#'
_lbl_num_len  = $ - _lbl_num
_lbl_session  db 'Session'
_lbl_session_len = $ - _lbl_session
_lbl_project  db 'Project'
_lbl_project_len = $ - _lbl_project
_lbl_status   db 'Status'
_lbl_status_len = $ - _lbl_status
_lbl_model    db 'Model'
_lbl_model_len = $ - _lbl_model
_lbl_context  db 'Context'
_lbl_context_len = $ - _lbl_context
_lbl_last     db 'Last'
_lbl_last_len = $ - _lbl_last

align 8
_col_table:
    dq _lbl_num,     _lbl_num_len,     COL_NUM_W
    dq _lbl_session, _lbl_session_len, COL_SESSION_W
    dq _lbl_project, _lbl_project_len, COL_PROJECT_W
    dq _lbl_status,  _lbl_status_len,  COL_STATUS_W
    dq _lbl_model,   _lbl_model_len,   COL_MODEL_W
    dq _lbl_context, _lbl_context_len, COL_CONTEXT_W
    dq _lbl_last,    _lbl_last_len,    COL_LAST_W
_col_table_n = 7


; --- from app/jsonl_first_ts.inc ---
_jft_key_ts     db 'timestamp'
_jft_key_ts_len = $ - _jft_key_ts

; --- from app/jsonl_in_cwd.inc ---
_seg_proj2      db '/.claude/projects/'
_seg_proj2_len  = $ - _seg_proj2
_seg_jsonl2     db '.jsonl'
_seg_jsonl2_len = 6


; --- from app/jsonl_locate.inc ---
_seg_proj    db '/.claude/projects/'
_seg_proj_len = $ - _seg_proj
_seg_jsonl   db '.jsonl'
_seg_jsonl_len = $ - _seg_jsonl


; --- from app/jsonl_parse.inc ---
_skey_type             db 'type'
_skey_type_len         = $ - _skey_type
_skey_message          db 'message'
_skey_message_len      = $ - _skey_message
_skey_model            db 'model'
_skey_model_len        = $ - _skey_model
_skey_usage            db 'usage'
_skey_usage_len        = $ - _skey_usage
_skey_timestamp        db 'timestamp'
_skey_timestamp_len    = $ - _skey_timestamp
_skey_input            db 'input_tokens'
_skey_input_len        = $ - _skey_input
_skey_output           db 'output_tokens'
_skey_output_len       = $ - _skey_output
_skey_cache_create     db 'cache_creation_input_tokens'
_skey_cache_create_len = $ - _skey_cache_create
_skey_cache_read       db 'cache_read_input_tokens'
_skey_cache_read_len   = $ - _skey_cache_read
_str_assistant         db 'assistant'
_str_assistant_len     = $ - _str_assistant
_str_user              db 'user'
_str_user_len          = $ - _str_user
_str_system            db 'system'
_str_system_len        = $ - _str_system
_str_synthetic         db '<synthetic>'
_str_synthetic_len     = $ - _str_synthetic


; --- from app/model.inc ---

_m_opus_47       db 'claude-opus-4-7'
_m_opus_47_len   = $ - _m_opus_47
_d_opus_47       db 'Opus 4.7'
_d_opus_47_len   = $ - _d_opus_47

_m_opus_46       db 'claude-opus-4-6'
_m_opus_46_len   = $ - _m_opus_46
_d_opus_46       db 'Opus 4.6'
_d_opus_46_len   = $ - _d_opus_46

_m_sonnet_46     db 'claude-sonnet-4-6'
_m_sonnet_46_len = $ - _m_sonnet_46
_d_sonnet_46     db 'Sonnet 4.6'
_d_sonnet_46_len = $ - _d_sonnet_46

_m_haiku_45      db 'claude-haiku-4-5-20251001'
_m_haiku_45_len  = $ - _m_haiku_45
_d_haiku_45      db 'Haiku 4.5'
_d_haiku_45_len  = $ - _d_haiku_45

_m_sonnet_45     db 'claude-sonnet-4-5-20250514'
_m_sonnet_45_len = $ - _m_sonnet_45
_d_sonnet_45     db 'Sonnet 4.5'
_d_sonnet_45_len = $ - _d_sonnet_45

_m_opus_4        db 'claude-opus-4-20250514'
_m_opus_4_len    = $ - _m_opus_4
_d_opus_4        db 'Opus 4'
_d_opus_4_len    = $ - _d_opus_4

_m_sonnet_4      db 'claude-sonnet-4-20250514'
_m_sonnet_4_len  = $ - _m_sonnet_4
_d_sonnet_4      db 'Sonnet 4'
_d_sonnet_4_len  = $ - _d_sonnet_4

; record: id_ptr q, id_len q, disp_ptr q, disp_len q, window q  -> 40 bytes
align 8
_model_table:
    dq _m_opus_47,    _m_opus_47_len,    _d_opus_47,    _d_opus_47_len,    1000000
    dq _m_opus_46,    _m_opus_46_len,    _d_opus_46,    _d_opus_46_len,    1000000
    dq _m_sonnet_46,  _m_sonnet_46_len,  _d_sonnet_46,  _d_sonnet_46_len,  200000
    dq _m_haiku_45,   _m_haiku_45_len,   _d_haiku_45,   _d_haiku_45_len,   200000
    dq _m_sonnet_45,  _m_sonnet_45_len,  _d_sonnet_45,  _d_sonnet_45_len,  200000
    dq _m_opus_4,     _m_opus_4_len,     _d_opus_4,     _d_opus_4_len,     200000
    dq _m_sonnet_4,   _m_sonnet_4_len,   _d_sonnet_4,   _d_sonnet_4_len,   200000
_model_table_end:

_MODEL_REC_BYTES = 40


; --- from app/panes.inc ---
str_claude db 'claude'


; --- from app/sessions.inc ---
_skey_session_id     db 'sessionId'
_skey_session_id_len = $ - _skey_session_id
_seg_dot_claude      db '/.claude/sessions/'
_seg_dot_claude_len  = $ - _seg_dot_claude
_seg_json_ext        db '.json', 0
_seg_json_ext_len    = $ - _seg_json_ext - 1


; --- from app/status.inc ---
_cps_path_tmux        db '/usr/bin/tmux', 0
_cps_arg_cap          db 'capture-pane', 0
_cps_arg_t            db '-t', 0
_cps_arg_p            db '-p', 0

_psf_marker_esc       db 'Esc to cancel'
_psf_marker_esc_len   = $ - _psf_marker_esc
_psf_ellipsis         db 0xE2, 0x80, 0xA6     ; "…" U+2026
_psf_chevron          db 0xE2, 0x9D, 0xAF     ; "❯" U+276F

_CPS_OUT_BYTES = 16384


; --- from app/timefmt.inc ---

; cumulative days at start of each month (non-leap, leap)
align 4
_month_off_normal:
    dd  0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
_month_off_leap:
    dd  0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335


; --- from os/proc_paths.inc ---
_pp_proc_stat_path db '/proc/stat', 0

; _pp_scratch lives in arena (ARENA_OFF_PP_SCRATCH).

; --- from os/term.inc ---
_esc_clear     db 0x1B,'[2J',0x1B,'[H'
_esc_clear_len = $ - _esc_clear
_esc_alt_on    db 0x1B,'[?1049h'
_esc_alt_on_len = $ - _esc_alt_on
_esc_alt_off   db 0x1B,'[?1049l'
_esc_alt_off_len = $ - _esc_alt_off
_esc_cur_hide  db 0x1B,'[?25l'
_esc_cur_hide_len = $ - _esc_cur_hide
_esc_cur_show  db 0x1B,'[?25h'
_esc_cur_show_len = $ - _esc_cur_show
_esc_inv       db 0x1B,'[7m'
_esc_inv_len   = $ - _esc_inv
_esc_reset     db 0x1B,'[0m'
_esc_reset_len = $ - _esc_reset
_esc_sel_bg_on  db 0x1B,'[48;5;238m'
_esc_sel_bg_on_len = $ - _esc_sel_bg_on
_esc_sel_bg_off db 0x1B,'[49m'
_esc_sel_bg_off_len = $ - _esc_sel_bg_off
_esc_fg_turquoise db 0x1B,'[38;5;51m'
_esc_fg_turquoise_len = $ - _esc_fg_turquoise
_esc_fg_default db 0x1B,'[39m'
_esc_fg_default_len = $ - _esc_fg_default

