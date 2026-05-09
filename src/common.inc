; common.inc — shared definitions for fast_recon.
;
; Linux x86-64 syscall numbers, exit-code conventions, and a tiny test-helper
; macro set used by tests/*.asm. Source files include this directly.

; -- Linux x86-64 syscalls (subset we use) -------------------------------------
SYS_read          = 0
SYS_write         = 1
SYS_open          = 2
SYS_close         = 3
SYS_stat          = 4
SYS_fstat         = 5
SYS_lseek         = 8
SYS_mmap          = 9
SYS_munmap        = 11
SYS_brk           = 12
SYS_pipe          = 22
SYS_dup2          = 33
SYS_nanosleep     = 35
SYS_getpid        = 39
SYS_clone         = 56
SYS_fork          = 57
SYS_execve        = 59
SYS_exit          = 60
SYS_wait4         = 61
SYS_kill          = 62
SYS_uname         = 63
SYS_getcwd        = 79
SYS_readlink      = 89
SYS_getdents64    = 217
SYS_clock_gettime = 228
SYS_exit_group    = 231
SYS_openat        = 257
SYS_unlink        = 87
SYS_mremap        = 25
SYS_ioctl         = 16
SYS_poll          = 7

; --- termios bits we touch ---
TCGETS = 0x5401
TCSETS = 0x5402

T_BRKINT = 0x0002
T_ICRNL  = 0x0100
T_INPCK  = 0x0010
T_ISTRIP = 0x0020
T_IXON   = 0x0400

T_OPOST  = 0x0001

T_CSIZE  = 0x0030
T_PARENB = 0x0100
T_CS8    = 0x0030

T_ECHO   = 0x0008
T_ICANON = 0x0002
T_IEXTEN = 0x8000
T_ISIG   = 0x0001

; Kernel struct termios (Linux x86_64): 4×u32 + c_line + c_cc[NCCS=19] = 36 bytes.
; (glibc adds c_ispeed/c_ospeed for its userspace view, but TCGETS/TCSETS
; ioctls work with the kernel layout.)
TERMIOS_BYTES   = 36
TIO_OFF_IFLAG   = 0
TIO_OFF_OFLAG   = 4
TIO_OFF_CFLAG   = 8
TIO_OFF_LFLAG   = 12
TIO_OFF_LINE    = 16
TIO_OFF_CC      = 17
TIO_VMIN_IDX    = 6
TIO_VTIME_IDX   = 5

; signals
SIGTERM = 15
SIGKILL = 9
SIGINT  = 2

; poll
POLLIN = 1

; -- File flags ----------------------------------------------------------------
O_RDONLY   = 0
O_WRONLY   = 1
O_RDWR     = 2
O_NONBLOCK = 04000o
O_CREAT    = 0100o
O_TRUNC    = 01000o

; mmap
PROT_READ   = 1
PROT_WRITE  = 2
MAP_PRIVATE = 2
MAP_SHARED  = 1
MAP_ANON    = 0x20

; -- Exit codes for tests ------------------------------------------------------
EXIT_OK   = 0
EXIT_FAIL = 1

; -- Test macros ---------------------------------------------------------------
; assert_eq <reg64>, <expected_imm_or_reg>  -> if ne: exit EXIT_FAIL
macro assert_eq actual, expected {
    cmp     actual, expected
    je      @f
    mov     edi, EXIT_FAIL
    mov     eax, SYS_exit
    syscall
@@:
}

; assert_eq_n n, actual, expected -> on fail exit with code n (1..127)
macro assert_eq_n n, actual, expected {
    cmp     actual, expected
    je      @f
    mov     edi, n
    mov     eax, SYS_exit
    syscall
@@:
}

; assert_z <reg64> -> exit fail unless zero
macro assert_z reg {
    test    reg, reg
    jz      @f
    mov     edi, EXIT_FAIL
    mov     eax, SYS_exit
    syscall
@@:
}

; test_pass — exit 0
macro test_pass {
    xor     edi, edi
    mov     eax, SYS_exit
    syscall
}
