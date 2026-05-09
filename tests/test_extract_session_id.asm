; test_extract_session_id.asm
;
; extract_session_id(rdi=buf, rsi=len, rdx=out36) -> rax=len, rcx=0 ok / -1 fail.
; Looks up "sessionId" in the JSON object, copies the inner string into `out`,
; returns its length (without quotes).

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; ok case
    lea     rdi, [doc1]
    lea     rsi, [doc1_end]
    sub     rsi, rdi
    lea     rdx, [outbuf]
    call    extract_session_id
    assert_eq_n 1, rcx, 0
    assert_eq_n 2, rax, 36
    lea     rdi, [outbuf]
    lea     rsi, [exp1]
    mov     edx, 36
    call    memcmp
    assert_eq_n 3, rax, 0

    ; missing key
    lea     rdi, [doc2]
    lea     rsi, [doc2_end]
    sub     rsi, rdi
    lea     rdx, [outbuf]
    call    extract_session_id
    assert_eq_n 10, rcx, -1

    test_pass

include '../src/lib.inc'

segment readable
doc1 db '{"pid":102861,"sessionId":"4efdb386-fa87-4e0e-aa83-5bff2f6f5e6a","cwd":"/x"}'
doc1_end:
exp1 db '4efdb386-fa87-4e0e-aa83-5bff2f6f5e6a'
doc2 db '{"pid":1,"foo":"bar"}'
doc2_end:

segment readable writeable
outbuf rb 64
