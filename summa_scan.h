/*
 * summa_scan.h - Header file for directory scanning functionality
 */

#ifndef SUMMA_SCAN_H
#define SUMMA_SCAN_H

#include <stdbool.h>
#include <stddef.h>
#include "summa.h"

/* Date source priorities */
typedef enum {
    DATE_SOURCE_HEADER,      /* From # YYYY-MM-DD in file */
    DATE_SOURCE_FILENAME,    /* From filename */
    DATE_SOURCE_PATH,        /* From directory path */
    DATE_SOURCE_METADATA,    /* From file modification time */
    DATE_SOURCE_NONE         /* No date found */
} date_source_t;

/* Scan configuration */
typedef struct {
    bool recursive;
    bool follow_symlinks;
    bool date_from_filename;
    bool date_from_path;
    bool verbose;
    int max_depth;
    size_t max_file_size;
    char **exclude_patterns;
    int exclude_count;
    char **include_patterns;
    int include_count;
} scan_config_t;

/* File info structure */
typedef struct file_info {
    char *path;
    char *filename;
    bool has_time_entries;
    bool has_date_headers;
    int entry_count;
    date_t inferred_date;
    date_source_t date_source;
    struct file_info *next;
} file_info_t;

/* Scan results */
typedef struct {
    file_info_t *files;
    int file_count;
    int entries_total;
    int files_with_dates;
    int files_without_dates;
} scan_result_t;

/* Function prototypes */
scan_result_t* scan_directory(const char *path, scan_config_t *config);
void free_scan_result(scan_result_t *result);
logfile_t* process_scan_results(scan_result_t *scan_result, scan_config_t *config);

/* Utility functions for external use */
date_t extract_date_from_filename(const char *filename);
date_t extract_date_from_path(const char *path);

#endif /* SUMMA_SCAN_H */
