; core/sort.inc — small sort helpers.


; sort_idx_by_u64_desc(rdi=keys, rsi=n, rdx=order_out)
;   Fills order_out[0..n) with a permutation of indices such that
;   keys[order[i]] is non-increasing. order_out is a u32 array.
;   O(n^2) selection sort — n stays tiny (≤ 64 panes).
sort_idx_by_u64_desc:
    push    rbx
    test    rsi, rsi
    jz      .done
    ; init order[i] = i
    xor     ecx, ecx
.init:
    cmp     rcx, rsi
    jae     .sorted
    mov     [rdx + rcx*4], ecx
    inc     rcx
    jmp     .init
.sorted:
    ; selection sort outer loop
    xor     ecx, ecx                 ; i
.outer:
    cmp     rcx, rsi
    jae     .done
    mov     r8, rcx                  ; best = i
    lea     r9, [rcx + 1]            ; j = i+1
.inner:
    cmp     r9, rsi
    jae     .swap
    movsxd  rax, dword [rdx + r9*4]
    mov     r10, qword [rdi + rax*8]
    movsxd  rax, dword [rdx + r8*4]
    mov     rbx, qword [rdi + rax*8]
    cmp     r10, rbx
    jbe     .next
    mov     r8, r9
.next:
    inc     r9
    jmp     .inner
.swap:
    cmp     r8, rcx
    je      .step
    mov     eax, dword [rdx + rcx*4]
    mov     r11d, dword [rdx + r8*4]
    mov     dword [rdx + rcx*4], r11d
    mov     dword [rdx + r8*4], eax
.step:
    inc     rcx
    jmp     .outer
.done:
    pop     rbx
    ret
