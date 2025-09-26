/*
 * summa_scan.c - Directory scanning and file discovery for Summa
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <regex.h>
#include "summa.h"
#include "summa_scan.h"

/* Maximum path length */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* File size limits */
#define MAX_FILE_SIZE (10 * 1024 * 1024)  /* 10MB */
#define SAMPLE_LINES 50                    /* Lines to check for time entries */

/* Type definitions are in summa_scan.h */

/* Function prototypes */
static bool is_text_file(const char *path);
static bool has_time_entries(const char *path, int *count, bool *has_dates);
static bool should_process_file(const char *path, scan_config_t *config);
/* These are exported in summa_scan.h */
static void scan_directory_recursive(const char *path, scan_result_t *result,
                                    scan_config_t *config, int depth);
static file_info_t* analyze_file(const char *path, scan_config_t *config);
static bool validate_path(const char *path, char *validated_path, size_t max_len);

/* Check if file is likely a text file */
static bool is_text_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;

    /* Check first 512 bytes for binary data */
    unsigned char buffer[512];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), fp);
    fclose(fp);

    if (bytes_read == 0) return false;

    /* Check for null bytes or other binary indicators */
    for (size_t i = 0; i < bytes_read; i++) {
        if (buffer[i] == 0) return false;  /* Null byte = binary */
        if (buffer[i] < 32 && buffer[i] != '\n' && buffer[i] != '\r' &&
            buffer[i] != '\t') {
            /* Control character that's not whitespace */
            if (buffer[i] != 27) return false;  /* Allow ESC for ANSI */
        }
    }

    return true;
}

/* Check if file contains time entries */
static bool has_time_entries(const char *path, int *count, bool *has_dates) {
    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    char line[1024];
    int lines_checked = 0;
    int entries_found = 0;
    int dates_found = 0;

    /* Compile regex for time patterns */
    regex_t time_regex;
    regex_t date_regex;
    regcomp(&time_regex, "^[0-9]{4}-[0-9]{4}", REG_EXTENDED);
    regcomp(&date_regex, "^# [0-9]{4}-[0-9]{2}-[0-9]{2}", REG_EXTENDED);

    while (fgets(line, sizeof(line), fp) && lines_checked < SAMPLE_LINES) {
        lines_checked++;

        /* Check for time entry pattern */
        if (regexec(&time_regex, line, 0, NULL, 0) == 0) {
            entries_found++;
        }

        /* Check for date header */
        if (regexec(&date_regex, line, 0, NULL, 0) == 0) {
            dates_found++;
        }
    }

    regfree(&time_regex);
    regfree(&date_regex);
    fclose(fp);

    if (count) *count = entries_found;
    if (has_dates) *has_dates = (dates_found > 0);

    /* Consider it a time log file if it has at least 1 time entry */
    return entries_found >= 1;
}

/* Check if file should be processed */
static bool should_process_file(const char *path, scan_config_t *config) {
    struct stat st;
    if (stat(path, &st) != 0) return false;

    /* Skip directories */
    if (S_ISDIR(st.st_mode)) return false;

    /* Skip symlinks if not following */
    if (S_ISLNK(st.st_mode) && !config->follow_symlinks) return false;

    /* Check file size */
    if ((size_t)st.st_size > config->max_file_size) return false;

    /* Check file extension if include patterns specified */
    if (config->include_count > 0) {
        bool matched = false;
        const char *filename = strrchr(path, '/');
        if (!filename) filename = path;
        else filename++;

        for (int i = 0; i < config->include_count; i++) {
            if (strstr(filename, config->include_patterns[i]) != NULL) {
                matched = true;
                break;
            }
        }
        if (!matched) return false;
    }

    /* Check exclude patterns */
    for (int i = 0; i < config->exclude_count; i++) {
        if (strstr(path, config->exclude_patterns[i]) != NULL) {
            return false;
        }
    }

    /* Check if it's a text file */
    if (!is_text_file(path)) return false;

    return true;
}

/* Extract date from filename using common patterns */
date_t extract_date_from_filename(const char *filename) {
    date_t date = {0, 0, 0};

    /* Try YYYY-MM-DD pattern */
    if (sscanf(filename, "%d-%d-%d", &date.year, &date.month, &date.day) == 3) {
        if (date.year >= 2000 && date.year <= 2100 &&
            date.month >= 1 && date.month <= 12 &&
            date.day >= 1 && date.day <= 31) {
            return date;
        }
    }

    /* Try YYYYMMDD pattern */
    if (sscanf(filename, "%4d%2d%2d", &date.year, &date.month, &date.day) == 3) {
        if (date.year >= 2000 && date.year <= 2100 &&
            date.month >= 1 && date.month <= 12 &&
            date.day >= 1 && date.day <= 31) {
            return date;
        }
    }

    /* Try DD-MM-YYYY pattern */
    if (sscanf(filename, "%d-%d-%d", &date.day, &date.month, &date.year) == 3) {
        if (date.year >= 2000 && date.year <= 2100 &&
            date.month >= 1 && date.month <= 12 &&
            date.day >= 1 && date.day <= 31) {
            return date;
        }
    }

    /* Return empty date if no pattern matched */
    date.year = 0;
    date.month = 0;
    date.day = 0;
    return date;
}

