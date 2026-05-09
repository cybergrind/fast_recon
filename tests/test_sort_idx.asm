; test_sort_idx.asm
;
; sort_idx_by_u64_desc(rdi=keys, rsi=n, rdx=order_out)
;   Fills order_out[0..n) with permutation of [0..n) such that keys[order[i]]
;   is non-increasing.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; --- ascending input -> indices reversed ---
    ; keys = {10, 20, 30, 40} -> order = {3, 2, 1, 0}
    lea     rdi, [keys1]
    mov     esi, 4
    lea     rdx, [order]
    call    sort_idx_by_u64_desc
    assert_eq_n 1, dword [order + 0], 3
    assert_eq_n 2, dword [order + 4], 2
    assert_eq_n 3, dword [order + 8], 1
    assert_eq_n 4, dword [order + 12], 0

    ; --- already-descending input -> identity ---
    ; keys = {100, 50, 10}
    lea     rdi, [keys2]
    mov     esi, 3
    lea     rdx, [order]
    call    sort_idx_by_u64_desc
    assert_eq_n 10, dword [order + 0], 0
    assert_eq_n 11, dword [order + 4], 1
    assert_eq_n 12, dword [order + 8], 2

    ; --- with ties: equal keys keep input order (stable sort or ok either way) ---
    ; keys = {5, 9, 5, 9} — accept any output where order[0..1] map to keys==9
    ; and order[2..3] map to keys==5.
    lea     rdi, [keys3]
    mov     esi, 4
    lea     rdx, [order]
    call    sort_idx_by_u64_desc
    ; key at order[0] must be 9, at order[1] must be 9
    movsxd  rax, dword [order + 0]
    mov     rcx, qword [keys3 + rax*8]
    assert_eq_n 20, rcx, 9
    movsxd  rax, dword [order + 4]
    mov     rcx, qword [keys3 + rax*8]
    assert_eq_n 21, rcx, 9
    movsxd  rax, dword [order + 8]
    mov     rcx, qword [keys3 + rax*8]
    assert_eq_n 22, rcx, 5
    movsxd  rax, dword [order + 12]
    mov     rcx, qword [keys3 + rax*8]
    assert_eq_n 23, rcx, 5

    ; --- n=0 is a no-op ---
    lea     rdi, [keys1]
    xor     esi, esi
    lea     rdx, [order]
    mov     dword [order], 0xDEADBEEF
    call    sort_idx_by_u64_desc
    assert_eq_n 30, dword [order], 0xDEADBEEF

    ; --- n=1 always {0} ---
    lea     rdi, [keys1]
    mov     esi, 1
    lea     rdx, [order]
    call    sort_idx_by_u64_desc
    assert_eq_n 40, dword [order], 0

    test_pass

include '../src/lib.inc'

segment readable
keys1 dq 10, 20, 30, 40
keys2 dq 100, 50, 10
keys3 dq 5, 9, 5, 9

segment readable writeable
align 4
order rd 16
