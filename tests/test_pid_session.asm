; test_pid_session.asm — pid_session_id end-to-end via fixture file.
;
; Creates $HOME/.claude/sessions/{TEST_PID}.json with known content, calls
; pid_session_id, asserts the parsed UUID, deletes the fixture.

format ELF64 executable 3
include '../src/common.inc'

TEST_PID = 4242000000                ; almost certainly free

segment readable executable
entry $
    call    arena_init
    ; --- build path via build_session_path so we know exactly where to write ---
    mov     rdi, TEST_PID
    lea     rsi, [path_buf]
    mov     edx, 512
    call    build_session_path
    assert_eq_n 1, rcx, 0

    ; --- create fixture file ---
    lea     rdi, [path_buf]
    mov     esi, O_WRONLY or O_CREAT or O_TRUNC
    mov     edx, 0644o
    mov     eax, SYS_open
    syscall
    test    rax, rax
    js      .die
    mov     r12, rax                 ; fd

    mov     rdi, r12
    lea     rsi, [fixture]
    lea     rdx, [fixture_end]
    sub     rdx, rsi
    call    sys_write
    mov     rdi, r12
    call    sys_close

    ; --- pid_session_id ---
    mov     rdi, TEST_PID
    lea     rsi, [uuid_buf]
    call    pid_session_id
    assert_eq_n 2, rcx, 0
    assert_eq_n 3, rax, 36

    lea     rdi, [uuid_buf]
    lea     rsi, [expected_uuid]
    mov     edx, 36
    call    memcmp
    assert_eq_n 4, rax, 0

    ; --- cleanup ---
    lea     rdi, [path_buf]
    mov     eax, SYS_unlink
    syscall

    ; --- not-found case: pid that has no file (use TEST_PID after we deleted the fixture) ---
    mov     rdi, TEST_PID
    lea     rsi, [uuid_buf]
    call    pid_session_id
    assert_eq_n 5, rcx, -1

    test_pass
.die:
    mov     edi, 90
    mov     eax, SYS_exit
    syscall

include '../src/lib.inc'

segment readable
fixture       db '{"pid":4242000000,"sessionId":"abcdef01-2345-6789-abcd-ef0123456789","cwd":"/x","startedAt":1}'
fixture_end:
expected_uuid db 'abcdef01-2345-6789-abcd-ef0123456789'

segment readable writeable
path_buf rb 512
uuid_buf rb 64
