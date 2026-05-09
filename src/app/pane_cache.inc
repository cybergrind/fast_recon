; app/pane_cache.inc — per-pid cache of the stable parts of a row.
;
; Layout: 64 slots of PCACHE_BYTES bytes, each beginning with the pid
; (qword). pid == 0 marks a free slot. Slots live in the arena
; (ARENA_OFF_PCACHE) so they aren't materialized in the binary.
;
; Cache is only useful for things that don't change tick-to-tick: cwd,
; git_info, JSONL session id and JSONL absolute path. Volatile fields
; (status, tokens, last activity) are still recomputed every refresh.

PCACHE_ENTRIES = 64


; pcache_reset() — wipe all slots (used by tests + at startup).
pcache_reset:
    push    rbx
    push    r12
    arena_lea rdi, ARENA_OFF_PCACHE
    xor     esi, esi
    mov     edx, PCACHE_ENTRIES * PCACHE_BYTES
    call    memset
    pop     r12
    pop     rbx
    ret

; pcache_lookup(rdi=pid) -> rax: slot ptr, or 0 if absent.
pcache_lookup:
    push    rbx
    push    r12
    test    rdi, rdi
    jz      .miss
    mov     rbx, rdi                 ; pid
    arena_lea r12, ARENA_OFF_PCACHE
    xor     ecx, ecx
.lp:
    cmp     ecx, PCACHE_ENTRIES
    jae     .miss
    cmp     qword [r12 + PCACHE_OFF_PID], rbx
    je      .hit
    add     r12, PCACHE_BYTES
    inc     ecx
    jmp     .lp
.hit:
    mov     rax, r12
    pop     r12
    pop     rbx
    ret
.miss:
    xor     eax, eax
    pop     r12
    pop     rbx
    ret

; pcache_alloc(rdi=pid) -> rax: slot ptr.
;   - If pid already in the table, returns the existing slot (idempotent).
;   - Else picks a free slot (pid==0). If none, evicts the slot at index 0.
;   - On a fresh allocation the slot is zeroed and PCACHE_OFF_PID set to pid.
pcache_alloc:
    push    rbx
    push    r12
    push    r13
    mov     rbx, rdi                 ; pid

    ; first, try existing
    call    pcache_lookup
    test    rax, rax
    jnz     .done

    ; scan for free slot
    arena_lea r12, ARENA_OFF_PCACHE
    xor     ecx, ecx
.scan_free:
    cmp     ecx, PCACHE_ENTRIES
    jae     .force_evict
    cmp     qword [r12 + PCACHE_OFF_PID], 0
    je      .place
    add     r12, PCACHE_BYTES
    inc     ecx
    jmp     .scan_free
.force_evict:
    arena_lea r12, ARENA_OFF_PCACHE  ; evict slot 0
.place:
    mov     rdi, r12
    xor     esi, esi
    mov     edx, PCACHE_BYTES
    call    memset
    mov     qword [r12 + PCACHE_OFF_PID], rbx
    mov     rax, r12
.done:
    pop     r13
    pop     r12
    pop     rbx
    ret

; pcache_evict_missing(rdi=alive_pids_buf, rsi=count)
;   alive_pids_buf is an array of i32 pids (matches PANE_OFF_PID storage,
;   but we accept i32 for compactness in the alive set). Any cached slot
;   whose pid is NOT in this array is wiped (pid := 0).
pcache_evict_missing:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    mov     rbp, rdi                 ; alive_buf
    mov     r13, rsi                 ; count
    arena_lea r12, ARENA_OFF_PCACHE
    xor     ebx, ebx                 ; slot index
.outer:
    cmp     ebx, PCACHE_ENTRIES
    jae     .done
    mov     r14, qword [r12 + PCACHE_OFF_PID]
    test    r14, r14
    jz      .step                    ; already free
    ; search alive_buf for r14
    xor     ecx, ecx
.inner:
    cmp     rcx, r13
    jae     .miss
    movsxd  rax, dword [rbp + rcx*4]
    cmp     rax, r14
    je      .keep
    inc     rcx
    jmp     .inner
.miss:
    mov     qword [r12 + PCACHE_OFF_PID], 0
.keep:
.step:
    add     r12, PCACHE_BYTES
    inc     ebx
    jmp     .outer
.done:
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret
