; test_model.asm — model_display_name(rdi=id, rsi=id_len, rdx=outbuf, rcx=cap)
;                            -> rax = display_len
;
; And model_context_window(rdi=id, rsi=id_len) -> rax = window (default 200000).

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; opus 4.7 -> "Opus 4.7"
    lea     rdi, [m_opus_47]
    mov     esi, m_opus_47_len
    lea     rdx, [obuf]
    mov     ecx, 64
    call    model_display_name
    assert_eq_n 1, rax, 8
    lea     rdi, [obuf]
    lea     rsi, [exp_opus_47]
    mov     edx, 8
    call    memcmp
    assert_eq_n 2, rax, 0

    ; sonnet-4-6 -> "Sonnet 4.6"
    lea     rdi, [m_sonnet_46]
    mov     esi, m_sonnet_46_len
    lea     rdx, [obuf]
    mov     ecx, 64
    call    model_display_name
    assert_eq_n 3, rax, 10
    lea     rdi, [obuf]
    lea     rsi, [exp_sonnet_46]
    mov     edx, 10
    call    memcmp
    assert_eq_n 4, rax, 0

    ; unknown -> echo input verbatim
    lea     rdi, [m_unk]
    mov     esi, m_unk_len
    lea     rdx, [obuf]
    mov     ecx, 64
    call    model_display_name
    assert_eq_n 10, rax, m_unk_len
    lea     rdi, [obuf]
    lea     rsi, [m_unk]
    mov     edx, m_unk_len
    call    memcmp
    assert_eq_n 11, rax, 0

    ; context window: opus-4-7 = 1_000_000
    lea     rdi, [m_opus_47]
    mov     esi, m_opus_47_len
    call    model_context_window
    mov     rcx, 1000000
    assert_eq_n 20, rax, rcx

    ; sonnet-4-6 = 200000
    lea     rdi, [m_sonnet_46]
    mov     esi, m_sonnet_46_len
    call    model_context_window
    mov     rcx, 200000
    assert_eq_n 21, rax, rcx

    ; unknown = 200000
    lea     rdi, [m_unk]
    mov     esi, m_unk_len
    call    model_context_window
    mov     rcx, 200000
    assert_eq_n 22, rax, rcx

    test_pass

include '../src/lib.inc'

segment readable
m_opus_47    db 'claude-opus-4-7'
m_opus_47_len = $ - m_opus_47
m_sonnet_46  db 'claude-sonnet-4-6'
m_sonnet_46_len = $ - m_sonnet_46
m_unk        db 'claude-something-else'
m_unk_len    = $ - m_unk
exp_opus_47  db 'Opus 4.7'
exp_sonnet_46 db 'Sonnet 4.6'

segment readable writeable
obuf rb 64
