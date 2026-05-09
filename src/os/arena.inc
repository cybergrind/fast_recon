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
; Big-static slices moved off the binary so `format ELF64 executable 3`
; doesn't write zero-fill (or db-mixed segments) into the file.
ARENA_OFF_PANE_RECS    = ARENA_OFF_CPS_OUT + 16384               ; MAX_PANES * 56
ARENA_OFF_CLAIMED_BUF  = ARENA_OFF_PANE_RECS + 64*56
ARENA_OFF_JSONL_PATH   = ARENA_OFF_CLAIMED_BUF + 64*40
ARENA_OFF_IW_DIR_BUF   = ARENA_OFF_JSONL_PATH + 1024
ARENA_OFF_IW_FULL_BUF  = ARENA_OFF_IW_DIR_BUF + 1024
; Numerous small statics (≥ 64 B) follow.
ARENA_OFF_GI_BUF       = ARENA_OFF_IW_FULL_BUF + 1024             ; git_info scratch (4 KB)
ARENA_OFF_SORT_KEYS    = ARENA_OFF_GI_BUF + 4096                  ; MAX_PANES qwords
ARENA_OFF_SORT_ORDER   = ARENA_OFF_SORT_KEYS + 64*8
ARENA_OFF_ALIVE_PIDS   = ARENA_OFF_SORT_ORDER + 64*4
ARENA_OFF_UUID_BUF     = ARENA_OFF_ALIVE_PIDS + 64*4
ARENA_OFF_SCRATCH      = ARENA_OFF_UUID_BUF + 64
ARENA_OFF_PP_SCRATCH   = ARENA_OFF_SCRATCH + 64
ARENA_OFF_HDR_COLS     = ARENA_OFF_PP_SCRATCH + 64
ARENA_OFF_IW_STATBUF   = ARENA_OFF_HDR_COLS + 128
ARENA_OFF_IW_BEST_NAME = ARENA_OFF_IW_STATBUF + 144
ARENA_OFF_IW_PA_BEST   = ARENA_OFF_IW_BEST_NAME + 256
ARENA_OFF_GI_ARGV      = ARENA_OFF_IW_PA_BEST + 256               ; rq 8 (64 B)
ARENA_BYTES            = ARENA_OFF_GI_ARGV + 64


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
    ; Populate runtime pointers for the few buffers accessed via static
    ; cached addresses (see _state.inc).
    arena_lea rcx, ARENA_OFF_IW_DIR_BUF
    mov     qword [_iw_dir_buf_ptr], rcx
    arena_lea rcx, ARENA_OFF_IW_FULL_BUF
    mov     qword [_iw_full_buf_ptr], rcx
    arena_lea rcx, ARENA_OFF_GI_BUF
    mov     qword [_gi_buf_ptr], rcx
    arena_lea rcx, ARENA_OFF_GI_ARGV
    mov     qword [_gi_argv_ptr], rcx
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

