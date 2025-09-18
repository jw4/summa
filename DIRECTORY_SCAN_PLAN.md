# Directory Scanning and Auto-Discovery Plan for Summa

## Executive Summary

This document outlines a comprehensive plan for enabling Summa to automatically discover and parse time tracking data from an entire directory structure, intelligently deducing dates from filenames when date headers are absent, and handling various file formats and naming conventions.

## Goals

1. **Auto-Discovery**: Automatically find all files containing time entries in a directory tree
2. **Smart Date Inference**: Deduce dates from filenames, paths, and context
3. **Format Detection**: Support multiple file formats (.md, .txt, .log, etc.)
4. **Conflict Resolution**: Handle duplicate entries and conflicting data
5. **Performance**: Efficiently scan large directory structures
6. **Flexibility**: Support various naming conventions and organizational styles

## File Discovery Strategy

### Phase 1: File Pattern Recognition

#### 1.1 Filename Date Patterns
```c
typedef struct {
    const char *pattern;
    const char *regex;
    const char *date_format;
} filename_pattern_t;

// Common filename patterns to recognize
filename_pattern_t patterns[] = {
    // ISO date formats
    {"YYYY-MM-DD",     "([0-9]{4})-([0-9]{2})-([0-9]{2})", "%Y-%m-%d"},
    {"YYYYMMDD",       "([0-9]{4})([0-9]{2})([0-9]{2})", "%Y%m%d"},
    {"YYYY_MM_DD",     "([0-9]{4})_([0-9]{2})_([0-9]{2})", "%Y_%m_%d"},

    // Common date formats
    {"DD-MM-YYYY",     "([0-9]{2})-([0-9]{2})-([0-9]{4})", "%d-%m-%Y"},
    {"MM-DD-YYYY",     "([0-9]{2})-([0-9]{2})-([0-9]{4})", "%m-%d-%Y"},
    {"DD.MM.YYYY",     "([0-9]{2})\\.([0-9]{2})\\.([0-9]{4})", "%d.%m.%Y"},

    // Year-Month patterns
    {"YYYY-MM",        "([0-9]{4})-([0-9]{2})", "%Y-%m"},
    {"YYYYMM",         "([0-9]{4})([0-9]{2})", "%Y%m"},

    // Named months
    {"Month-YYYY",     "(January|February|March|April|May|June|July|August|September|October|November|December)[- ]([0-9]{4})", NULL},
    {"YYYY-Month",     "([0-9]{4})[- ](January|February|March|April|May|June|July|August|September|October|November|December)", NULL},

    // Week patterns
    {"YYYY-WXX",       "([0-9]{4})-W([0-9]{2})", NULL},  // ISO week
    {"week-XX-YYYY",   "week[- ]([0-9]{1,2})[- ]([0-9]{4})", NULL},

    // Relative patterns
    {"today",          "today", NULL},
    {"yesterday",      "yesterday", NULL},
    {"monday",         "(monday|tuesday|wednesday|thursday|friday|saturday|sunday)", NULL}
};
```

#### 1.2 Directory Structure Patterns
```c
// Common directory structures
typedef enum {
    STRUCT_FLAT,           // All files in one directory
    STRUCT_YEAR_MONTH,    // /YYYY/MM/files
    STRUCT_YEAR_MONTH_DAY,// /YYYY/MM/DD/files
    STRUCT_PROJECT_DATE,  // /project/YYYY-MM-DD/files
    STRUCT_DATE_PROJECT,  // /YYYY-MM-DD/project/files
    STRUCT_WEEKLY,        // /YYYY/week-XX/files
    STRUCT_CUSTOM         // User-defined structure
} dir_structure_t;

typedef struct {
    dir_structure_t type;
    char *base_path;
    char *pattern;     // Custom pattern for STRUCT_CUSTOM
    bool recursive;
    int max_depth;
} scan_config_t;
```

### Phase 2: File Content Detection

#### 2.1 Time Entry Detection
```c
typedef struct {
    bool has_time_entries;
    bool has_date_headers;
    int entry_count;
    int date_header_count;
    date_t first_date;
    date_t last_date;
    double confidence;  // 0.0 to 1.0
} file_analysis_t;

file_analysis_t* analyze_file(const char *filepath) {
    // Quick scan for time patterns
    // Look for HHMM-HHMM patterns
    // Check for date headers
    // Calculate confidence score
}

bool is_time_log_file(const char *filepath) {
    // Quick heuristic checks:
    // 1. File size (skip huge files)
    // 2. Extension (.md, .txt, .log, etc.)
    // 3. First few lines contain time patterns
    // 4. Reasonable text file (not binary)
}
```