/* Extract date from directory path */
date_t extract_date_from_path(const char *path) {
    date_t date = {0, 0, 0};
    char *path_copy = strdup(path);
    char *token = strtok(path_copy, "/");

    while (token != NULL) {
        /* Try to extract date from each path component */
        date = extract_date_from_filename(token);
        if (date.year > 0) {
            free(path_copy);
            return date;
        }

        /* Check for year/month/day structure */
        int year = atoi(token);
        if (year >= 2000 && year <= 2100) {
            date.year = year;
            token = strtok(NULL, "/");
            if (token) {
                int month = atoi(token);
                if (month >= 1 && month <= 12) {
                    date.month = month;
                    token = strtok(NULL, "/");
                    if (token) {
                        int day = atoi(token);
                        if (day >= 1 && day <= 31) {
                            date.day = day;
                            free(path_copy);
                            return date;
                        }
                    }
                }
            }
        }

        token = strtok(NULL, "/");
    }

    free(path_copy);
    date.year = 0;
    date.month = 0;
    date.day = 0;
    return date;
}

/* Analyze a single file */
static file_info_t* analyze_file(const char *path, scan_config_t *config) {
    int entry_count = 0;
    bool has_dates = false;

    /* Check if file has time entries */
    if (!has_time_entries(path, &entry_count, &has_dates)) {
        return NULL;
    }

    /* Create file info */
    file_info_t *info = malloc(sizeof(file_info_t));
    info->path = strdup(path);

    /* Extract filename */
    const char *filename = strrchr(path, '/');
    if (filename) {
        filename++;
        info->filename = strdup(filename);
    } else {
        info->filename = strdup(path);
    }

    info->has_time_entries = true;
    info->has_date_headers = has_dates;
    info->entry_count = entry_count;
    info->next = NULL;

    /* Try to infer date if no headers found */
    if (!has_dates) {
        /* Try filename first */
        if (config->date_from_filename) {
            info->inferred_date = extract_date_from_filename(info->filename);
            if (info->inferred_date.year > 0) {
                info->date_source = DATE_SOURCE_FILENAME;
            }
        }

        /* Try path if filename didn't work */
        if (info->inferred_date.year == 0 && config->date_from_path) {
            info->inferred_date = extract_date_from_path(path);
            if (info->inferred_date.year > 0) {
                info->date_source = DATE_SOURCE_PATH;
            }
        }

        /* Fall back to file modification time */
        if (info->inferred_date.year == 0) {
            struct stat st;
            if (stat(path, &st) == 0) {
                struct tm *tm = localtime(&st.st_mtime);
                info->inferred_date.year = tm->tm_year + 1900;
                info->inferred_date.month = tm->tm_mon + 1;
                info->inferred_date.day = tm->tm_mday;
                info->date_source = DATE_SOURCE_METADATA;
            } else {
                info->date_source = DATE_SOURCE_NONE;
            }
        }
    } else {
        info->date_source = DATE_SOURCE_HEADER;
    }

    return info;
}

