# fast_recon — handoff

A pure x86-64 FASM Linux ELF that reimplements [recon](https://github.com/cybergrind/recon)'s
TUI dashboard for Claude Code tmux sessions. No libc, no external runtime —
all I/O via direct Linux syscalls.

## Status

- **22/22 unit tests passing** (`make test`)
- **Binary: ~24 KB** (mostly initialized data; `.text` is small, big buffers
  are mmap'd at startup so they aren't carried in the file)
- **TUI verified** end-to-end: spawn under tmux, capture screen, drive j/k/x,
  observe SIGTERM landing on the selected pid

## Build

```
cd fast_recon/
make            # build/fast_recon
make test       # one ELF per tests/test_*.asm; exits 0 = pass
make clean
```

Requires only `fasm` (1.73+) and a working tmux/git in `$PATH` for the runtime.

## Run

```
./build/fast_recon
```

Hotkeys:
- `j` / `k` — move selection (wraps top↔bottom)
- `x` — SIGTERM the highlighted claude pid
- `q` / `Ctrl-C` — quit (restores termios + alt-screen)

The TUI refreshes every 2 s. Sessions are sorted **descending by last
activity**, so the most recently active session is always at the top. The
selected row gets a subtle grey background that spans the full terminal
width.

## What's rendered

The 7-column table mirrors recon's exactly:

```
#  Session  Project (repo::dir::branch)  Status  Model  Context  Last Activity
```

Per-pane data flow on each refresh tick:

```
tmux list-panes -a -F "#{pane_id}|#{session_name}|#{pane_pid}|#{pane_current_command}"
   ↓ parse_claude_panes (filter cmd==claude)
   ↓ readlink /proc/{pid}/cwd                      → cwd
   ↓ git -C cwd rev-parse --show-toplevel / branch → repo · dir · branch
   ↓ pid_session_id  (~/.claude/sessions/{pid}.json)
        ↓ find_jsonl_path           ← preferred path
   OR
   ↓ find_recent_jsonl_in_cwd       ← fallback when {pid}.json missing
        (tracks "claimed" UUIDs across panes so two panes sharing a cwd don't
         collide on the same JSONL)
   ↓ mmap the JSONL, parse_jsonl_buf:
        - latest "type":"assistant" line wins
        - "<synthetic>" assistants are ignored
        - tokens = input + cache_creation + cache_read; output separately
        - timestamp + model from same record
   ↓ tmux capture-pane -t %{pane_id} -p → classify_pane_status
        ("esc to interrupt" → Working, "Esc to cancel" → Input, else Idle)
   ↓ iso8601_to_epoch + sort_idx_by_u64_desc → display order
   ↓ render with ANSI colors
```

## Layout

```
src/
  common.inc                        syscall numbers, flags, test macros
  lib.inc                           umbrella include for tests + main
  fast_recon.asm                    main + event loop + draw_row
  core/                             pure logic (no syscalls)
    str.inc                         strlen, memcmp, memcpy, memset
    num.inc                         parse_u64, format_u64
    json.inc                        json_skip_ws/string/value, json_find_key
    sort.inc                        sort_idx_by_u64_desc (selection sort, ≤64 N)
  os/                               OS resources
    sys.inc                         thin wrappers for read/write/open/close/…
    io.inc                          read_file_all (open+fstat+mmap+close)
    proc.inc                        exec_capture (pipe+fork+dup2+execve+wait4)
    term.inc                        termios raw, alt screen, ANSI helpers,
                                    poll-based key reader, term_get_cols
    proc_paths.inc                  /proc/{pid}/cwd resolution
    env.inc                         /proc/self/environ scan; home_dir()
    arena.inc                       mmap'd workspace (out_buf 64K, row_data 42K,
                                    env 8K, dent 8K, iw_dent 8K, cps_out 16K)
  app/                              claude/tmux domain
    panes.inc                       parse pipe-delimited tmux list-panes
    sessions.inc                    extract_session_id; pid_session_id
    jsonl_locate.inc                find {sid}.jsonl in projects/ (walk all dirs)
    jsonl_in_cwd.inc                fallback: latest *.jsonl in projects/{enc(cwd)}/
                                    with claimed-set exclusion + ns-resolution mtime
    jsonl_parse.inc                 model + tokens + timestamp from JSONL
    model.inc                       claude-opus-4-7 → "Opus 4.7" / 1M, etc.
    git_info.inc                    repo / branch / relative_dir via subprocess
    status.inc                      capture-pane → Working / Input / Idle / New
    timefmt.inc                     iso8601_to_epoch, format_relative
    format_tokens.inc               "Nk / NM" rendering for Context col
tests/
  run_tests.sh                      runner; one ELF per test, exit 0 = PASS
  test_*.asm                        22 standalone tests
Makefile
README.md
```

## Notable design decisions

- **Pure FASM, no link step.** Each test is its own `format ELF64 executable 3`
  binary that includes `lib.inc`. The main binary is the same.
- **Arena allocator at startup.** FASM's `format ELF64 executable 3` materializes
  `rb N` regions into the file as zeroes, so big buffers as static `bss` made
  the binary ~120 KB. Now they live in a single mmap'd anonymous region; the
  binary stays ~24 KB. See `os/arena.inc` and `arena_init`.
- **Display-vs-real index split.** `sel` and the row-loop index `ebp` are
  *display* indices; `sort_order[i]` maps them to the underlying
  `pane_recs[]` / `row_data[]` row. Kill resolves the same way.
- **Claimed-UUID set per refresh.** When two claude pids share a cwd,
  `find_recent_jsonl_in_cwd` would otherwise pick the same JSONL for both.
  `refresh_panes` clears `claimed_n` at tick start and `claim_jsonl_path`
  appends the resolved stem after each row succeeds.
- **Nanosecond mtime comparison.** Filesystem ext4/btrfs have ns-resolution
  mtimes. Comparing only `tv_sec` ties files created milliseconds apart and
  the loser is silently dropped. We compare `(tv_sec, tv_nsec)` lexicographically.
- **ANSI fg-only resets.** Mid-row `\x1B[39m` (default fg) instead of
  `\x1B[0m` (full reset) so the selection's grey background persists across
  the whole row regardless of the colours each cell emits.

## FASM gotchas hit (so the next person doesn't)

- Octal literals are `NNNo` (suffix), not `0oNNN` (prefix).
- `out` is a reserved word — name your buffer `outbuf`.
- Macro labels need `local` so multiple expansions don't redeclare them.
- Memory operands need explicit size — `mov r12, qword [recs + 8]`, not
  `mov r12, [recs + 8]`.
- Each `segment …` directive in source becomes one ELF `PT_LOAD` (FASM does
  not merge same-flag segments). Removing redundant segment switches saves
  56 bytes per directive.
- Static `dq …` rodata mixed with `rb …` reservations into the same writeable
  segment makes FASM materialize all of it. Keep big reservations in the arena
  and small initialized data in an executable segment.

## Testing rhythm

Strict red→green per public proc:

1. Write `tests/test_<thing>.asm` that calls the proc and asserts on results.
2. Run `make test`. Confirm `undefined symbol '<thing>'` (RED).
3. Implement. Run again. Confirm GREEN.

Tests requiring the arena (`test_env`, `test_pid_session`, `test_find_jsonl`,
`test_find_jsonl_excl`) call `arena_init` at entry.

The harness uses unique exit codes per assertion via the `assert_eq_n n, …`
macro so a failure tells you exactly which step broke.

## Open follow-ups (not in scope unless asked)

- **30-s git-info caching.** Each refresh shells out twice per pane to `git`.
  Recon caches per-cwd; we don't yet.
- **Smaller binary.** Remaining wins: consolidate per-module `segment` blocks
  to drop more `PT_LOAD` headers (~1 KB potential), or hand-roll a minimal
  ELF via `format binary` (could halve the file).
- **More TUI features.** No `/` search, no `v` view mode, no `i` next-input,
  no Enter-to-attach, no Ctrl-A/E/U in any input.
- **`Mon DD HH:MM` timestamps.** Currently only relative time. recon also
  shows absolute for old timestamps.
- **Status hold.** recon keeps "Working" sticky for ~5 s after the last
  generation completes to avoid flicker; we re-classify every refresh.

## Key invariants & contracts

- **`refresh_panes` is the single state-mutation point.** Everything between
  ticks is read-only against `pane_recs`, `row_data`, `sort_order`,
  `claimed_buf`.
- **Row records (`row_data[i]`) live in the mmap'd arena.** Pointers into
  these structs are valid until the next `arena_init` (only one at startup).
- **Claimed-UUID slots are 40 bytes, NUL-padded after the stem**. The match
  test in `find_recent_jsonl_in_cwd` requires the byte at `slot[stem_len]`
  to be `0` to count as a full match; that's how we avoid false-positive
  prefix matches.
- **`sort_order` is u32 indices into `pane_recs` / `row_data`**. `sel` is a
  display index into `sort_order`, not a real-row index.

## Glossary of registers used by convention here

- SysV AMD64 calling convention internally (`rdi, rsi, rdx, rcx, r8, r9`).
- Procs may freely clobber `rax, rcx, rdx, rsi, rdi, r8-r11`.
- Callee-saved (`rbx, rbp, r12-r15`) are pushed at entry, popped at exit.
- Syscalls use the kernel ABI (`rax = nr; rdi, rsi, rdx, r10, r8, r9`); our
  thin wrappers handle the `rcx → r10` shuffle when needed.

## How to pick up from here

1. `git pull && cd fast_recon/ && make test` — should be 22/22 green.
2. Read this file + `src/lib.inc` to find the include order.
3. For any new public proc: write the test first, watch it RED, implement.
4. Memory of decisions and prior session context lives at
   `~/.claude/projects/-home-kpi-devel-opensource-recon/memory/`.
