# CLAUDE.md - AI Assistant Guide for Summa

## Project Overview

Summa is a fast command-line time tracking log file parser written in C. It parses time log files with entries in the format `HHMM-HHMM Description #tags` and provides summary views (daily, weekly, monthly), filtering, and multiple output formats (text, CSV, JSON).

**Key Features:**
- Time log parsing with tag-based categorization
- Directory scanning with date inference from filenames/paths
- SQLite database for persistent storage and queries
- Multiple summary views and output formats
- Comprehensive validation and error handling

## Build Commands

```bash
make              # Build the summa executable (default target)
make build        # Same as above
make test         # Build and run test suite
make debug        # Build with debugging symbols (-g -O0)
make release      # Build optimized version (-O2)
make clean        # Remove build artifacts (summa, *.o)
make install      # Install to ~/bin (or PREFIX=/path)
make uninstall    # Remove installed binary
make valgrind     # Run memory leak check (debug build required)
make man          # Preview the man page
```

## Source File Architecture

```
summa/
├── summa.c          # Main program, CLI parsing, output formatting (~1650 lines)
├── summa.h          # Core data structures (date_t, logline_t, logfile_t, taglist_t)
├── summa_scan.c     # Directory scanning and file discovery (~500 lines)
├── summa_scan.h     # Scan config, file_info_t, scan_result_t structures
├── summa_db.c       # SQLite database operations (~950 lines)
├── summa_db.h       # Database types, query options, statistics
├── summa.1          # Man page documentation
├── Makefile         # Build system
├── test_summa.sh    # Comprehensive test suite (bash)
└── testdata/        # Test fixtures
    ├── synthetic_3month.md   # Large test file (~283 entries)
    └── scan_test/            # Directory scanning test cases
```

## Code Conventions

### C Style
- **Standard:** C99 (`-std=c99`)
- **Compiler flags:** `-Wall -Wextra`
- **Naming:** snake_case for functions and variables, UPPER_CASE for macros
- **Types:** Custom typedef structs with `_t` suffix (e.g., `date_t`, `logline_t`)
- **Memory:** Manual malloc/free with NULL checks; capacity-doubling growth pattern

### Common Patterns

**Dynamic array growth:**
```c
if (list->count >= list->capacity) {
    if (list->capacity > INT_MAX / 2) {
        fprintf(stderr, "Error: capacity overflow\n");
        return;
    }
    list->capacity *= 2;
    char **new = realloc(list->items, sizeof(char*) * list->capacity);
    if (!new) { /* handle failure */ }
    list->items = new;
}
```

**NULL safety checks:**
```c
if (!ptr) return NULL;
```

**Verbose mode logging:**
```c
if (verbose) {
    fprintf(stderr, "Debug: message %s\n", value);
}
```

### Header File Structure
- Include guards: `#ifndef SUMMA_H / #define SUMMA_H / #endif`
- Include `<stdbool.h>` for bool type
- Document purpose at top of file with `/* ... */` comments
- Export only necessary functions

### Global Variables
Declared extern in headers, defined in summa.c:
- `current_logfile` - Active parsed logfile
- `current_date` - Date being processed
- `verbose` - Verbose output flag
- `filter_from`, `filter_to`, `filter_tag` - Query filters

## Key Data Structures

```c
/* Constants for initial capacities (defined in summa.h) */
#define SUMMA_INITIAL_CAPACITY      10
#define SUMMA_SUMMARY_CAPACITY     100
#define SUMMA_LINE_BUFFER_SIZE    4096

typedef struct {
    int year, month, day;
} date_t;

typedef struct {
    date_t date;
    timespan_t timespan;
    char *description;
    int percentage;
    taglist_t *tags;
} logline_t;

typedef struct logfile {
    logline_t **entries;
    int count;
    int capacity;
} logfile_t;

/* Summary structures for reporting (also in summa.h) */
typedef struct { char *tag; int total_minutes; int entry_count; } tag_summary_t;
typedef struct { date_t date; int total_minutes; int entry_count; } daily_summary_t;
typedef struct { int year; int week; int total_minutes; int entry_count; date_t first_day; date_t last_day; } weekly_summary_t;
typedef struct { int year; int month; int total_minutes; int entry_count; int days_with_entries; } monthly_summary_t;
```

## Testing

Run tests with `make test` or directly with `./test_summa.sh`.

The test suite (`test_summa.sh`) covers:
- Version/help flags
- Basic time entry parsing
- Date headers and filtering
- Tag aggregation
- Multiple output formats (text, CSV, JSON)
- Daily/weekly/monthly summaries
- Edge cases (midnight crossing, invalid dates)
- Directory scanning
- Database operations

Test format uses colored output with pass/fail indicators. Tests use stdin piping:
```bash
echo "0800-0900 test #work" | ./summa
```

## Log File Format

```markdown
# YYYY-MM-DD
HHMM-HHMM Description #tag1 #tag2
HHMM-HHMM %75 Task with effort percentage #work
```

- Date headers start with `# ` followed by ISO date
- Time entries are 24-hour format `HHMM-HHMM`
- Optional percentage `%NN` for effort tracking
- Tags are hashtags anywhere in description
- Non-matching lines are silently ignored (useful for notes)

## Database Schema

SQLite database at `~/.summa/summa.db` with tables:
- `metadata` - Key-value config storage
- `files` - Scanned file tracking (path, mtime, entry count)
- `entries` - Time entries with file_id foreign key
- `tags` - Unique tag names
- `entry_tags` - Many-to-many entry-tag relationship

## Command-Line Interface

Key options:
- `-d/-w/-m` - Daily/weekly/monthly summary
- `-f FORMAT` - Output format (text/csv/json)
- `--from DATE --to DATE` - Date range filter
- `--tag TAG` - Filter by tag
- `--sort-tags METHOD` - Sort by alpha/time/count
- `-S PATH -R` - Scan directory recursively
- `--db [PATH]` - Use SQLite database
- `--import` - Import entries to database
- `-v` - Verbose mode for debugging

## Development Tips

1. **Adding new features:** Update the header file first, implement in .c, add tests
2. **Memory leaks:** Use `make valgrind` to check for memory issues
3. **Debug output:** Use `if (verbose) fprintf(stderr, ...)` pattern
4. **Date handling:** Always validate with `compare_dates()` before use
5. **Buffer sizes:** Use `SUMMA_LINE_BUFFER_SIZE` constant, PATH_MAX for paths
6. **Error handling:** Print line numbers in validation warnings for user debugging
7. **Capacity constants:** Use `SUMMA_INITIAL_CAPACITY` and `SUMMA_SUMMARY_CAPACITY` instead of magic numbers
8. **NULL checks:** Always check malloc/calloc returns before use

## Dependencies

- C compiler (gcc or clang)
- Make
- SQLite3 library (`-lsqlite3`)
- POSIX systems (uses wordexp, dirent, stat)

## Version

Version is derived from git tags via Makefile:
```make
GIT_VERSION := $(shell git describe --abbrev=4 --dirty --always --tags 2>/dev/null || echo "unknown")
```
