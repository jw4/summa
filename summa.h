/*
 * summa.h - Header file for summa core functionality
 */

#ifndef SUMMA_H
#define SUMMA_H

#include <stdio.h>
#include <stdbool.h>

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
    char *raw_line;
} logline_t;

/* Log file */
typedef struct logfile {
    logline_t **entries;
    int count;
    int capacity;
} logfile_t;

/* Global variables (declared extern) */
extern date_t current_date;
extern logfile_t *current_logfile;
extern bool verbose;

/* Core functions */
logfile_t* create_logfile(void);
void free_logfile(logfile_t *file);
int parse_two_phase(FILE *input);

/* Filter variables */
extern date_t filter_from;
extern date_t filter_to;
extern char *filter_tag;

#endif /* SUMMA_H */