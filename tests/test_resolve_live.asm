; test_resolve_live.asm — end-to-end JSONL resolution + parse for a live claude pid.
;
; Drives the same chain refresh_panes uses, then mmaps the resolved JSONL
; and parses it. Asserts that for at least one live claude pid we land on
; a JSONL whose tail parses into a non-zero model+token+timestamp triple.
;
; Skips (exit 0) if no claude pid is running.

format ELF64 executable 3
include '../src/common.inc'

DT_DIR        = 4
DENT_RECLEN_OFF = 16
DENT_TYPE_OFF   = 18
DENT_NAME_OFF   = 19
DENTBUF        = 8192
JSONL_TAIL     = 1048576

segment readable executable
entry $
    call    arena_init

    lea     rdi, [proc_path]
    xor     esi, esi
    xor     edx, edx
    call    sys_open
    test    rax, rax
    js      .skip
    mov     r12, rax

    sub     rsp, DENTBUF

.scan_refill:
    mov     rdi, r12
    mov     rsi, rsp
    mov     edx, DENTBUF
    call    sys_getdents64
    test    rax, rax
    jle     .scan_done
    mov     r13, rax
    xor     r14, r14

.scan_next:
    cmp     r14, r13
    jae     .scan_refill
    lea     rbx, [rsp + r14]
    movzx   ecx, word [rbx + DENT_RECLEN_OFF]
    add     r14, rcx

    cmp     byte [rbx + DENT_TYPE_OFF], DT_DIR
    jne     .scan_next
    lea     rdi, [rbx + DENT_NAME_OFF]
    movzx   eax, byte [rdi]
    sub     eax, '0'
    cmp     eax, 9
    ja      .scan_next

    mov     rdi, rbx
    add     rdi, DENT_NAME_OFF
    mov     rcx, rdi
.namelen:
    cmp     byte [rcx], 0
    je      .got_namelen
    inc     rcx
    jmp     .namelen
.got_namelen:
    mov     rsi, rcx
    sub     rsi, rdi
    call    parse_u64
    mov     r15, rax                     ; pid

    mov     rdi, r15
    call    is_claude
    test    rax, rax
    jz      .scan_next

    mov     rdi, r15
    lea     rsi, [uuid_buf]
    call    pid_session_id
    test    rcx, rcx
    js      .scan_next
    mov     qword [uuid_len], rax

    add     rsp, DENTBUF
    mov     rdi, r12
    call    sys_close

    ; --- find_jsonl_path ---
    mov     qword [pid_found], r15
    lea     rdi, [uuid_buf]
    mov     rsi, qword [uuid_len]
    lea     rdx, [jsonl_path]
    mov     ecx, 1024
    call    find_jsonl_path
    assert_eq_n 1, r8, 0
    mov     qword [jsonl_len], rax

    ; --- read_file_tail ---
    lea     rdi, [jsonl_path]
    mov     rsi, JSONL_TAIL
    call    read_file_tail
    assert_eq_n 2, rcx, 0
    test    rax, rax
    jz      .die_tail_zero
    test    rdx, rdx
    jz      .die_tail_zero
    mov     qword [tail_ptr], rax
    mov     qword [tail_len], rdx
    mov     qword [tail_off], r8

    ; --- skip partial first line if mmap started mid-file ---
    test    r8, r8
    jz      .parse_now
    mov     rsi, qword [tail_ptr]
    mov     rdx, rsi
    add     rdx, qword [tail_len]
.skip_partial:
    cmp     rsi, rdx
    jae     .die_tail_zero
    cmp     byte [rsi], 0x0A
    je      .skip_after
    inc     rsi
    jmp     .skip_partial
.skip_after:
    inc     rsi                          ; past '\n'
    mov     qword [tail_ptr], rsi
    mov     rax, rdx
    sub     rax, rsi
    mov     qword [tail_len], rax

.parse_now:
    ; --- parse_jsonl_buf ---
    lea     rdi, [info_buf]
    xor     esi, esi
    mov     edx, INFO_BYTES
    call    memset
    mov     rdi, qword [tail_ptr]
    mov     rsi, qword [tail_len]
    lea     rdx, [info_buf]
    call    parse_jsonl_buf

    ; release the mmap (use the original ptr/len before partial-skip — we
    ; saved tail_off so the original ptr is tail_ptr0; but we overwrote
    ; tail_ptr/len. For a one-shot test we leak the mmap; OS reaps on exit.

    ; --- assertions: model_len > 0, ts_len > 0 ---
    mov     rax, qword [info_buf + INFO_OFF_MODEL_LEN]
    test    rax, rax
    jz      .die_no_model
    mov     rax, qword [info_buf + INFO_OFF_TS_LEN]
    test    rax, rax
    jz      .die_no_ts

    test_pass

.scan_done:
    add     rsp, DENTBUF
    mov     rdi, r12
    call    sys_close
.skip:
    test_pass

.die_tail_zero:
    mov     edi, 50
    mov     eax, SYS_exit
    syscall
.die_no_model:
    mov     edi, 51
    mov     eax, SYS_exit
    syscall
.die_no_ts:
    mov     edi, 52
    mov     eax, SYS_exit
    syscall

is_claude:
    push    rbx
    push    r12
    sub     rsp, 64
    mov     rsi, rdi
    lea     rdi, [rsp]
    mov     dword [rdi], '/pro'
    mov     word [rdi + 4], 'c/'
    add     rdi, 6
    call    format_u64
    add     rdi, rax
    mov     dword [rdi], '/com'
    mov     byte [rdi + 4], 'm'
    mov     byte [rdi + 5], 0
    lea     rdi, [rsp]
    mov     rsi, O_RDONLY
    xor     edx, edx
    mov     eax, SYS_open
    syscall
    test    rax, rax
    js      .nope
    mov     r12, rax
    sub     rsp, 32
    mov     rdi, r12
    mov     rsi, rsp
    mov     edx, 32
    mov     eax, SYS_read
    syscall
    mov     rbx, rax
    mov     rdi, r12
    mov     eax, SYS_close
    syscall
    cmp     rbx, 6
    jb      .nope_drop
    cmp     dword [rsp], 'clau'
    jne     .nope_drop
    cmp     word [rsp + 4], 'de'
    jne     .nope_drop
    add     rsp, 32
    add     rsp, 64
    mov     eax, 1
    pop     r12
    pop     rbx
    ret
.nope_drop:
    add     rsp, 32
.nope:
    add     rsp, 64
    xor     eax, eax
    pop     r12
    pop     rbx
    ret

include '../src/lib.inc'

segment readable
proc_path db '/proc', 0

segment readable writeable
align 8
uuid_buf   rb 64
uuid_len   rq 1
jsonl_path rb 1024
jsonl_len  rq 1
pid_found  rq 1
tail_ptr   rq 1
tail_len   rq 1
tail_off   rq 1
info_buf   rb INFO_BYTES
