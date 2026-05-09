; test_timefmt.asm
;
; iso8601_to_epoch(rdi=str, rsi=len) -> rax = epoch_seconds (u64)
; format_relative(rdi=outbuf, rsi=cap, rdx=now_epoch, rcx=then_epoch)
;     -> rax = bytes written

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; "1970-01-01T00:00:00.000Z" -> 0
    lea     rdi, [iso_epoch]
    mov     esi, iso_epoch_len
    call    iso8601_to_epoch
    assert_eq_n 1, rax, 0

    ; "2025-01-01T00:00:00.000Z" -> 1735689600
    lea     rdi, [iso_2025]
    mov     esi, iso_2025_len
    call    iso8601_to_epoch
    mov     rcx, 1735689600
    assert_eq_n 2, rax, rcx

    ; "2024-02-29T12:34:56.000Z" — leap day, expect 1709210096
    lea     rdi, [iso_leap]
    mov     esi, iso_leap_len
    call    iso8601_to_epoch
    mov     rcx, 1709210096
    assert_eq_n 3, rax, rcx

    ; format_relative cases (now - then):
    ;   diff < 60      -> "< 1m"
    ;   diff < 3600    -> "{m}m ago"
    ;   diff < 86400   -> "{h}h ago"
    ;   else           -> "{n}d ago" (we keep it simple)

    lea     rdi, [outbuf]
    mov     esi, 32
    mov     rdx, 1000
    mov     rcx, 1000
    call    format_relative          ; diff=0 -> "< 1m"
    assert_eq_n 10, rax, 4
    lea     rdi, [outbuf]
    lea     rsi, [exp_lt1m]
    mov     edx, 4
    call    memcmp
    assert_eq_n 11, rax, 0

    lea     rdi, [outbuf]
    mov     esi, 32
    mov     rdx, 5000
    mov     rcx, 4880                ; diff = 120 -> "2m ago"
    call    format_relative
    assert_eq_n 20, rax, 6
    lea     rdi, [outbuf]
    lea     rsi, [exp_2m]
    mov     edx, 6
    call    memcmp
    assert_eq_n 21, rax, 0

    lea     rdi, [outbuf]
    mov     esi, 32
    mov     rdx, 100000
    mov     rcx, 89200               ; diff = 10800s = 3h
    call    format_relative
    assert_eq_n 30, rax, 6
    lea     rdi, [outbuf]
    lea     rsi, [exp_3h]
    mov     edx, 6
    call    memcmp
    assert_eq_n 31, rax, 0

    test_pass

include '../src/lib.inc'

segment readable
iso_epoch  db '1970-01-01T00:00:00.000Z'
iso_epoch_len = $ - iso_epoch
iso_2025   db '2025-01-01T00:00:00.000Z'
iso_2025_len = $ - iso_2025
iso_leap   db '2024-02-29T12:34:56.000Z'
iso_leap_len = $ - iso_leap

exp_lt1m   db '< 1m'
exp_2m     db '2m ago'
exp_3h     db '3h ago'

segment readable writeable
outbuf rb 32