/* Recursively scan directory */
static void scan_directory_recursive(const char *path, scan_result_t *result,
                                    scan_config_t *config, int depth) {
    if (!config->recursive && depth > 0) return;
    if (depth > config->max_depth) return;

    DIR *dir = opendir(path);
    if (!dir) {
        if (config->verbose) {
            fprintf(stderr, "Warning: Cannot open directory %s\n", path);
        }
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Build full path */
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        /* Validate the constructed path */
        char validated_full_path[PATH_MAX];
        if (!validate_path(full_path, validated_full_path, sizeof(validated_full_path))) {
            if (config->verbose) {
                fprintf(stderr, "Warning: Skipping invalid path: %s\n", full_path);
            }
            continue;
        }

        struct stat st;
        if (stat(validated_full_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            /* Recurse into directory */
            scan_directory_recursive(validated_full_path, result, config, depth + 1);
        } else {
            /* Process file */
            if (!should_process_file(validated_full_path, config)) continue;

            file_info_t *info = analyze_file(validated_full_path, config);
            if (info) {
                /* Add to results */
                info->next = result->files;
                result->files = info;
                result->file_count++;
                result->entries_total += info->entry_count;

                if (info->has_date_headers || info->date_source != DATE_SOURCE_NONE) {
                    result->files_with_dates++;
                } else {
                    result->files_without_dates++;
                }

                if (config->verbose) {
                    printf("Found: %s (%d entries", validated_full_path, info->entry_count);
                    if (!info->has_date_headers && info->date_source != DATE_SOURCE_NONE) {
                        printf(", date from %s: %04d-%02d-%02d",
                               info->date_source == DATE_SOURCE_FILENAME ? "filename" :
                               info->date_source == DATE_SOURCE_PATH ? "path" : "metadata",
                               info->inferred_date.year,
                               info->inferred_date.month,
                               info->inferred_date.day);
                    }
                    printf(")\n");
                }
            }
        }
    }

    closedir(dir);
}

/* Validate and canonicalize a path to prevent directory traversal attacks */
static bool validate_path(const char *path, char *validated_path, size_t max_len) {
    if (!path || !validated_path || max_len == 0) return false;

    /* Use realpath to resolve symbolic links and normalize the path */
    char *canonical = realpath(path, NULL);
    if (!canonical) {
        /* Path doesn't exist or is inaccessible */
        return false;
    }

    /* Check if the canonical path fits in our buffer */
    size_t len = strlen(canonical);
    if (len >= max_len) {
        free(canonical);
        return false;
    }

    /* Check for dangerous patterns that shouldn't appear in canonical paths */
    if (strstr(canonical, "/../") != NULL ||
        strstr(canonical, "/./") != NULL ||
        (len >= 3 && strcmp(canonical + len - 3, "/..") == 0) ||
        (len >= 2 && strcmp(canonical + len - 2, "/.") == 0)) {
        free(canonical);
        return false;
    }

    /* Copy the validated path */
    strcpy(validated_path, canonical);
    free(canonical);
    return true;
}

/* Main scan function */
scan_result_t* scan_directory(const char *path, scan_config_t *config) {
    scan_result_t *result = calloc(1, sizeof(scan_result_t));

    /* Check if path exists first (preserves original error message) */
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "Error: Path does not exist: %s\n", path);
        return result;
    }

    /* Then validate and canonicalize the path for security */
    char validated_path[PATH_MAX];
    if (!validate_path(path, validated_path, sizeof(validated_path))) {
        fprintf(stderr, "Error: Invalid or unsafe path: %s\n", path);
        return result;
    }

    if (S_ISDIR(st.st_mode)) {
        /* Scan directory */
        scan_directory_recursive(validated_path, result, config, 0);
    } else {
        /* Single file */
        if (should_process_file(validated_path, config)) {
            file_info_t *info = analyze_file(validated_path, config);
            if (info) {
                result->files = info;
                result->file_count = 1;
                result->entries_total = info->entry_count;
                if (info->has_date_headers || info->date_source != DATE_SOURCE_NONE) {
                    result->files_with_dates = 1;
                } else {
                    result->files_without_dates = 1;
                }
            }
        }
    }

    return result;
}

/* Free scan results */
void free_scan_result(scan_result_t *result) {
    if (!result) return;

    file_info_t *current = result->files;
    while (current) {
        file_info_t *next = current->next;
        free(current->path);
        free(current->filename);
        free(current);
        current = next;
    }

    free(result);
}

/* Process files found during scan */
logfile_t* process_scan_results(scan_result_t *scan_result, scan_config_t *config) {
    if (!scan_result || scan_result->file_count == 0) {
        return NULL;
    }

    /* Create merged logfile */
    logfile_t *merged = create_logfile();

    /* Process each file */
    file_info_t *file = scan_result->files;
    while (file) {
        FILE *fp = fopen(file->path, "r");
        if (!fp) {
            file = file->next;
            continue;
        }

        /* Parse file using existing parser with inferred date */
        date_t saved_date = current_date;
        if (!file->has_date_headers && file->date_source != DATE_SOURCE_NONE) {
            current_date = file->inferred_date;
            if (config->verbose) {
                fprintf(stderr, "Using inferred date %04d-%02d-%02d for %s\n",
                       current_date.year, current_date.month, current_date.day,
                       file->filename);
            }
        }

        /* Parse the file into the merged logfile */
        logfile_t *saved_logfile = current_logfile;
        current_logfile = merged;

        parse_two_phase(fp);

        /* Restore globals */
        current_logfile = saved_logfile;
        current_date = saved_date;

        fclose(fp);
        file = file->next;
    }

    return merged;
}
