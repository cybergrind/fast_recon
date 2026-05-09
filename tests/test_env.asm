; test_env.asm — read_env_var / home_dir.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; HOME must exist when running tests under any normal shell.
    lea     rdi, [outbuf]
    mov     esi, 256
    call    home_dir
    assert_eq_n 1, r8, 0
    test    rax, rax
    jz      .die                     ; got something
    cmp     byte [outbuf], '/'
    jne     .die                     ; HOME is absolute

    ; nonexistent var should return r8 = -1
    lea     rdi, [name_bogus]
    mov     esi, name_bogus_len
    lea     rdx, [outbuf]
    mov     ecx, 256
    call    read_env_var
    assert_eq_n 2, r8, -1

    test_pass
.die:
    mov     edi, 99
    mov     eax, SYS_exit
    syscall

include '../src/lib.inc'

segment readable
name_bogus db 'FAST_RECON_NEVER_SET_VAR_77'
name_bogus_len = $ - name_bogus

segment readable writeable
outbuf rb 256