#### 2.2 Smart Sampling
```c
typedef struct {
    int max_lines_to_check;     // Default: 100
    int min_time_entries;        // Default: 2
    double min_confidence;       // Default: 0.7
    bool check_entire_file;      // Default: false
} detection_config_t;

// Fast detection by sampling
bool quick_detect_time_file(FILE *fp, detection_config_t *config) {
    // Read first N lines
    // Check last N lines
    // Random sample from middle
    // Return true if enough time patterns found
}
```

## Date Inference Engine

### 3.1 Date Source Priority
```c
typedef enum {
    DATE_SOURCE_HEADER,      // Explicit date header in file (highest priority)
    DATE_SOURCE_INLINE,      // Date mentioned inline with entry
    DATE_SOURCE_FILENAME,    // Date extracted from filename
    DATE_SOURCE_DIRECTORY,   // Date extracted from directory path
    DATE_SOURCE_METADATA,    // File modification/creation time
    DATE_SOURCE_CONTEXT,     // Inferred from surrounding files
    DATE_SOURCE_DEFAULT      // Fallback to current date (lowest priority)
} date_source_t;

typedef struct {
    date_t date;
    date_source_t source;
    double confidence;
    char *source_string;     // Original string used for extraction
} inferred_date_t;
```

### 3.2 Inference Rules
```c
inferred_date_t* infer_date(const char *filepath, const char *line, scan_context_t *ctx) {
    // Priority order:
    // 1. Check for explicit date in line
    // 2. Check current context (last known date)
    // 3. Extract from filename
    // 4. Extract from directory path
    // 5. Check file metadata
    // 6. Look at sibling files
    // 7. Use default/current date
}

// Context tracking for better inference
typedef struct {
    date_t last_explicit_date;
    date_t last_inferred_date;
    char *current_file;
    char *current_directory;
    hash_table_t *file_date_cache;  // Cache filename->date mappings
} scan_context_t;
```

### 3.3 Filename to Date Extraction
```c
date_t extract_date_from_filename(const char *filename) {
    // Examples:
    // "2024-03-15.md" -> March 15, 2024
    // "20240315_notes.txt" -> March 15, 2024
    // "march-15-2024.log" -> March 15, 2024
    // "week12-2024.md" -> Week 12 of 2024 (use Monday)
    // "project-notes-03-15.txt" + year context -> March 15, [year]
    // "monday.md" + week context -> Monday of current week
    // "notes.txt" -> NULL (cannot extract)
}

// Advanced extraction with context
date_t extract_date_with_context(const char *filepath, scan_context_t *ctx) {
    // Use full path for more context
    // "/2024/march/15/daily.md" -> March 15, 2024
    // "/projects/2024/Q1/week10.txt" -> Week 10 of Q1 2024
    // "../yesterday/tasks.md" -> Yesterday's date
}
```

## Directory Scanning Implementation

### 4.1 Scanner Architecture
```c
typedef struct {
    char **paths;           // Directories to scan
    int path_count;
    char **exclude_patterns;// Patterns to exclude
    int exclude_count;
    char **include_patterns;// Patterns to include
    int include_count;
    bool follow_symlinks;
    int max_depth;
    size_t max_file_size;   // Skip files larger than this
    detection_config_t detection;
    void (*progress_callback)(const char *current_path, int files_found);
} scanner_config_t;

typedef struct {
    logfile_t *merged_log;
    int files_processed;
    int files_skipped;
    int entries_found;
    int dates_inferred;
    char **error_files;
    int error_count;
} scan_result_t;

scan_result_t* scan_directory(const char *path, scanner_config_t *config);
```

### 4.2 Parallel Processing
```c
// Multi-threaded scanning for large directories
typedef struct {
    pthread_t thread_id;
    char *path;
    scanner_config_t *config;
    scan_result_t *result;
} scan_job_t;

scan_result_t* parallel_scan(char **paths, int count, scanner_config_t *config) {
    // Create thread pool
    // Distribute paths among threads
    // Merge results
    // Handle conflicts
}
```

### 4.3 Incremental Scanning
```c
// Cache for avoiding re-scanning unchanged files
typedef struct {
    char *cache_path;       // ~/.summa/scan_cache.db
    sqlite3 *db;
} scan_cache_t;

typedef struct {
    char *filepath;
    time_t last_modified;
    char *file_hash;
    int entry_count;
    date_t first_date;
    date_t last_date;
} cached_file_info_t;

bool should_rescan_file(scan_cache_t *cache, const char *filepath);
void update_cache_entry(scan_cache_t *cache, const char *filepath, file_analysis_t *analysis);
```

