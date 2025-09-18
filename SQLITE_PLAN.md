# SQLite Storage Implementation Plan for Summa

## Executive Summary

This document outlines a comprehensive plan for adding SQLite database support to Summa, enabling persistent storage, advanced querying, and multi-file data management while maintaining backward compatibility with the existing text-based workflow.

## Goals

1. **Persistent Storage**: Store parsed time entries in a local SQLite database
2. **Advanced Querying**: Enable SQL-based analysis beyond current filtering
3. **Multi-file Management**: Track entries from multiple log files
4. **Performance**: Handle datasets with 100,000+ entries efficiently
5. **Backward Compatibility**: Maintain all existing functionality
6. **Data Integrity**: Ensure no data loss during import/export

## Database Schema Design

### Core Tables

```sql
-- Sources table: Track where entries come from
CREATE TABLE sources (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    filename TEXT NOT NULL,
    filepath TEXT,
    import_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_modified TIMESTAMP,
    file_hash TEXT,
    UNIQUE(filepath, file_hash)
);

-- Entries table: Main time log entries
CREATE TABLE entries (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id INTEGER,
    date DATE NOT NULL,
    start_time TIME NOT NULL,
    end_time TIME NOT NULL,
    duration_minutes INTEGER NOT NULL,
    description TEXT,
    percentage INTEGER CHECK(percentage >= 0 AND percentage <= 100),
    line_number INTEGER,
    raw_line TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(source_id) REFERENCES sources(id) ON DELETE CASCADE
);

-- Tags table: Unique tags
CREATE TABLE tags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Entry-tag junction table (many-to-many)
CREATE TABLE entry_tags (
    entry_id INTEGER NOT NULL,
    tag_id INTEGER NOT NULL,
    PRIMARY KEY(entry_id, tag_id),
    FOREIGN KEY(entry_id) REFERENCES entries(id) ON DELETE CASCADE,
    FOREIGN KEY(tag_id) REFERENCES tags(id) ON DELETE CASCADE
);

-- Indexes for performance
CREATE INDEX idx_entries_date ON entries(date);
CREATE INDEX idx_entries_source ON entries(source_id);
CREATE INDEX idx_entries_duration ON entries(duration_minutes);
CREATE INDEX idx_tags_name ON tags(name);
CREATE INDEX idx_entry_tags_entry ON entry_tags(entry_id);
CREATE INDEX idx_entry_tags_tag ON entry_tags(tag_id);

-- View for simplified querying
CREATE VIEW entry_view AS
SELECT
    e.id,
    e.date,
    e.start_time,
    e.end_time,
    e.duration_minutes,
    e.description,
    e.percentage,
    s.filename,
    s.filepath,
    GROUP_CONCAT(t.name) as tags
FROM entries e
LEFT JOIN sources s ON e.source_id = s.id
LEFT JOIN entry_tags et ON e.id = et.entry_id
LEFT JOIN tags t ON et.tag_id = t.id
GROUP BY e.id;
```

### Additional Tables for Advanced Features

```sql
-- Projects table (future enhancement)
CREATE TABLE projects (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,
    description TEXT,
    hourly_rate DECIMAL(10,2),
    currency TEXT DEFAULT 'USD',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Entry-project mapping
CREATE TABLE entry_projects (
    entry_id INTEGER NOT NULL,
    project_id INTEGER NOT NULL,
    PRIMARY KEY(entry_id, project_id),
    FOREIGN KEY(entry_id) REFERENCES entries(id) ON DELETE CASCADE,
    FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE
);

-- Statistics cache table
CREATE TABLE statistics_cache (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    cache_key TEXT NOT NULL UNIQUE,
    cache_value TEXT,
    computed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP
);
```

## Implementation Phases

### Phase 1: Core Database Operations (Week 1)

#### 1.1 Database Module (`summa_db.c`)
```c
typedef struct {
    sqlite3 *db;
    char *db_path;
    bool in_transaction;
} summa_db_t;

// Core functions
summa_db_t* db_open(const char *path);
void db_close(summa_db_t *db);
int db_init_schema(summa_db_t *db);
int db_begin_transaction(summa_db_t *db);
int db_commit(summa_db_t *db);
int db_rollback(summa_db_t *db);
```

#### 1.2 Entry Operations
```c
// CRUD operations
int db_insert_entry(summa_db_t *db, logline_t *entry, int source_id);
int db_update_entry(summa_db_t *db, int entry_id, logline_t *entry);
int db_delete_entry(summa_db_t *db, int entry_id);
logline_t* db_get_entry(summa_db_t *db, int entry_id);

// Bulk operations
int db_insert_entries_bulk(summa_db_t *db, logfile_t *file, int source_id);
logfile_t* db_query_entries(summa_db_t *db, const char *sql);
```

