/*
 * summa_db.h - SQLite database support for Summa
 */

#ifndef SUMMA_DB_H
#define SUMMA_DB_H

#include <stdbool.h>
#include <sqlite3.h>
#include "summa.h"
#include "summa_scan.h"

/* Database version for schema migrations */
#define DB_VERSION 1

/* Default database path */
#define DEFAULT_DB_PATH "~/.summa/summa.db"

/* Database connection handle */
typedef struct {
    sqlite3 *db;
    char *path;
    bool in_transaction;
} summa_db_t;

/* Statistics structure */
typedef struct {
    int total_entries;
    int total_files;
    int total_tags;
    int total_minutes;
    date_t earliest_date;
    date_t latest_date;
} db_stats_t;

/* Query options */
typedef struct {
    date_t from_date;
    date_t to_date;
    char *tag;
    char *file_pattern;
    char *description_pattern;
    int limit;
    int offset;
} query_options_t;

/* Database initialization and management */
summa_db_t* db_open(const char *path);
void db_close(summa_db_t *db);
bool db_init_schema(summa_db_t *db);
bool db_check_schema(summa_db_t *db);
bool db_migrate_schema(summa_db_t *db, int from_version);

/* Transaction management */
bool db_begin_transaction(summa_db_t *db);
bool db_commit_transaction(summa_db_t *db);
bool db_rollback_transaction(summa_db_t *db);

/* Import operations */
bool db_import_file(summa_db_t *db, const char *filepath, logfile_t *logfile);
bool db_import_entry(summa_db_t *db, const char *filepath, logline_t *entry);
bool db_import_scan_results(summa_db_t *db, scan_result_t *results);

/* Query operations */
logfile_t* db_query_entries(summa_db_t *db, query_options_t *options);
logfile_t* db_query_by_date_range(summa_db_t *db, date_t from, date_t to);
logfile_t* db_query_by_tag(summa_db_t *db, const char *tag);
logfile_t* db_query_by_file(summa_db_t *db, const char *filepath);

/* Statistics and aggregation */
db_stats_t* db_get_stats(summa_db_t *db);
/* tag_summary_t* db_get_tag_summary(summa_db_t *db, query_options_t *options); */
logfile_t* db_get_daily_summary(summa_db_t *db, query_options_t *options);
logfile_t* db_get_weekly_summary(summa_db_t *db, query_options_t *options);
logfile_t* db_get_monthly_summary(summa_db_t *db, query_options_t *options);

/* Cache operations for scan results */
bool db_cache_file_scan(summa_db_t *db, const char *filepath,
                        time_t mtime, int entry_count);
bool db_is_file_cached(summa_db_t *db, const char *filepath, time_t mtime);
bool db_clear_cache(summa_db_t *db);

/* Export operations */
bool db_export_to_file(summa_db_t *db, const char *filepath,
                      query_options_t *options);

/* Utility functions */
char* db_expand_path(const char *path);
bool db_vacuum(summa_db_t *db);
bool db_backup(summa_db_t *db, const char *backup_path);

#endif /* SUMMA_DB_H */