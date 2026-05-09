; test_exec.asm
;
; exec_capture(rdi=argv0_ptr, rsi=argv_array, rdx=buf, rcx=buf_cap)
;   argv_array: NULL-terminated array of `char*` (argv[0] same as argv0_ptr).
;   On success: rax = bytes written into buf, rdx = exit_status (low 8 bits).
;   On error:   rax = -errno (negative).
;
; Test: run /bin/echo hello — captured stdout should be "hello\n", exit 0.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    lea     rdi, [path_echo]
    lea     rsi, [argv_echo]
    lea     rdx, [obuf]
    mov     ecx, 64
    call    exec_capture
    ; bytes written = 6 ("hello\n")
    assert_eq_n 1, rax, 6
    movzx   ecx, byte [obuf + 0]
    assert_eq_n 2, rcx, 'h'
    movzx   ecx, byte [obuf + 5]
    assert_eq_n 3, rcx, 0x0A
    ; exit status 0
    assert_eq_n 4, rdx, 0

    ; second invocation: /bin/true exits 0, no output
    lea     rdi, [path_true]
    lea     rsi, [argv_true]
    lea     rdx, [obuf]
    mov     ecx, 64
    call    exec_capture
    assert_eq_n 10, rax, 0
    assert_eq_n 11, rdx, 0

    ; third: /bin/false exits 1
    lea     rdi, [path_false]
    lea     rsi, [argv_false]
    lea     rdx, [obuf]
    mov     ecx, 64
    call    exec_capture
    assert_eq_n 20, rdx, 1

    test_pass

include '../src/lib.inc'

segment readable
path_echo  db '/usr/bin/echo', 0
arg_hello  db 'hello', 0
path_true  db '/usr/bin/true', 0
path_false db '/usr/bin/false', 0

argv_echo  dq path_echo, arg_hello, 0
argv_true  dq path_true, 0
argv_false dq path_false, 0

segment readable writeable
obuf rb 64
