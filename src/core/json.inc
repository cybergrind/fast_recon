; json.inc — minimal JSON scanner.
;
; Designed for the well-formed records produced by Claude Code (JSONL).
; Not a validator. Behavior on malformed input: scan stops at end-of-buffer
; and the caller will see an unmatched value range.
;
; All entry points take a half-open [p, end) byte range and return rax = new p.


; json_skip_ws(rdi=p, rsi=end) -> rax = new_p
; ASCII whitespace: space, tab, CR, LF.
json_skip_ws:
    mov     rax, rdi
.loop:
    cmp     rax, rsi
    jae     .done
    movzx   ecx, byte [rax]
    cmp     cl, ' '
    je      .step
    cmp     cl, 0x09
    je      .step
    cmp     cl, 0x0A
    je      .step
    cmp     cl, 0x0D
    je      .step
    jmp     .done
.step:
    inc     rax
    jmp     .loop
.done:
    ret

; json_skip_string(rdi=p, rsi=end) -> rax = new_p
; Pre: byte at p is opening '"'. Returns one-past-closing-quote, or end.
json_skip_string:
    mov     rax, rdi
    cmp     rax, rsi
    jae     .done
    inc     rax                      ; skip opening quote
.loop:
    cmp     rax, rsi
    jae     .done
    movzx   ecx, byte [rax]
    cmp     cl, '"'
    je      .closed
    cmp     cl, '\'
    jne     .step
    ; escape: skip the escape byte and the next byte unconditionally
    add     rax, 2
    jmp     .loop
.step:
    inc     rax
    jmp     .loop
.closed:
    inc     rax
.done:
    ret

; json_skip_value(rdi=p, rsi=end) -> rax = new_p
; Skips one value. For strings: delegates to json_skip_string.
; For objects/arrays: counts braces, ignoring those inside strings.
; For numbers and literals: consumes [-+0-9a-zA-Z._eE].
json_skip_value:
    mov     rax, rdi
    cmp     rax, rsi
    jae     .done
    movzx   ecx, byte [rax]
    cmp     cl, '"'
    je      .str
    cmp     cl, '{'
    je      .nested
    cmp     cl, '['
    je      .nested
    ; literal / number: consume byte while it's in our continuation set
.lit_loop:
    cmp     rax, rsi
    jae     .done
    movzx   ecx, byte [rax]
    ; alphanumeric, '-', '+', '.', '_'
    cmp     cl, '0'
    jb      .not_digit
    cmp     cl, '9'
    jbe     .lit_step
.not_digit:
    cmp     cl, 'A'
    jb      .not_upper
    cmp     cl, 'Z'
    jbe     .lit_step
.not_upper:
    cmp     cl, 'a'
    jb      .not_lower
    cmp     cl, 'z'
    jbe     .lit_step
.not_lower:
    cmp     cl, '-'
    je      .lit_step
    cmp     cl, '+'
    je      .lit_step
    cmp     cl, '.'
    je      .lit_step
    cmp     cl, '_'
    je      .lit_step
    jmp     .done
.lit_step:
    inc     rax
    jmp     .lit_loop

.str:
    mov     rdi, rax
    call    json_skip_string         ; preserves rsi
    ret

.nested:
    ; Brace/bracket-balanced skip. Use rdx as depth counter.
    push    rbx
    xor     edx, edx                 ; depth
.n_loop:
    cmp     rax, rsi
    jae     .n_done
    movzx   ecx, byte [rax]
    cmp     cl, '"'
    jne     .n_check_open
    ; string literal — delegate
    mov     rdi, rax
    push    rdx
    call    json_skip_string
    pop     rdx
    jmp     .n_loop
.n_check_open:
    cmp     cl, '{'
    je      .n_inc
    cmp     cl, '['
    je      .n_inc
    cmp     cl, '}'
    je      .n_dec
    cmp     cl, ']'
    je      .n_dec
    inc     rax
    jmp     .n_loop
.n_inc:
    inc     rdx
    inc     rax
    jmp     .n_loop
.n_dec:
    dec     rdx
    inc     rax
    test    rdx, rdx
    jnz     .n_loop
.n_done:
    pop     rbx
    ret

.done:
    ret

; json_find_key(rdi=p, rsi=end, rdx=key_ptr, rcx=key_len)
;   Pre: p points at '{' that begins the object to search (depth-1 keys only).
;   On hit:  rax = value_start (post-colon, ws-skipped),
;            rdx = value_end   (one-past-end of value),
;            rcx = 0
;   On miss: rax = 0, rdx = 0, rcx = -1
json_find_key:
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    mov     r12, rsi                 ; end
    mov     r13, rdx                 ; key_ptr
    mov     r14, rcx                 ; key_len
    mov     rax, rdi
    cmp     rax, r12
    jae     .miss
    movzx   ecx, byte [rax]
    cmp     cl, '{'
    jne     .miss
    inc     rax                      ; past '{'
.loop:
    mov     rdi, rax
    mov     rsi, r12
    call    json_skip_ws
    cmp     rax, r12
    jae     .miss
    movzx   ecx, byte [rax]
    cmp     cl, '}'
    je      .miss                    ; empty / exhausted
    cmp     cl, '"'
    jne     .miss                    ; malformed
    ; key starts at rax+1, find closing quote
    mov     rbx, rax                 ; quote position
    mov     rdi, rax
    mov     rsi, r12
    call    json_skip_string         ; rax now one past closing quote
    ; key span: [rbx+1 .. rax-1)
    lea     rdi, [rbx + 1]           ; key start
    mov     rcx, rax
    sub     rcx, rdi
    dec     rcx                      ; subtract trailing quote
    ; compare lengths
    cmp     rcx, r14
    jne     .skip_value
    ; memcmp(rdi, r13, r14)
    push    rax                      ; preserve cursor
    mov     rsi, r13
    mov     rdx, r14
    call    memcmp
    mov     rcx, rax                 ; diff
    pop     rax                      ; cursor restored
    test    rcx, rcx
    jnz     .skip_value
    ; matched key — skip ws + ':' + ws → value start
    mov     rdi, rax
    mov     rsi, r12
    call    json_skip_ws
    cmp     rax, r12
    jae     .miss
    movzx   ecx, byte [rax]
    cmp     cl, ':'
    jne     .miss
    inc     rax
    mov     rdi, rax
    mov     rsi, r12
    call    json_skip_ws
    mov     r15, rax                 ; value_start
    mov     rdi, rax
    mov     rsi, r12
    call    json_skip_value          ; rax = value_end
    mov     rdx, rax
    mov     rax, r15
    xor     ecx, ecx
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret

.skip_value:
    ; advance past ':' and value, then expect ',' or '}'
    mov     rdi, rax
    mov     rsi, r12
    call    json_skip_ws
    cmp     rax, r12
    jae     .miss
    movzx   ecx, byte [rax]
    cmp     cl, ':'
    jne     .miss
    inc     rax
    mov     rdi, rax
    mov     rsi, r12
    call    json_skip_ws
    mov     rdi, rax
    mov     rsi, r12
    call    json_skip_value
    mov     rdi, rax
    mov     rsi, r12
    call    json_skip_ws
    cmp     rax, r12
    jae     .miss
    movzx   ecx, byte [rax]
    cmp     cl, ','
    je      .next
    cmp     cl, '}'
    je      .miss
    jmp     .miss
.next:
    inc     rax
    jmp     .loop

.miss:
    xor     eax, eax
    xor     edx, edx
    mov     rcx, -1
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
