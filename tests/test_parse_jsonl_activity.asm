; test_parse_jsonl_activity.asm
;
; "Last activity" must reflect the most recent line of any qualifying type
; (assistant / user / system) — recon does the same. While claude is
; mid-generation the latest assistant is from the previous turn, but the
; user message that started the current turn IS already on disk and should
; push the last-activity timestamp forward to "now".
;
; Model and token counts still come from the last assistant line only.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    lea     rdi, [info]
    xor     esi, esi
    mov     edx, INFO_BYTES
    call    memset

    lea     rdi, [doc]
    mov     rsi, doc_end - doc
    lea     rdx, [info]
    call    parse_jsonl_buf

    ; --- Last assistant supplies model + tokens (unchanged behavior) ---
    mov     rax, qword [info + INFO_OFF_MODEL_LEN]
    mov     rcx, exp_model_end - exp_model
    assert_eq_n 1, rax, rcx
    mov     rdi, info + INFO_OFF_MODEL_BUF
    lea     rsi, [exp_model]
    mov     rdx, exp_model_end - exp_model
    call    memcmp
    assert_eq_n 2, rax, 0

    mov     rax, qword [info + INFO_OFF_INPUT]
    assert_eq_n 3, rax, 1500
    mov     rax, qword [info + INFO_OFF_OUTPUT]
    assert_eq_n 4, rax, 42

    ; --- The trailing user line wins for last activity ---
    mov     rax, qword [info + INFO_OFF_TS_LEN]
    mov     rcx, exp_ts_end - exp_ts
    assert_eq_n 5, rax, rcx
    mov     rdi, info + INFO_OFF_TS_BUF
    lea     rsi, [exp_ts]
    mov     rdx, exp_ts_end - exp_ts
    call    memcmp
    assert_eq_n 6, rax, 0

    test_pass

include '../src/lib.inc'

segment readable
doc:
    ; assistant turn from earlier today (model/tokens flushed)
    db '{"type":"assistant","timestamp":"2026-05-09T10:00:00.000Z","message":{"model":"claude-opus-4-7","usage":{"input_tokens":1000,"cache_creation_input_tokens":200,"cache_read_input_tokens":300,"output_tokens":42}}}', 0x0A
    ; user just now started a new turn, claude has not yet written its
    ; reply assistant line — last_activity should still advance from this
    db '{"type":"user","timestamp":"2026-05-09T11:30:00.000Z","message":{"role":"user","content":"more please"}}', 0x0A
doc_end:

exp_model     db 'claude-opus-4-7'
exp_model_end:
exp_ts        db '2026-05-09T11:30:00.000Z'
exp_ts_end:

segment readable writeable
align 8
info rb INFO_BYTES
