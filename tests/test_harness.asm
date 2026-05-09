; test_harness.asm — sanity check: assembler + run-loop + exit code path work.
format ELF64 executable 3
include '../src/common.inc'

segment readable executable
entry $
    test_pass