## Conflict Resolution

### 5.1 Duplicate Entry Handling
```c
typedef enum {
    DUPLICATE_SKIP,         // Skip duplicate entries
    DUPLICATE_MERGE,        // Merge if compatible
    DUPLICATE_KEEP_FIRST,   // Keep first occurrence
    DUPLICATE_KEEP_LAST,    // Keep last occurrence
    DUPLICATE_KEEP_ALL,     // Keep all (mark as duplicates)
    DUPLICATE_INTERACTIVE   // Ask user
} duplicate_strategy_t;

typedef struct {
    logline_t *entry1;
    logline_t *entry2;
    char *file1;
    char *file2;
    int line1;
    int line2;
} duplicate_pair_t;

duplicate_pair_t* find_duplicates(logfile_t *log);
logfile_t* resolve_duplicates(logfile_t *log, duplicate_strategy_t strategy);
```

### 5.2 Date Conflict Resolution
```c
typedef struct {
    date_t header_date;      // Date from file header
    date_t filename_date;    // Date from filename
    date_t path_date;        // Date from directory path
    date_t metadata_date;    // Date from file metadata
} date_conflict_t;

date_t resolve_date_conflict(date_conflict_t *conflict, date_source_t preference);
```

## Command-Line Interface

### 6.1 New Options
```bash
# Basic directory scanning
summa --scan-dir PATH          # Scan directory for time files
summa --scan-recursive PATH    # Recursive scan
summa -S PATH                   # Short form

# Configuration options
--date-from-filename            # Enable date extraction from filenames
--date-from-path               # Enable date extraction from directory paths
--date-format FORMAT           # Specify expected date format in filenames
--exclude PATTERN              # Exclude files matching pattern
--include PATTERN              # Only include files matching pattern
--max-depth N                  # Maximum directory depth
--follow-symlinks              # Follow symbolic links

# Conflict resolution
--on-duplicate ACTION          # skip|merge|first|last|all|ask
--prefer-date-from SOURCE      # header|filename|path|metadata

# Performance options
--parallel                     # Use parallel processing
--cache                        # Cache scan results
--quick-scan                   # Fast mode (sample files)

# Output options
--group-by date|file|project  # Group results
--show-sources                 # Show where dates came from
--show-confidence              # Show confidence scores
```

### 6.2 Usage Examples
```bash
# Scan current directory recursively
summa --scan-recursive .

# Scan with date extraction from filenames
summa --scan-dir ~/logs --date-from-filename

# Scan project directories with specific structure
summa --scan-dir ~/projects --date-format "YYYY-MM-DD" --max-depth 3

# Complex scan with filters
summa --scan-recursive ~/notes \
      --include "*.md" \
      --exclude "archive/*" \
      --date-from-filename \
      --prefer-date-from filename \
      --on-duplicate merge

# Weekly report from scattered files
summa --scan-dir ~/work \
      --date-from-path \
      -w \
      --from 2024-01-01 \
      --tag meeting

# Quick scan for overview
summa --scan-dir ~/documents \
      --quick-scan \
      --show-sources \
      -m
```

## Implementation Phases

### Phase 1: Basic Directory Scanning (Week 1)
- Implement recursive file discovery
- Basic time entry detection
- Simple filename date extraction
- Single-threaded processing

### Phase 2: Smart Date Inference (Week 2)
- Complete inference engine
- Multiple date source support
- Confidence scoring
- Context tracking

### Phase 3: Advanced Features (Week 3)
- Parallel processing
- Caching system
- Duplicate detection
- Conflict resolution

### Phase 4: Integration & Polish (Week 4)
- Command-line interface
- Configuration options
- Progress reporting
- Error handling

## Testing Strategy

### 7.1 Test Directory Structures
```
test/scan_tests/
├── flat/                    # All files in one directory
│   ├── 2024-01-15.md
│   ├── 20240116_notes.txt
│   └── wednesday.log
├── structured/              # Year/Month/Day structure
│   └── 2024/
│       ├── 01/
│       │   ├── 15/daily.md
│       │   └── 16/tasks.txt
│       └── 02/
│           └── notes.md
├── mixed/                   # Mixed naming conventions
│   ├── project-a/
│   │   └── timesheet.md
│   ├── Jan-2024.txt
│   └── week03/
│       └── monday.log
└── edge_cases/             # Problematic cases
    ├── no_dates.txt
    ├── conflicting_dates.md
    └── huge_file.log
```

