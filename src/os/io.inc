; io.inc — higher-level file I/O.
;
; struct stat (Linux x86_64) — only st_size at offset 48 matters here.
STAT_SIZE_OFFSET = 48
STAT_BUF_BYTES   = 144

segment readable executable

; read_file_all(rdi=path0)
;   ok:  rax = ptr (mmap'd MAP_PRIVATE | PROT_READ, page-aligned),
;        rdx = byte length, rcx = 0
;   err: rax = 0, rcx = -errno
;
; Strategy: open / fstat / mmap / close. The mmap holds an independent
; reference so we can close the fd immediately. Empty files (size 0) are
; reported as ptr=non-null sentinel, len=0 — but mmap can't size 0, so we
; treat that as: rax = 1 (sentinel non-null), rdx = 0.
read_file_all:
    push    r12
    push    r13
    push    r14

    ; --- open(path, O_RDONLY, 0) ---
    mov     rsi, O_RDONLY
    xor     edx, edx
    mov     eax, SYS_open
    syscall
    test    rax, rax
    js      .err_open
    mov     r12, rax                 ; fd

    ; --- fstat(fd, statbuf) ---
    sub     rsp, STAT_BUF_BYTES
    mov     rdi, r12
    mov     rsi, rsp
    mov     eax, SYS_fstat
    syscall
    test    rax, rax
    js      .err_after_open

    mov     r13, [rsp + STAT_SIZE_OFFSET]    ; size
    add     rsp, STAT_BUF_BYTES

    test    r13, r13
    jz      .empty

    ; --- mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0) ---
    xor     edi, edi                 ; addr
    mov     rsi, r13                 ; len
    mov     edx, PROT_READ
    mov     r10, MAP_PRIVATE
    mov     r8, r12                  ; fd
    xor     r9d, r9d                 ; offset
    mov     eax, SYS_mmap
    syscall
    cmp     rax, -4096
    ja      .err_after_open          ; mmap returned -errno
    mov     r14, rax

    ; close fd
    mov     rdi, r12
    mov     eax, SYS_close
    syscall

    mov     rax, r14
    mov     rdx, r13
    xor     ecx, ecx
    pop     r14
    pop     r13
    pop     r12
    ret

.empty:
    ; close fd, return ptr=1 (non-null sentinel), len=0
    mov     rdi, r12
    mov     eax, SYS_close
    syscall
    mov     eax, 1
    xor     edx, edx
    xor     ecx, ecx
    pop     r14
    pop     r13
    pop     r12
    ret

.err_after_open:
    mov     r14, rax                 ; preserve -errno
    mov     rdi, r12
    mov     eax, SYS_close
    syscall
    mov     rcx, r14
    xor     eax, eax
    xor     edx, edx
    pop     r14
    pop     r13
    pop     r12
    ret

.err_open:
    mov     rcx, rax
    xor     eax, eax
    xor     edx, edx
    pop     r14
    pop     r13
    pop     r12
    ret

; release_file(rdi=ptr, rsi=len) -> void.
; munmap that buffer; safe to call with len=0 (no-op).
release_file:
    test    rsi, rsi
    jz      .nop
    cmp     rdi, 1
    jbe     .nop                     ; reject empty-file sentinel (1)
    mov     eax, SYS_munmap
    syscall
.nop:
    ret
