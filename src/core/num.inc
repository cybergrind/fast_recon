; num.inc — integer parsing and formatting.

segment readable executable

; parse_u64(rdi=ptr, rsi=len) -> rax=value, rdx=bytes consumed.
; Reads decimal digits while index < len AND byte in '0'..'9'. Stops on first
; non-digit. No overflow check; callers ensure inputs fit in 64 bits.
parse_u64:
    xor     eax, eax                 ; accumulator
    xor     edx, edx                 ; index = consumed count
.loop:
    cmp     rdx, rsi
    jae     .done
    movzx   ecx, byte [rdi + rdx]
    sub     ecx, '0'
    cmp     ecx, 9
    ja      .done                    ; unsigned compare: also catches negatives
    ; rax = rax*10 + ecx
    lea     rax, [rax + rax*4]       ; rax * 5
    add     rax, rax                 ; rax * 10
    add     rax, rcx
    inc     rdx
    jmp     .loop
.done:
    ret

; format_u64(rdi=dst, rsi=value) -> rax = bytes written.
; Writes decimal digits at dst (no NUL). Min 1 byte ("0"). Caller must provide
; >= 20 bytes. Output is left-aligned at dst.
format_u64:
    mov     rax, rsi                 ; value
    lea     r8, [_fmt_tmp_end]       ; scratch end (one past last byte)
    mov     rcx, r8                  ; write cursor (fill backward)
    mov     r10d, 10
.divloop:
    xor     edx, edx
    div     r10                      ; rax /= 10, rdx = remainder
    dec     rcx
    add     dl, '0'
    mov     [rcx], dl
    test    rax, rax
    jnz     .divloop
    ; copy [rcx .. r8) into [rdi ..)
    mov     rax, r8
    sub     rax, rcx                 ; rax = length
    push    rdi                      ; save original dst
    push    rax                      ; save length
    mov     rsi, rcx
    mov     rcx, rax
    rep     movsb
    pop     rax                      ; length
    pop     rdi
    ret

segment readable writeable
; 24-byte scratch buffer used by format_u64 (max u64 is 20 digits).
align 16
_fmt_tmp:     db 24 dup (?)
_fmt_tmp_end:
