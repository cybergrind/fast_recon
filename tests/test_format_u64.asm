; test_format_u64.asm — format_u64(rdi=dst, rsi=value) -> rax=length

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; 0 -> "0", len 1
    lea     rdi, [outbuf]
    xor     esi, esi
    call    format_u64
    assert_eq rax, 1
    movzx   ecx, byte [outbuf]
    assert_eq rcx, '0'

    ; 7 -> "7"
    lea     rdi, [outbuf]
    mov     esi, 7
    call    format_u64
    assert_eq rax, 1
    movzx   ecx, byte [outbuf]
    assert_eq rcx, '7'

    ; 12345 -> "12345", len 5
    lea     rdi, [outbuf]
    mov     esi, 12345
    call    format_u64
    assert_eq rax, 5
    movzx   ecx, byte [outbuf + 0]
    assert_eq rcx, '1'
    movzx   ecx, byte [outbuf + 4]
    assert_eq rcx, '5'

    ; round-trip: format then parse should reconstruct
    lea     rdi, [outbuf]
    mov     rsi, 9876543210
    call    format_u64
    mov     r12, rax                 ; length
    lea     rdi, [outbuf]
    mov     rsi, r12
    call    parse_u64
    mov     rcx, 9876543210
    assert_eq rax, rcx
    assert_eq rdx, r12

    test_pass

include '../src/lib.inc'

segment readable writeable
outbuf rb 32
