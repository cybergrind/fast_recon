# fast_recon

A reimplementation of [recon](../) in pure x86-64 FASM assembly for Linux.

## Goal

Replicate recon's functionality (TUI dashboard for Claude Code tmux sessions)
as a tiny statically-linked ELF64 binary written entirely in flat assembler,
with no libc dependency — all I/O via direct Linux syscalls.

## Build

```
make            # build all
make test       # run the test suite
make clean
```

## Layout

- `src/`     — library and main program sources
- `tests/`   — one `.asm` per test; each assembles to a standalone binary
               that exits 0 on pass, non-zero on fail
- `build/`   — assembler output (gitignored)

## Approach

Test-driven, bottom-up. Each module starts with a failing test, then the
implementation is written until the test passes. The dependency chain we
follow roughly mirrors recon's data flow:

1. low-level primitives (strlen, memcmp, parse_u64, ...)
2. syscall wrappers (read_file, list_dir, exec)
3. JSON / JSONL parser
4. tmux pane discovery
5. session join logic
6. TUI rendering (ANSI)
7. event loop / main
