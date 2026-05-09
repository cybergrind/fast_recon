; test_jsonl_first_ts.asm
;
; jsonl_first_ts_epoch(rdi=path0) -> rax = epoch_secs, rcx = 0/-errno.
;
; Reads first line of a JSONL file, finds top-level "timestamp" string,
; converts it via iso8601_to_epoch.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; Write a small JSONL file. First line is a system entry with a known
    ; timestamp (2026-01-15T12:00:00.000Z = 1768564800).
    lea     rdi, [path0]
    mov     esi, O_WRONLY or O_CREAT or O_TRUNC
    mov     edx, 0644o
    mov     eax, SYS_open
    syscall
    test    rax, rax
    js      .die_open
    mov     r12, rax
    mov     rdi, r12
    lea     rsi, [doc]
    mov     rdx, doc_end - doc
    mov     eax, SYS_write
    syscall
    mov     rdi, r12
    call    sys_close

    lea     rdi, [path0]
    call    jsonl_first_ts_epoch
    assert_eq_n 1, rcx, 0
    mov     rcx, 1768478400
    assert_eq_n 2, rax, rcx

    ; bogus path
    lea     rdi, [bogus0]
    call    jsonl_first_ts_epoch
    test    rcx, rcx
    jns     .die_bogus

    ; Empty file: rcx<0 (no first line)
    lea     rdi, [empty0]
    mov     esi, O_WRONLY or O_CREAT or O_TRUNC
    mov     edx, 0644o
    mov     eax, SYS_open
    syscall
    test    rax, rax
    js      .die_open
    mov     r12, rax
    mov     rdi, r12
    call    sys_close
    lea     rdi, [empty0]
    call    jsonl_first_ts_epoch
    test    rcx, rcx
    jns     .die_empty

    ; cleanup
    lea     rdi, [path0]
    mov     eax, SYS_unlink
    syscall
    lea     rdi, [empty0]
    mov     eax, SYS_unlink
    syscall

    test_pass

.die_open:
    mov     edi, 30
    mov     eax, SYS_exit
    syscall
.die_bogus:
    mov     edi, 11
    mov     eax, SYS_exit
    syscall
.die_empty:
    mov     edi, 12
    mov     eax, SYS_exit
    syscall

include '../src/lib.inc'

segment readable
path0  db '/tmp/test_jsonl_first_ts.jsonl', 0
empty0 db '/tmp/test_jsonl_first_ts_empty.jsonl', 0
bogus0 db '/tmp/this/path/does/not/exist/yz', 0

doc:
    db '{"type":"system","timestamp":"2026-01-15T12:00:00.000Z","content":"hello"}', 0x0A
    db '{"type":"user","timestamp":"2026-02-20T08:00:00.000Z","message":{}}', 0x0A
doc_end:
