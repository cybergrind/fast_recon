; test_parse_panes.asm
;
; parse_claude_panes(rdi=buf, rsi=len, rdx=out, rcx=out_cap) -> rax = n_panes
;
; Each output record is PANE_REC_BYTES (40):
;   +0  name_ptr   q
;   +8  name_len   q
;   +16 pid        q
;   +24 cmd_ptr    q   (always "claude" — kept for future)
;   +32 cmd_len    q
;
; Skips lines whose command is not "claude". Pointers reference the input buf
; (zero-copy). Caller must keep buf alive for the lifetime of the records.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    lea     rdi, [input]
    lea     rsi, [input_end]
    sub     rsi, rdi                 ; len
    lea     rdx, [recs]
    mov     ecx, 8
    call    parse_claude_panes
    assert_eq_n 1, rax, 2

    ; rec[0]: name "sess2" (len 5), pid 200
    mov     r12, qword [recs + 0*PANE_REC_BYTES + 8]
    assert_eq_n 10, r12, 5
    mov     r12, qword [recs + 0*PANE_REC_BYTES + 16]
    assert_eq_n 11, r12, 200
    ; verify name bytes via memcmp
    mov     rdi, qword [recs + 0*PANE_REC_BYTES + 0]
    lea     rsi, [exp_sess2]
    mov     edx, 5
    call    memcmp
    assert_eq_n 12, rax, 0

    ; rec[1]: name "bar" (len 3), pid 12345
    mov     r12, qword [recs + 1*PANE_REC_BYTES + 8]
    assert_eq_n 20, r12, 3
    mov     r12, qword [recs + 1*PANE_REC_BYTES + 16]
    assert_eq_n 21, r12, 12345
    mov     rdi, qword [recs + 1*PANE_REC_BYTES + 0]
    lea     rsi, [exp_bar]
    mov     edx, 3
    call    memcmp
    assert_eq_n 22, rax, 0

    ; --- empty buffer -> 0 ---
    lea     rdi, [input]
    xor     rsi, rsi
    lea     rdx, [recs]
    mov     ecx, 8
    call    parse_claude_panes
    assert_eq_n 30, rax, 0

    ; --- no trailing newline still parses ---
    lea     rdi, [no_nl]
    lea     rsi, [no_nl_end]
    sub     rsi, rdi
    lea     rdx, [recs]
    mov     ecx, 8
    call    parse_claude_panes
    assert_eq_n 40, rax, 1
    mov     r12, qword [recs + 16]
    assert_eq_n 41, r12, 7

    test_pass

include '../src/lib.inc'

segment readable
input db '%1|sess1|100|zsh', 0x0A
      db '%2|sess2|200|claude', 0x0A
      db '%3|foo|999|vim', 0x0A
      db '%4|bar|12345|claude', 0x0A
input_end:

no_nl db '%5|only|7|claude'
no_nl_end:

exp_sess2 db 'sess2'
exp_bar   db 'bar'

segment readable writeable
align 8
recs rb 8 * PANE_REC_BYTES
