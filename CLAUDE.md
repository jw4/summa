# Summa

C99 time-tracking log parser. SQLite-backed, multiple output formats. See `README.md` for log format, install, and CLI usage.

## Working in this repo

- **C99** (`-std=c99`), `-Wall -Wextra`. Manual malloc/free with NULL checks. POSIX (uses wordexp, dirent, stat).
- **Naming:** snake_case for functions/vars, UPPER_CASE for macros. Typedef structs with `_t` suffix (`date_t`, `logline_t`, `taglist_t`).
- **Capacity-doubling growth** with overflow guard:
  ```c
  if (list->capacity > INT_MAX / 2) { /* error */ return; }
  list->capacity *= 2;
  ```
- **Use named constants** from `summa.h`: `SUMMA_INITIAL_CAPACITY`, `SUMMA_SUMMARY_CAPACITY`, `SUMMA_LINE_BUFFER_SIZE`, `PATH_MAX`. No magic numbers.
- **Verbose logging** pattern: `if (verbose) fprintf(stderr, "Debug: ...\n");`
- **Validation warnings should print line numbers** to help users debug their log files.

## Layout

- `summa.c` — main, CLI, output formatting (~1650 lines)
- `summa.h` — core data structures, capacity constants
- `summa_scan.c/.h` — directory scanning, file discovery, date inference
- `summa_db.c/.h` — SQLite (`~/.summa/summa.db`) — tables: `metadata`, `files`, `entries`, `tags`, `entry_tags`
- `summa.1` — man page
- `test_summa.sh` — bash test suite (run via `make test`)
- `testdata/` — fixtures including 283-entry synthetic file

## Workflow

- `make valgrind` after non-trivial allocations (requires `make debug` build first)
- Tests use stdin piping: `echo "0800-0900 test #work" | ./summa`
- Version derived from `git describe --abbrev=4 --dirty --always --tags` in Makefile
