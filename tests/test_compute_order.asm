; test_compute_order.asm
;
; compute_pane_order(rdi=row_data_base, rsi=n, rdx=order_out, rcx=keys_scratch)
;   - Reads u64 ts_epoch from each row at offset ROW_OFF_TS_EPOCH.
;   - Writes those keys into keys_scratch[0..n).
;   - Sorts indices into order_out so row_data[order[i]].ts_epoch is
;     non-increasing.
;
; Verifies the bridge between refresh_panes' enrichment step and the sort:
; whatever ts_epoch lives in each row must end up driving the display order.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; populate three rows: epochs = [100, 300, 200]
    lea     rdi, [rows]
    xor     esi, esi
    mov     edx, 3 * ROW_BYTES
    call    memset
    mov     qword [rows + 0 * ROW_BYTES + ROW_OFF_TS_EPOCH], 100
    mov     qword [rows + 1 * ROW_BYTES + ROW_OFF_TS_EPOCH], 300
    mov     qword [rows + 2 * ROW_BYTES + ROW_OFF_TS_EPOCH], 200

    lea     rdi, [rows]
    mov     esi, 3
    lea     rdx, [order]
    lea     rcx, [keys]
    call    compute_pane_order

    ; expected order: 1 (300), 2 (200), 0 (100)
    movsxd  rax, dword [order + 0]
    assert_eq_n 1, rax, 1
    movsxd  rax, dword [order + 4]
    assert_eq_n 2, rax, 2
    movsxd  rax, dword [order + 8]
    assert_eq_n 3, rax, 0

    ; keys must mirror ts_epoch in row order (not sorted) — that's the contract
    mov     rax, qword [keys + 0]
    assert_eq_n 10, rax, 100
    mov     rax, qword [keys + 8]
    assert_eq_n 11, rax, 300
    mov     rax, qword [keys + 16]
    assert_eq_n 12, rax, 200

    ; n=0 is a no-op (must not blow up)
    lea     rdi, [rows]
    xor     esi, esi
    lea     rdx, [order]
    lea     rcx, [keys]
    call    compute_pane_order

    ; n=1 must yield order=[0]
    mov     dword [order + 0], 0xFFFFFFFF
    lea     rdi, [rows]
    mov     esi, 1
    lea     rdx, [order]
    lea     rcx, [keys]
    call    compute_pane_order
    movsxd  rax, dword [order + 0]
    assert_eq_n 20, rax, 0

    test_pass

include '../src/lib.inc'

segment readable writeable
align 8
rows  rb 8 * ROW_BYTES
order rd 8
keys  rq 8
