; app/jsonl_parse.inc — extract model/tokens/timestamp from JSONL buffer.
;
; struct jsonl_info layout (128 bytes):
;   +0   model_len            q
;   +8   input_tokens         q     (input + cache_creation + cache_read)
;   +16  output_tokens        q
;   +24  timestamp_len        q
;   +32  model_buf[64]
;   +96  timestamp_buf[32]
;
; Per recon: only `"type":"assistant"` lines are interesting; "<synthetic>"
; assistants are ignored.

INFO_OFF_MODEL_LEN  = 0
INFO_OFF_INPUT      = 8
INFO_OFF_OUTPUT     = 16
INFO_OFF_TS_LEN     = 24
INFO_OFF_MODEL_BUF  = 32
INFO_OFF_TS_BUF     = 96
INFO_BYTES          = 128

segment readable executable

; _find_key_in(start, end, key_ptr, key_len) -> rax/rdx/rcx
; Convenience: call json_find_key with (start, end, key, len). Same return
; semantics as json_find_key. Args via stack to avoid every caller building
; the same dance.

; _copy_inner_str(rdi=value_start, rdx=value_end, rsi=outbuf, rcx=cap, r8=&out_len_field)
; If value range starts/ends with '"', copies the inner content (truncated
; to cap), updates [r8] = stored length. No-op if not a string.
_copy_inner_str:
    cmp     byte [rdi], '"'
    jne     .out
    inc     rdi
    dec     rdx
    cmp     rdi, rdx
    ja      .out
    push    r8
    push    rsi
    push    rdi
    mov     rsi, rdx
    sub     rsi, rdi                 ; inner len
    cmp     rsi, rcx
    jbe     .ok
    mov     rsi, rcx
.ok:
    pop     rdi
    push    rsi
    pop     r9                       ; final length
    push    r9
    mov     rdx, r9                  ; len for memcpy
    pop     r9                       ; restore len
    pop     rsi                      ; outbuf
    pop     r8                       ; &len_field
    push    r8
    push    r9
    push    rsi                      ; outbuf to dst
    mov     rdi, rsi
    pop     rdi                      ; dst
    pop     r9
    pop     r8
.out:
    ret
; ^^^ that proc got tangled — abandoning helper and inlining instead.

; parse_jsonl_buf(rdi=buf, rsi=len, rdx=&info) -> rax = 0/-1
parse_jsonl_buf:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    mov     r14, rdi                 ; cursor
    lea     r15, [rdi + rsi]         ; end
    mov     r12, rdx                 ; &info

.line_loop:
    cmp     r14, r15
    jae     .done
    mov     rbx, r14                 ; line_start
.find_eol:
    cmp     r14, r15
    jae     .have_line
    cmp     byte [r14], 0x0A
    je      .have_line
    inc     r14
    jmp     .find_eol
.have_line:
    mov     rbp, r14                 ; line_end (exclusive of \n)
    cmp     r14, r15
    jae     .skip_advance
    inc     r14                      ; for next iter
.skip_advance:

    ; -- find top-level "type" --
    mov     rdi, rbx
    mov     rsi, rbp
    lea     rdx, [_skey_type]
    mov     ecx, _skey_type_len
    call    json_find_key
    test    rcx, rcx
    js      .line_loop
    cmp     byte [rax], '"'
    jne     .line_loop
    ; inner range: [rax+1, rdx-1) — value without quotes
    lea     r8, [rax + 1]
    lea     r9, [rdx - 1]
    mov     r11, r9
    sub     r11, r8                  ; inner len

    ; "assistant" → full extraction (timestamp + model + tokens)
    cmp     r11, _str_assistant_len
    jne     .check_user_or_system
    push    r8
    push    r9
    mov     rdi, r8
    lea     rsi, [_str_assistant]
    mov     rdx, _str_assistant_len
    call    memcmp
    pop     r9
    pop     r8
    test    rax, rax
    jnz     .check_user_or_system    ; same length but different word
    jmp     .have_assistant

.check_user_or_system:
    ; "user" or "system" → timestamp-only update
    cmp     r11, _str_user_len
    jne     .check_system
    push    r8
    push    r9
    mov     rdi, r8
    lea     rsi, [_str_user]
    mov     rdx, _str_user_len
    call    memcmp
    pop     r9
    pop     r8
    test    rax, rax
    jz      .ts_only
.check_system:
    cmp     r11, _str_system_len
    jne     .line_loop
    push    r8
    push    r9
    mov     rdi, r8
    lea     rsi, [_str_system]
    mov     rdx, _str_system_len
    call    memcmp
    pop     r9
    pop     r8
    test    rax, rax
    jnz     .line_loop
.ts_only:
    ; pull just the timestamp; do not touch model/tokens
    mov     rdi, rbx
    mov     rsi, rbp
    lea     rdx, [_skey_timestamp]
    mov     ecx, _skey_timestamp_len
    call    json_find_key
    test    rcx, rcx
    js      .line_loop
    cmp     byte [rax], '"'
    jne     .line_loop
    inc     rax
    dec     rdx
    cmp     rax, rdx
    ja      .line_loop
    mov     r9, rdx
    sub     r9, rax
    cmp     r9, 32
    jbe     .ts_only_ok
    mov     r9, 32
.ts_only_ok:
    mov     qword [r12 + INFO_OFF_TS_LEN], r9
    push    r9
    mov     rsi, rax
    lea     rdi, [r12 + INFO_OFF_TS_BUF]
    mov     rdx, r9
    call    memcpy
    pop     r9
    jmp     .line_loop

