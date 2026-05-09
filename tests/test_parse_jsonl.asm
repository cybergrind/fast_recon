; test_parse_jsonl.asm
;
; struct jsonl_info (128 bytes):
;   +0   model_len            q
;   +8   input_tokens         q
;   +16  output_tokens        q
;   +24  timestamp_len        q
;   +32  model_buf[64]
;   +96  timestamp_buf[32]
;
; parse_jsonl_buf(rdi=buf, rsi=len, rdx=&info) -> rax = 0/-1
;   Iterates JSONL lines. Updates info from each "assistant"-type record.
;   model and timestamp are taken from the latest qualifying line.
;   input_tokens = usage.input + cache_creation + cache_read; output = usage.output.

INFO_OFF_MODEL_LEN  = 0
INFO_OFF_INPUT      = 8
INFO_OFF_OUTPUT     = 16
INFO_OFF_TS_LEN     = 24
INFO_OFF_MODEL_BUF  = 32
INFO_OFF_TS_BUF     = 96
INFO_BYTES          = 128

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; zero info struct
    lea     rdi, [info]
    xor     esi, esi
    mov     edx, INFO_BYTES
    call    memset

    lea     rdi, [doc]
    lea     rsi, [doc_end]
    sub     rsi, rdi
    lea     rdx, [info]
    call    parse_jsonl_buf
    assert_eq_n 1, rax, 0

    ; expected: latest assistant model is "claude-sonnet-4-6"
    mov     r12, qword [info + INFO_OFF_MODEL_LEN]
    assert_eq_n 2, r12, 17
    lea     rdi, [info + INFO_OFF_MODEL_BUF]
    lea     rsi, [exp_model]
    mov     edx, 17
    call    memcmp
    assert_eq_n 3, rax, 0

    ; tokens: latest assistant has input=1000, cache_creation=200, cache_read=300, output=42
    ;   total_input = 1500
    mov     r12, qword [info + INFO_OFF_INPUT]
    assert_eq_n 4, r12, 1500
    mov     r12, qword [info + INFO_OFF_OUTPUT]
    assert_eq_n 5, r12, 42

    ; latest timestamp = "2025-12-31T23:59:59.000Z" (24 chars)
    mov     r12, qword [info + INFO_OFF_TS_LEN]
    assert_eq_n 6, r12, 24
    lea     rdi, [info + INFO_OFF_TS_BUF]
    lea     rsi, [exp_ts]
    mov     edx, 24
    call    memcmp
    assert_eq_n 7, rax, 0

    test_pass

include '../src/lib.inc'

segment readable
; A JSONL fixture with: a user line (ignored), an assistant with old data,
; a synthetic assistant (ignored), and the final assistant with the
; values we expect to surface.
doc:
    db '{"type":"user","timestamp":"2025-01-01T00:00:00.000Z","message":{"role":"user"}}', 0x0A
    db '{"type":"assistant","timestamp":"2025-06-01T12:00:00.000Z","message":{"model":"claude-opus-4-7","usage":{"input_tokens":50,"cache_creation_input_tokens":0,"cache_read_input_tokens":0,"output_tokens":10}}}', 0x0A
    db '{"type":"assistant","message":{"model":"<synthetic>","usage":{"input_tokens":99999,"output_tokens":99999}}}', 0x0A
    db '{"type":"assistant","timestamp":"2025-12-31T23:59:59.000Z","message":{"model":"claude-sonnet-4-6","usage":{"input_tokens":1000,"cache_creation_input_tokens":200,"cache_read_input_tokens":300,"output_tokens":42}}}', 0x0A
doc_end:

exp_model db 'claude-sonnet-4-6'
exp_ts    db '2025-12-31T23:59:59.000Z'
dbglf     db 0x0A

segment readable writeable
dbgnum rb 24

segment readable writeable
align 8
info rb INFO_BYTES
