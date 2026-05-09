; os/arena.inc — runtime-allocated heap for big buffers.
;
; FASM's `format ELF64 executable 3` writes most reserved-bytes (`rb N`) regions
; to the output file as zeros, inflating the binary. Big buffers are instead
; allocated with mmap(MAP_ANONYMOUS) at startup, and accessed through a pointer.
;
; Layout: one contiguous mmap chunk, sliced by fixed offsets.

ARENA_OFF_OUT_BUF      = 0
ARENA_OFF_ROW_DATA     = 65536                                   ; after 64 KB out_buf
ARENA_OFF_PCACHE       = ARENA_OFF_ROW_DATA + 64*ROW_BYTES       ; per-pid stable-fields cache
ARENA_OFF_ENV_BUF      = ARENA_OFF_PCACHE   + 64*PCACHE_BYTES
ARENA_OFF_DENT_BUF     = ARENA_OFF_ENV_BUF + 8192
ARENA_OFF_IW_DENT_BUF  = ARENA_OFF_DENT_BUF + 8192
ARENA_OFF_CPS_OUT      = ARENA_OFF_IW_DENT_BUF + 8192
ARENA_BYTES            = ARENA_OFF_CPS_OUT + 16384

segment readable executable

; arena_init() -> rax = 0/-errno.
; mmap a single anonymous read-write region big enough for every "big buffer",
; store base in `arena_base`. Idempotent.
arena_init:
    cmp     qword [arena_base], 0
    jne     .ok
    xor     edi, edi                 ; addr = NULL
    mov     rsi, ARENA_BYTES
    mov     edx, PROT_READ or PROT_WRITE
    mov     r10, MAP_PRIVATE or MAP_ANON
    mov     r8, -1                   ; fd
    xor     r9d, r9d                 ; offset
    mov     eax, SYS_mmap
    syscall
    cmp     rax, -4096
    ja      .err
    mov     qword [arena_base], rax
.ok:
    xor     eax, eax
    ret
.err:
    ret

; arena_addr(rdi=offset) -> rax = base + offset
arena_addr:
    mov     rax, qword [arena_base]
    add     rax, rdi
    ret

segment readable writeable
align 8
arena_base rq 1