#### 1.3 Tag Management
```c
int db_ensure_tag(summa_db_t *db, const char *tag_name);
int db_link_entry_tag(summa_db_t *db, int entry_id, int tag_id);
taglist_t* db_get_all_tags(summa_db_t *db);
```

### Phase 2: Import/Export Pipeline (Week 2)

#### 2.1 Import Functions
```c
typedef struct {
    bool skip_duplicates;
    bool update_existing;
    bool preserve_ids;
    void (*progress_callback)(int current, int total);
} import_options_t;

int db_import_file(summa_db_t *db, const char *filepath, import_options_t *opts);
int db_import_directory(summa_db_t *db, const char *dirpath, import_options_t *opts);
```

#### 2.2 Export Functions
```c
typedef enum {
    EXPORT_FORMAT_MARKDOWN,
    EXPORT_FORMAT_CSV,
    EXPORT_FORMAT_JSON,
    EXPORT_FORMAT_SQL
} export_format_t;

int db_export(summa_db_t *db, const char *filepath, export_format_t format,
              const char *filter_sql);
```

#### 2.3 Sync Mechanism
```c
typedef struct {
    int entries_added;
    int entries_updated;
    int entries_deleted;
    int errors;
} sync_stats_t;

sync_stats_t* db_sync_file(summa_db_t *db, const char *filepath);
```

### Phase 3: Query Interface (Week 3)

#### 3.1 Query Builder
```c
typedef struct {
    char *select_clause;
    char *from_clause;
    char *where_clause;
    char *group_clause;
    char *order_clause;
    int limit;
    int offset;
} query_builder_t;

query_builder_t* qb_new(void);
void qb_select(query_builder_t *qb, const char *fields);
void qb_where_date_range(query_builder_t *qb, date_t start, date_t end);
void qb_where_tag(query_builder_t *qb, const char *tag);
void qb_where_duration_min(query_builder_t *qb, int minutes);
char* qb_build(query_builder_t *qb);
```

#### 3.2 Predefined Queries
```c
// Common queries
logfile_t* db_query_by_date_range(summa_db_t *db, date_t start, date_t end);
logfile_t* db_query_by_tag(summa_db_t *db, const char *tag);
logfile_t* db_query_by_project(summa_db_t *db, const char *project);

// Aggregate queries
typedef struct {
    char *key;
    int total_minutes;
    int entry_count;
} aggregate_result_t;

aggregate_result_t* db_aggregate_by_tag(summa_db_t *db);
aggregate_result_t* db_aggregate_by_day(summa_db_t *db);
aggregate_result_t* db_aggregate_by_week(summa_db_t *db);
aggregate_result_t* db_aggregate_by_month(summa_db_t *db);
```

### Phase 4: Command-Line Integration (Week 4)

#### 4.1 New Command-Line Options
```
--db PATH           Use SQLite database at PATH
--import FILE       Import file into database
--import-dir DIR    Import all files from directory
--sync              Sync existing files with database
--query SQL         Execute custom SQL query
--cache             Enable query result caching
--no-db             Disable database (use file mode only)
```

#### 4.2 Database Commands
```
summa db init              Initialize new database
summa db import FILE       Import file into database
summa db export FORMAT     Export database to format
summa db stats             Show database statistics
summa db vacuum            Optimize database
summa db check             Check database integrity
```

#### 4.3 Configuration File
```ini
# ~/.summarc
[database]
path = ~/.summa/summa.db
auto_import = true
cache_queries = true
cache_ttl = 3600

[import]
skip_duplicates = true
update_existing = false

[export]
default_format = markdown
include_raw_lines = false
```

## Migration Strategy

### Step 1: Parallel Mode
- Keep existing file-based parsing
- Add optional database storage
- Database serves as cache/index initially

### Step 2: Hybrid Mode
- File parsing feeds into database
- Queries can use either source
- Gradual migration of features

### Step 3: Database-First Mode
- Database becomes primary storage
- Files become import sources
- Advanced features require database

## Performance Considerations

### 1. Bulk Operations
- Use prepared statements
- Batch inserts in transactions
- Implement progress callbacks

### 2. Query Optimization
- Create appropriate indexes
- Use EXPLAIN QUERY PLAN
- Implement query result caching