.have_assistant:

    ; -- skip if line contains "<synthetic>" --
    mov     rdi, rbx
    mov     rsi, rbp
    sub     rsi, rdi                 ; line len for _strstr
    lea     rdx, [_str_synthetic]
    mov     ecx, _str_synthetic_len
    call    _strstr
    test    rax, rax
    jnz     .line_loop

    ; -- top-level "timestamp" --
    mov     rdi, rbx
    mov     rsi, rbp
    lea     rdx, [_skey_timestamp]
    mov     ecx, _skey_timestamp_len
    call    json_find_key
    test    rcx, rcx
    js      .skip_ts
    cmp     byte [rax], '"'
    jne     .skip_ts
    inc     rax
    dec     rdx
    cmp     rax, rdx
    ja      .skip_ts
    mov     r9, rdx
    sub     r9, rax                  ; ts len
    cmp     r9, 32
    jbe     .ts_ok
    mov     r9, 32
.ts_ok:
    mov     qword [r12 + INFO_OFF_TS_LEN], r9
    push    r9
    mov     rsi, rax
    lea     rdi, [r12 + INFO_OFF_TS_BUF]
    mov     rdx, r9
    call    memcpy
    pop     r9
.skip_ts:

    ; -- find top-level "message" object --
    mov     rdi, rbx
    mov     rsi, rbp
    lea     rdx, [_skey_message]
    mov     ecx, _skey_message_len
    call    json_find_key
    test    rcx, rcx
    js      .line_loop
    cmp     byte [rax], '{'
    jne     .line_loop
    ; preserve message range across the model + usage extraction
    mov     qword [_msg_start], rax
    mov     qword [_msg_end], rdx

    ; -- model inside message --
    mov     rdi, rax
    mov     rsi, rdx
    lea     rdx, [_skey_model]
    mov     ecx, _skey_model_len
    call    json_find_key
    test    rcx, rcx
    js      .skip_model
    cmp     byte [rax], '"'
    jne     .skip_model
    inc     rax
    dec     rdx
    cmp     rax, rdx
    ja      .skip_model
    mov     r9, rdx
    sub     r9, rax
    cmp     r9, 64
    jbe     .model_ok
    mov     r9, 64
.model_ok:
    mov     qword [r12 + INFO_OFF_MODEL_LEN], r9
    push    r9
    mov     rsi, rax
    lea     rdi, [r12 + INFO_OFF_MODEL_BUF]
    mov     rdx, r9
    call    memcpy
    pop     r9
.skip_model:

    ; -- usage inside message --
    mov     rdi, qword [_msg_start]
    mov     rsi, qword [_msg_end]
    lea     rdx, [_skey_usage]
    mov     ecx, _skey_usage_len
    call    json_find_key
    test    rcx, rcx
    js      .line_done
    cmp     byte [rax], '{'
    jne     .line_done
    mov     qword [_usage_start], rax
    mov     qword [_usage_end], rdx

    ; input_tokens + cache_creation + cache_read
    xor     r10, r10
    mov     rdi, qword [_usage_start]
    mov     rsi, qword [_usage_end]
    lea     rdx, [_skey_input]
    mov     ecx, _skey_input_len
    call    json_find_key
    test    rcx, rcx
    js      .no_inp
    mov     rdi, rax
    mov     rsi, rdx
    sub     rsi, rdi
    call    parse_u64
    add     r10, rax
.no_inp:

    mov     rdi, qword [_usage_start]
    mov     rsi, qword [_usage_end]
    lea     rdx, [_skey_cache_create]
    mov     ecx, _skey_cache_create_len
    call    json_find_key
    test    rcx, rcx
    js      .no_cc
    mov     rdi, rax
    mov     rsi, rdx
    sub     rsi, rdi
    call    parse_u64
    add     r10, rax
.no_cc:

    mov     rdi, qword [_usage_start]
    mov     rsi, qword [_usage_end]
    lea     rdx, [_skey_cache_read]
    mov     ecx, _skey_cache_read_len
    call    json_find_key
    test    rcx, rcx
    js      .no_cr
    mov     rdi, rax
    mov     rsi, rdx
    sub     rsi, rdi
    call    parse_u64
    add     r10, rax
.no_cr:
    mov     qword [r12 + INFO_OFF_INPUT], r10

    mov     rdi, qword [_usage_start]
    mov     rsi, qword [_usage_end]
    lea     rdx, [_skey_output]
    mov     ecx, _skey_output_len
    call    json_find_key
    test    rcx, rcx
    js      .line_done
    mov     rdi, rax
    mov     rsi, rdx
    sub     rsi, rdi
    call    parse_u64
    mov     qword [r12 + INFO_OFF_OUTPUT], rax

.line_done:
    jmp     .line_loop

.done:
    xor     eax, eax
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

; _strstr(rdi=hay, rsi=hay_len, rdx=needle, rcx=needle_len) -> rax = ptr or 0.
_strstr:
    push    rbx
    push    rbp
    mov     rbx, rdx                 ; needle
    mov     rbp, rcx                 ; needle_len
    test    rbp, rbp
    jz      .miss
    cmp     rsi, rbp
    jb      .miss
    sub     rsi, rbp
    lea     r8, [rdi + rsi + 1]      ; one past last possible start
.outer:
    cmp     rdi, r8
    jae     .miss
    push    rdi
    push    r8
    mov     rsi, rbx
    mov     rdx, rbp
    call    memcmp
    pop     r8
    pop     rdi
    test    rax, rax
    jz      .hit
    inc     rdi
    jmp     .outer
.hit:
    mov     rax, rdi
    pop     rbp
    pop     rbx
    ret
.miss:
    xor     eax, eax
    pop     rbp
    pop     rbx
    ret

segment readable
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

segment readable writeable
align 8
_msg_start    rq 1
_msg_end      rq 1
_usage_start  rq 1
_usage_end    rq 1
