; app/status_hold.inc — debounce Working→Idle transitions.
;
; The raw classifier flickers between Working and Idle as the spinner line
; scrolls during a single assistant turn. We persist the last "we saw it
; Working" timestamp per pid and report Working for HOLD_SECS after that,
; even when the latest classification says Idle.
;
; State is a small linear-probe table; pid 0 marks an empty slot. When the
; table fills, status_with_hold evicts the slot with the oldest stamp.

HOLD_SECS       = 30
HOLD_ENTRIES    = 64


; status_hold_reset() — wipe the table (used by tests and TUI startup).
status_hold_reset:
    arena_lea rdi, ARENA_OFF_HOLD_PID
    xor     esi, esi
    mov     edx, HOLD_ENTRIES * 8
    call    memset
    arena_lea rdi, ARENA_OFF_HOLD_TS
    xor     esi, esi
    mov     edx, HOLD_ENTRIES * 8
    call    memset
    ret

; status_with_hold(rdi=pid, rsi=raw_status, rdx=now_epoch) -> rax = status
;   Mutates the table:
;   - raw == STATUS_WORKING: stamp (pid, now). Inserts/updates/evicts.
;   - raw == STATUS_IDLE: if pid is in the table within HOLD_SECS of now,
;                         upgrades the result to STATUS_WORKING.
;   - any other raw value: returns raw unchanged.
status_with_hold:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    mov     r12, rdi                 ; pid
    mov     r13, rsi                 ; raw
    mov     r14, rdx                 ; now

    cmp     r13, STATUS_WORKING
    je      .stamp
    cmp     r13, STATUS_IDLE
    je      .check_hold
    ; INPUT / NEW / anything else passes through
    mov     rax, r13
    jmp     .epilogue

.check_hold:
    ; find existing entry for pid
    call    _find_slot
    test    rax, rax
    js      .pass_idle
    ; rax = slot index
    arena_lea rcx, ARENA_OFF_HOLD_TS
    mov     rcx, qword [rcx + rax*8]
    mov     rdx, r14
    sub     rdx, rcx                 ; elapsed = now - last_working_at
    cmp     rdx, HOLD_SECS
    jbe     .return_working
.pass_idle:
    mov     rax, r13                 ; STATUS_IDLE
    jmp     .epilogue
.return_working:
    mov     rax, STATUS_WORKING
    jmp     .epilogue

.stamp:
    call    _find_slot
    test    rax, rax
    jns     .stamp_existing
    ; pid not yet tracked — find a free slot, else evict oldest
    call    _find_free_or_oldest
    arena_lea rcx, ARENA_OFF_HOLD_PID
    mov     qword [rcx + rax*8], r12
.stamp_existing:
    arena_lea rcx, ARENA_OFF_HOLD_TS
    mov     qword [rcx + rax*8], r14
    mov     rax, STATUS_WORKING

.epilogue:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

; _find_slot — scan _hold_pid[] for r12. Returns index in rax, or -1 if absent.
_find_slot:
    xor     ecx, ecx
    arena_lea r11, ARENA_OFF_HOLD_PID
.lp:
    cmp     ecx, HOLD_ENTRIES
    jae     .miss
    cmp     qword [r11 + rcx*8], r12
    je      .hit
    inc     ecx
    jmp     .lp
.hit:
    mov     rax, rcx
    ret
.miss:
    mov     rax, -1
    ret

; _find_free_or_oldest — scan _hold_pid[]. Returns first slot with pid==0,
; else the slot with the smallest _hold_ts. Always returns a valid index.
_find_free_or_oldest:
    xor     ecx, ecx
    xor     r8d, r8d                 ; oldest_idx = 0
    arena_lea r11, ARENA_OFF_HOLD_PID
    arena_lea rax, ARENA_OFF_HOLD_TS
    mov     r9, qword [rax + 0]      ; oldest_ts = ts[0]
.lp:
    cmp     ecx, HOLD_ENTRIES
    jae     .done
    cmp     qword [r11 + rcx*8], 0
    je      .free
    arena_lea rdx, ARENA_OFF_HOLD_TS
    mov     r10, qword [rdx + rcx*8]
    cmp     r10, r9
    jae     .step
    mov     r9, r10
    mov     r8d, ecx
.step:
    inc     ecx
    jmp     .lp
.free:
    mov     rax, rcx
    ret
.done:
    mov     eax, r8d
    ret

