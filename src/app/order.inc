; app/order.inc — compute pane display order from per-row activity epochs.
;
; Depends on ROW_OFF_TS_EPOCH and ROW_BYTES being defined by the including
; module (fast_recon.asm and the unit tests both declare them before
; pulling in lib.inc).


; compute_pane_order(rdi=row_data_base, rsi=n, rdx=order_out, rcx=keys_scratch)
;   Fills keys_scratch[i] ← row_data[i].ts_epoch (u64), then runs
;   sort_idx_by_u64_desc to populate order_out[0..n) with a permutation that
;   places the largest timestamps first. n=0 is a no-op.
compute_pane_order:
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15

    test    rsi, rsi
    jz      .epilogue

    mov     r12, rdi                 ; row_data base
    mov     r13, rsi                 ; n
    mov     r14, rdx                 ; order_out
    mov     r15, rcx                 ; keys_scratch

    xor     rcx, rcx
.fill:
    cmp     rcx, r13
    jae     .fill_done
    mov     rax, rcx
    imul    rax, ROW_BYTES
    add     rax, r12
    mov     rax, qword [rax + ROW_OFF_TS_EPOCH]
    mov     qword [r15 + rcx*8], rax
    inc     rcx
    jmp     .fill
.fill_done:
    mov     rdi, r15
    mov     rsi, r13
    mov     rdx, r14
    call    sort_idx_by_u64_desc

.epilogue:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
