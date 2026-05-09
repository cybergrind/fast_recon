; test_pane_cache.asm
;
; Per-pid cache for the stable parts of a pane row (cwd / git / uuid /
; jsonl_path). Three primitives:
;
;   pcache_lookup(rdi=pid)         -> rax = ptr or 0
;   pcache_alloc(rdi=pid)          -> rax = ptr  (re-uses existing slot for
;                                                 the same pid; otherwise
;                                                 takes a free slot, or the
;                                                 numerically lowest slot if
;                                                 the table is full)
;   pcache_evict_missing(rdi=alive_pids_buf, rsi=count)
;       — clears entries whose pid is not in the alive list

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    call    arena_init

    ; tests rely on a clean cache
    call    pcache_reset

    ; --- 1. lookup miss before any alloc ---
    mov     edi, 1234
    call    pcache_lookup
    assert_eq_n 1, rax, 0

    ; --- 2. alloc returns a slot and lookup hits it ---
    mov     edi, 1234
    call    pcache_alloc
    test    rax, rax
    jnz     .a_ok
    mov     edi, 100
    mov     eax, SYS_exit
    syscall
.a_ok:
    mov     r12, rax                 ; slot ptr
    mov     rdi, r12
    mov     rax, qword [rdi + PCACHE_OFF_PID]
    assert_eq_n 2, rax, 1234

    mov     edi, 1234
    call    pcache_lookup
    assert_eq_n 3, rax, r12

    ; --- 3. allocating the same pid returns the same slot ---
    mov     edi, 1234
    call    pcache_alloc
    assert_eq_n 4, rax, r12

    ; --- 4. allocating a different pid returns a different slot ---
    mov     edi, 5678
    call    pcache_alloc
    cmp     rax, r12
    jne     .b_ok
    mov     edi, 110
    mov     eax, SYS_exit
    syscall
.b_ok:
    mov     r13, rax
    mov     rdi, r13
    mov     rax, qword [rdi + PCACHE_OFF_PID]
    assert_eq_n 5, rax, 5678

    ; --- 5. evict_missing(alive=[5678]) drops the 1234 slot ---
    mov     dword [alive + 0], 5678
    lea     rdi, [alive]
    mov     rsi, 1
    call    pcache_evict_missing

    mov     edi, 1234
    call    pcache_lookup
    assert_eq_n 6, rax, 0

    mov     edi, 5678
    call    pcache_lookup
    assert_eq_n 7, rax, r13

    ; --- 6. payload survives across lookups ---
    call    pcache_reset
    mov     edi, 9999
    call    pcache_alloc
    mov     r12, rax
    mov     qword [r12 + PCACHE_OFF_CWD_LEN], 5
    mov     dword [r12 + PCACHE_OFF_CWD + 0], 'abcd'
    mov     byte  [r12 + PCACHE_OFF_CWD + 4], 'e'

    mov     edi, 9999
    call    pcache_lookup
    mov     r13, rax
    mov     rax, qword [r13 + PCACHE_OFF_CWD_LEN]
    assert_eq_n 20, rax, 5
    movzx   eax, byte [r13 + PCACHE_OFF_CWD + 0]
    assert_eq_n 21, rax, 'a'
    movzx   eax, byte [r13 + PCACHE_OFF_CWD + 4]
    assert_eq_n 22, rax, 'e'

    test_pass

include '../src/lib.inc'

segment readable writeable
align 8
alive  rd 16