### 3. Memory Management
- Stream large result sets
- Limit default query results
- Implement pagination

### 4. Database Maintenance
- Auto-vacuum configuration
- Periodic ANALYZE commands
- Index statistics updates

## Testing Strategy

### 1. Unit Tests
```c
// Test database operations
void test_db_init(void);
void test_db_insert_entry(void);
void test_db_query_entries(void);
void test_db_transactions(void);
```

### 2. Integration Tests
```bash
# Test import/export cycle
test_import_export_consistency()
test_duplicate_detection()
test_large_file_import()
test_concurrent_access()
```

### 3. Performance Tests
```bash
# Benchmark operations
benchmark_import_speed()    # Target: 10,000 entries/second
benchmark_query_speed()     # Target: <100ms for complex queries
benchmark_memory_usage()    # Target: <50MB for 100k entries
```

### 4. Migration Tests
```bash
# Test data integrity
test_file_to_db_migration()
test_db_to_file_export()
test_version_compatibility()
```

## Error Handling

### 1. Database Errors
- Connection failures
- Lock timeouts
- Constraint violations
- Disk space issues

### 2. Data Errors
- Invalid entries
- Duplicate detection
- Encoding issues
- Corrupted data

### 3. Recovery Mechanisms
- Automatic backup before migrations
- Transaction rollback on errors
- Database repair tools
- Export to text fallback

## Security Considerations

### 1. Database Security
- File permissions (0600)
- Optional encryption (SQLCipher)
- SQL injection prevention
- Input sanitization

### 2. Privacy
- No telemetry
- Local storage only
- Configurable data retention
- Secure deletion options

## Future Enhancements

### 1. Advanced Analytics
- Machine learning for patterns
- Predictive time estimates
- Anomaly detection
- Productivity scoring

### 2. Collaboration Features
- Multi-user support
- Database syncing
- Conflict resolution
- Change tracking

### 3. Integration APIs
- REST API server mode
- Webhook support
- Third-party plugins
- Cloud backup

## Implementation Timeline

| Week | Phase | Deliverables |
|------|-------|------------|
| 1 | Core Database | Database module, schema, basic CRUD |
| 2 | Import/Export | File import, export formats, sync |
| 3 | Query Interface | Query builder, predefined queries |
| 4 | CLI Integration | New options, commands, config |
| 5 | Testing | Unit tests, integration tests |
| 6 | Documentation | User guide, migration guide |

## Dependencies

### Required Libraries
- SQLite 3.32.0+ (for generated columns)
- Optional: SQLCipher for encryption

### Build Changes
```makefile
# Add to Makefile
CFLAGS += -lsqlite3
SRCS += summa_db.c

# Optional encryption
ifdef USE_SQLCIPHER
    CFLAGS += -DUSE_SQLCIPHER -lsqlcipher
endif
```

## Backward Compatibility

### Guarantees
1. All existing commands continue to work
2. Text file parsing unchanged
3. Output formats preserved
4. No database required for basic usage

### Breaking Changes
- None in Phase 1-3
- Phase 4 may require --no-db flag for pure file mode

## Success Metrics

### Functionality
- ✓ All existing tests pass
- ✓ Database operations have 95%+ test coverage
- ✓ Import preserves 100% of data

### Performance
- ✓ Import speed ≥10,000 entries/second
- ✓ Query response <100ms for 100k entries
- ✓ Memory usage <50MB for typical usage

### Usability
- ✓ Setup requires ≤1 command
- ✓ Migration automated with progress
- ✓ Documentation covers all use cases

## Risk Mitigation

### Technical Risks
| Risk | Mitigation |
|------|-----------|
| Database corruption | Automatic backups, integrity checks |
| Performance degradation | Indexing, query optimization, caching |
| Migration failures | Rollback support, validation |
| Platform compatibility | Use standard SQLite, test on all platforms |

### User Experience Risks
| Risk | Mitigation |
|------|-----------|
| Complexity increase | Keep simple mode default |
| Data loss | Never modify source files |
| Learning curve | Comprehensive examples |
| Breaking workflows | Full backward compatibility |

## Conclusion

This plan provides a robust foundation for adding SQLite storage to Summa while maintaining its simplicity and reliability. The phased approach ensures each component is thoroughly tested before moving forward, and the emphasis on backward compatibility ensures no disruption to existing users.

The implementation will transform Summa from a simple parser into a comprehensive time tracking system capable of handling large datasets with advanced querying capabilities, while still maintaining the simplicity that makes it useful for quick command-line usage.