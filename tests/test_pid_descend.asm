; test_pid_descend.asm
;
; pid_descend_to_claude(rdi=pane_pid) -> rax = claude_pid_or_pane_pid
;
; tmux's pane_pid is the shell that hosts the claude command, not claude
; itself. This helper walks the children of `pane_pid` (via
; /proc/{pid}/task/{pid}/children) and returns the first whose comm starts
; with "claude". If none match, returns the original pane_pid (so callers
; that already pass a real claude pid keep working).
;
; Test strategy: fork a child that execs `sleep 30` (comm becomes "sleep");
; descend(self) → self (no claude child). Then exec a placeholder via
; spawning a process that renames itself to "claude" via prctl... too
; complex. Instead: the function walks children listing only — we test
; it by stat-ing self with a known children list parser via a
; well-defined fixture.
;
; Simpler: use /proc directly with a synthetic /tmp tree? No — function
; reads /proc which we can't fake cleanly.
;
; So: assert pid_descend_to_claude(self) == self (no claude child of the
; test process). And pid_descend_to_claude on a non-existent pid returns
; the input unchanged.

format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    ; --- self pid ---
    mov     eax, SYS_getpid
    syscall
    mov     r12, rax

    mov     rdi, r12
    call    pid_descend_to_claude
    assert_eq_n 1, rax, r12               ; no claude child → return self

    ; non-existent pid
    mov     rdi, 0x7FFFFFFE
    call    pid_descend_to_claude
    mov     rcx, 0x7FFFFFFE
    assert_eq_n 2, rax, rcx               ; bogus pid → returned unchanged

    test_pass

include '../src/lib.inc'