### 7.2 Test Cases
```c
void test_filename_date_extraction();
void test_directory_path_extraction();
void test_date_inference_priority();
void test_duplicate_detection();
void test_large_directory_scan();
void test_parallel_processing();
void test_cache_performance();
void test_conflict_resolution();
```

### 7.3 Benchmarks
```bash
# Performance targets
benchmark_scan_speed()       # Target: 1000 files/second
benchmark_inference_accuracy() # Target: 95% correct dates
benchmark_memory_usage()     # Target: O(1) memory per file
benchmark_cache_hit_rate()  # Target: 90% on re-scans
```

## Configuration File Support

### 8.1 Scanner Configuration
```ini
# ~/.summa/scan.conf
[scanner]
default_paths = ~/logs, ~/notes, ~/work
exclude_patterns = *.tmp, *.bak, .git/*, node_modules/*
include_patterns = *.md, *.txt, *.log
max_file_size = 10MB
max_depth = 5
follow_symlinks = false

[date_inference]
prefer_source = filename
filename_format = YYYY-MM-DD
directory_format = YYYY/MM/DD
use_file_metadata = true
confidence_threshold = 0.7

[duplicates]
strategy = merge
compare_precision = minute

[performance]
use_cache = true
cache_path = ~/.summa/scan_cache.db
parallel_threads = 4
quick_scan_lines = 100
```

### 8.2 Project-Specific Configuration
```ini
# project/.summa
[structure]
type = project
date_location = filename
pattern = {project}/logs/YYYY-MM-DD.md

[inference]
year_context = 2024
week_start = monday
default_date = use_file_modified

[output]
group_by = date
sort = chronological
```

## Error Handling

### 9.1 Common Errors
```c
typedef enum {
    SCAN_SUCCESS = 0,
    SCAN_ERR_PATH_NOT_FOUND,
    SCAN_ERR_PERMISSION_DENIED,
    SCAN_ERR_TOO_MANY_FILES,
    SCAN_ERR_INVALID_PATTERN,
    SCAN_ERR_CACHE_CORRUPTED,
    SCAN_ERR_OUT_OF_MEMORY
} scan_error_t;

typedef struct {
    scan_error_t code;
    char *message;
    char *filepath;
    int line_number;
} scan_error_info_t;
```

### 9.2 Recovery Strategies
- Skip unreadable files with warning
- Fall back to single-threaded on parallel failure
- Rebuild corrupted cache automatically
- Provide detailed error report at end

## Security Considerations

### 10.1 Path Traversal Prevention
```c
bool is_safe_path(const char *path) {
    // Check for ../ sequences
    // Resolve symlinks
    // Verify within allowed directories
    // Check file permissions
}
```

### 10.2 Resource Limits
```c
typedef struct {
    size_t max_memory;       // Maximum memory usage
    int max_files;          // Maximum files to process
    int max_threads;        // Maximum parallel threads
    time_t timeout;         // Maximum scan time
} resource_limits_t;
```

## Success Metrics

### Functionality
- ✓ Correctly identifies 95%+ of time log files
- ✓ Accurately infers dates 90%+ of the time
- ✓ Handles 10+ different filename formats
- ✓ Processes nested directories up to 10 levels

### Performance
- ✓ Scans 1000 files per second
- ✓ Incremental scans 10x faster with cache
- ✓ Memory usage O(1) per file
- ✓ Parallel processing 3x faster on 4 cores

### Usability
- ✓ Zero configuration for common cases
- ✓ Clear progress reporting
- ✓ Helpful error messages
- ✓ Intuitive conflict resolution

## Future Enhancements

### 11.1 Machine Learning Integration
- Train model on filename->date mappings
- Learn project-specific patterns
- Improve confidence scoring
- Anomaly detection

### 11.2 Cloud Storage Support
- Scan Dropbox folders
- Google Drive integration
- OneDrive support
- S3 bucket scanning

### 11.3 Real-time Monitoring
- Watch directories for changes
- Auto-import new files
- Live dashboard updates
- Webhook notifications

## Conclusion

This comprehensive directory scanning plan transforms Summa from a single-file parser into an intelligent time tracking aggregator that can automatically discover and process time entries across entire directory structures. The smart date inference engine ensures that even files without explicit date headers can be correctly processed, while the flexible configuration system accommodates various organizational styles and naming conventions.

The implementation maintains Summa's core philosophy of simplicity while adding powerful discovery and inference capabilities that make it practical for real-world use cases where time tracking data is scattered across multiple files and directories.