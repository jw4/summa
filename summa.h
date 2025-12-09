/*
 * summa.h - Header file for summa core functionality
 */

#ifndef SUMMA_H
#define SUMMA_H

#include <stdio.h>
#include <stdbool.h>

/* Constants for dynamic array initial capacities */
#define SUMMA_INITIAL_CAPACITY      10
#define SUMMA_SUMMARY_CAPACITY     100
#define SUMMA_LINE_BUFFER_SIZE    4096

/* Date structure */
typedef struct {
    int year;
    int month;
    int day;
} date_t;

/* Time structure */
typedef struct {
    int hour;
    int minute;
} summa_time_t;

/* Timespan structure */
typedef struct {
    summa_time_t start;
    summa_time_t end;
    int duration_minutes;
} timespan_t;

/* Tag list */
typedef struct {
    char **tags;
    int count;
    int capacity;
} taglist_t;

/* Log line entry */
typedef struct {
    date_t date;
    timespan_t timespan;
    char *description;
    int percentage;
    taglist_t *tags;
} logline_t;

/* Log file */
typedef struct logfile {
    logline_t **entries;
    int count;
    int capacity;
} logfile_t;

/* Summary structures for reporting */
typedef struct {
    char *tag;
    int total_minutes;
    int entry_count;
} tag_summary_t;

typedef struct {
    date_t date;
    int total_minutes;
    int entry_count;
} daily_summary_t;

typedef struct {
    int year;
    int week;
    int total_minutes;
    int entry_count;
    date_t first_day;
    date_t last_day;
} weekly_summary_t;

typedef struct {
    int year;
    int month;
    int total_minutes;
    int entry_count;
    int days_with_entries;
} monthly_summary_t;

/* Global variables (declared extern) */
extern date_t current_date;
extern logfile_t *current_logfile;
extern bool verbose;

/* Core functions */
logfile_t* create_logfile(void);
void free_logfile(logfile_t *file);
void add_entry(logfile_t *file, logline_t *entry);
int parse_two_phase(FILE *input);
int compare_dates(date_t *a, date_t *b);

/* Filter variables */
extern date_t filter_from;
extern date_t filter_to;
extern char *filter_tag;

#endif /* SUMMA_H */
