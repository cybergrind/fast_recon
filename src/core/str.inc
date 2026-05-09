; str.inc — string and memory primitives.
;
; Conventions: SysV AMD64 register-passing.
;   args:   rdi, rsi, rdx, rcx, r8, r9
;   return: rax
;   callee-saved: rbx, rbp, r12-r15
;
; Procs in this file are only allowed to clobber: rax, rcx, rdx, rsi, rdi, r8-r11.

segment readable executable

; memcpy(rdi=dst, rsi=src, rdx=len) -> rax = dst (mimic libc).
; Forward copy; caller must ensure regions don't overlap dst > src by < len.
memcpy:
    mov     rax, rdi                 ; preserve return value
    mov     rcx, rdx
    rep     movsb
    ret

; memset(rdi=dst, esi=byte, rdx=len) -> rax = dst.
memset:
    mov     r8, rdi                  ; save original dst for return
    mov     rcx, rdx                 ; len
    mov     al, sil                  ; byte to store
    rep     stosb                    ; rep stosb advances rdi
    mov     rax, r8
    ret

; memcmp(rdi=a, rsi=b, rdx=len) -> rax: signed difference (a[i]-b[i]) at first
; mismatch byte, or 0 if all len bytes match. len may be 0.
memcmp:
    xor     ecx, ecx                 ; index
.loop:
    cmp     rcx, rdx
    jae     .equal
    movzx   eax, byte [rdi + rcx]
    movzx   r8d, byte [rsi + rcx]
    sub     eax, r8d
    jne     .done                    ; rax already holds signed diff
    inc     rcx
    jmp     .loop
.equal:
    xor     eax, eax
.done:
    movsxd  rax, eax
    ret

; strlen(rdi=ptr) -> rax = bytes scanned before first NUL byte
strlen:
    xor     eax, eax
.loop:
    cmp     byte [rdi + rax], 0
    je      .done
    inc     rax
    jmp     .loop
.done:
    ret
