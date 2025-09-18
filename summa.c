#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>
#include <time.h>
#include "summa.h"
#include "summa_scan.h"

/* Version information */
#ifndef VERSION
#define VERSION "unknown"
#endif

/* Type definitions are now in summa.h */

/* Tag aggregation structure */
typedef struct {
    char *tag;
    int total_minutes;
    int entry_count;
} tag_summary_t;

/* Global data */
logfile_t *current_logfile = NULL;
date_t current_date = {0, 0, 0};  /* Current date being processed */
bool verbose = false;  /* Verbose mode flag */

/* Filter options */
date_t filter_from = {0, 0, 0};
date_t filter_to = {0, 0, 0};
char* filter_tag = NULL;

/* Line classification types */
typedef enum {
    LINE_DATE,    /* Lines like "# 2024-02-06" */
    LINE_TIME,    /* Lines like "0800-0900 description #tags" */
    LINE_OTHER    /* Everything else (ignored) */
} line_type_t;

/* Output format enum */
typedef enum {
    FORMAT_TEXT,
    FORMAT_CSV,
    FORMAT_JSON
} output_format_t;

/* Function declarations */
logfile_t* create_logfile(void);
logline_t* create_logline(void);
taglist_t* create_taglist(void);
void add_tag(taglist_t *list, const char *tag);
void add_entry(logfile_t *file, logline_t *entry);
char* trim_string(char *str);
int calculate_duration(summa_time_t *start, summa_time_t *end);
void free_logfile(logfile_t *file);
void free_logline(logline_t *entry);
void free_taglist(taglist_t *list);
void print_summary(logfile_t *file);
void print_daily_summary(logfile_t *file);
void print_weekly_summary(logfile_t *file);
void print_monthly_summary(logfile_t *file);
void print_csv(logfile_t *file);
void print_json(logfile_t *file);
void print_version(const char *progname);
void print_usage(const char *progname);

/* Two-phase parsing functions */
line_type_t classify_line(const char* line);
date_t parse_date_line(const char* line, int line_number);
logline_t* parse_time_line(const char* line, int line_number);
int parse_two_phase(FILE* input);
int compare_dates(date_t *d1, date_t *d2);
bool entry_passes_filters(logline_t *entry);

/* Implementation */

/* Create a new logfile */
logfile_t* create_logfile(void) {
    logfile_t *file = malloc(sizeof(logfile_t));
    file->entries = malloc(sizeof(logline_t*) * 10);
    file->count = 0;
    file->capacity = 10;
    return file;
}

/* Create a new logline */
logline_t* create_logline(void) {
    logline_t *entry = malloc(sizeof(logline_t));
    memset(entry, 0, sizeof(logline_t));
    return entry;
}

/* Create a new taglist */
taglist_t* create_taglist(void) {
    taglist_t *list = malloc(sizeof(taglist_t));
    list->tags = malloc(sizeof(char*) * 10);
    list->count = 0;
    list->capacity = 10;
    return list;
}

/* Add a tag to the taglist */
void add_tag(taglist_t *list, const char *tag) {
    if (!list || !tag) return;

    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->tags = realloc(list->tags, sizeof(char*) * list->capacity);
    }

    list->tags[list->count++] = strdup(tag);
}

/* Add an entry to the logfile */
void add_entry(logfile_t *file, logline_t *entry) {
    if (!file || !entry) return;

    if (file->count >= file->capacity) {
        file->capacity *= 2;
        file->entries = realloc(file->entries, sizeof(logline_t*) * file->capacity);
    }

    file->entries[file->count++] = entry;
}

/* Trim whitespace from string */
char* trim_string(char *str) {
    if (!str) return NULL;

    char *trimmed = strdup(str);

    /* Trim trailing whitespace */
    int len = strlen(trimmed);
    while (len > 0 && (trimmed[len-1] == ' ' || trimmed[len-1] == '\t')) {
        trimmed[--len] = '\0';
    }

    return trimmed;
}

/* Calculate duration in minutes between two times */
int calculate_duration(summa_time_t *start, summa_time_t *end) {
    int start_minutes = start->hour * 60 + start->minute;
    int end_minutes = end->hour * 60 + end->minute;
    int duration = end_minutes - start_minutes;

    if (duration < 0) {
        /* Assume crossing midnight */
        duration += 24 * 60;

        /* Warn if span is suspiciously long (> 12 hours) */
        if (duration > 12 * 60) {
            if (verbose) {
                fprintf(stderr, "Warning: Time span %02d:%02d-%02d:%02d is %d hours (backwards span?)\n",
                       start->hour, start->minute, end->hour, end->minute, duration / 60);
            }
            /* For backwards spans that aren't midnight crossings, return negative to indicate error */
            if (duration > 20 * 60) {
                return -1;
            }
        }
    }

    return duration;
}

/* Free memory functions */
void free_logfile(logfile_t *file) {
    if (!file) return;

    for (int i = 0; i < file->count; i++) {
        free_logline(file->entries[i]);
    }
    free(file->entries);
    free(file);
}

void free_logline(logline_t *entry) {
    if (!entry) return;

    if (entry->description) free(entry->description);
    if (entry->tags) free_taglist(entry->tags);
    free(entry);
}

void free_taglist(taglist_t *list) {
    if (!list) return;

    for (int i = 0; i < list->count; i++) {
        free(list->tags[i]);
    }
    free(list->tags);
    free(list);
}

/* Print version information */
void print_version(const char *progname) {
    (void)progname; /* Suppress unused parameter warning */
    printf("summa version %s\n", VERSION);
    printf("A fast and flexible time tracking log parser\n");
    printf("Repository: https://github.com/jw4/summa\n");
}

/* Print usage information */
void print_usage(const char *progname) {
    printf("Usage: %s [OPTIONS] [FILE]\n", progname);
    printf("Parse time log files and generate summaries\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -V, --version       Show version information\n");
    printf("  -f, --format FORMAT Output format (text, csv, json) [default: text]\n");
    printf("  -t, --tags          Show tag summary\n");
    printf("  -d, --daily         Show daily summary\n");
    printf("  -w, --weekly        Show weekly summary\n");
    printf("  -m, --monthly       Show monthly summary\n");
    printf("  -v, --verbose       Verbose output\n");
    printf("  --from DATE         Filter entries from DATE (YYYY-MM-DD)\n");
    printf("  --to DATE           Filter entries to DATE (YYYY-MM-DD)\n");
    printf("  --tag TAG           Filter entries by TAG (without #)\n");
    printf("\n");
    printf("Directory scanning:\n");
    printf("  -S, --scan PATH     Scan directory for time log files\n");
    printf("  -R, --recursive     Scan directories recursively\n");
    printf("  --date-from-filename Extract dates from filenames\n");
    printf("  --date-from-path    Extract dates from directory paths\n");
    printf("  --include PATTERN   Include only files matching pattern\n");
    printf("  --exclude PATTERN   Exclude files matching pattern\n");
    printf("\n");
    printf("If FILE is omitted, reads from stdin\n");
}

/* Print text summary */
void print_summary(logfile_t *file) {
    if (!file || file->count == 0) return;

    printf("=== TIME LOG SUMMARY ===\n");
    printf("Total entries: %d\n", file->count);
    printf("\n");

    /* Calculate tag summaries */
    int summary_capacity = 100;  /* Start with space for 100 tags */
    tag_summary_t *summaries = malloc(sizeof(tag_summary_t) * summary_capacity);
    int tag_count = 0;
    int total_minutes = 0;

    for (int i = 0; i < file->count; i++) {
        logline_t *entry = file->entries[i];
        total_minutes += entry->timespan.duration_minutes;

        if (entry->tags) {
            for (int j = 0; j < entry->tags->count; j++) {
                char *tag = entry->tags->tags[j];

                /* Find or create tag summary */
                int tag_idx = -1;
                for (int k = 0; k < tag_count; k++) {
                    if (strcmp(summaries[k].tag, tag) == 0) {
                        tag_idx = k;
                        break;
                    }
                }

                if (tag_idx == -1) {
                    /* New tag - check if we need to grow the array */
                    if (tag_count >= summary_capacity) {
                        summary_capacity *= 2;
                        summaries = realloc(summaries, sizeof(tag_summary_t) * summary_capacity);
                    }

                    tag_idx = tag_count++;
                    summaries[tag_idx].tag = strdup(tag);
                    summaries[tag_idx].total_minutes = 0;
                    summaries[tag_idx].entry_count = 0;
                }

                summaries[tag_idx].total_minutes += entry->timespan.duration_minutes;
                summaries[tag_idx].entry_count++;
            }
        }
    }

    printf("Time by tag:\n");
    for (int i = 0; i < tag_count; i++) {
        printf("  #%-19s: %2dh %02dm (%d entries)\n",
               summaries[i].tag,
               summaries[i].total_minutes / 60,
               summaries[i].total_minutes % 60,
               summaries[i].entry_count);
        free(summaries[i].tag);
    }

    /* Free the dynamically allocated summaries array */
    free(summaries);

    printf("\nTotal tracked time: %dh %02dm\n",
           total_minutes / 60, total_minutes % 60);
}

/* Calculate ISO week number (Monday as first day of week) */
int get_iso_week(int year, int month, int day) {
    struct tm date = {0};
    date.tm_year = year - 1900;
    date.tm_mon = month - 1;
    date.tm_mday = day;
    mktime(&date);

    /* Get the Thursday of the current week */
    int day_of_week = (date.tm_wday + 6) % 7; /* Monday = 0 */
    date.tm_mday += 3 - day_of_week; /* Move to Thursday */
    mktime(&date);

    /* Get January 1st of the same year */
    struct tm jan1 = {0};
    jan1.tm_year = date.tm_year;
    jan1.tm_mon = 0;
    jan1.tm_mday = 1;
    mktime(&jan1);

    /* Calculate week number */
    int days_diff = (date.tm_yday - jan1.tm_yday);
    int week = (days_diff / 7) + 1;

    return week;
}

/* Print daily summary */
void print_daily_summary(logfile_t *file) {
    if (file->count == 0) {
        printf("No entries to summarize.\n");
        return;
    }

    printf("=== DAILY SUMMARY ===\n\n");

    /* Structure to hold daily data */
    typedef struct {
        date_t date;
        int total_minutes;
        int entry_count;
    } daily_summary_t;

    /* Dynamic array for daily summaries */
    int day_capacity = 10;
    daily_summary_t *days = malloc(sizeof(daily_summary_t) * day_capacity);
    int day_count = 0;

    /* Aggregate by day */
    for (int i = 0; i < file->count; i++) {
        logline_t *entry = file->entries[i];

        /* Find or create day entry */
        int day_idx = -1;
        for (int j = 0; j < day_count; j++) {
            if (days[j].date.year == entry->date.year &&
                days[j].date.month == entry->date.month &&
                days[j].date.day == entry->date.day) {
                day_idx = j;
                break;
            }
        }

        if (day_idx == -1) {
            /* New day - check if we need to grow array */
            if (day_count >= day_capacity) {
                day_capacity *= 2;
                days = realloc(days, sizeof(daily_summary_t) * day_capacity);
            }

            day_idx = day_count++;
            days[day_idx].date = entry->date;
            days[day_idx].total_minutes = 0;
            days[day_idx].entry_count = 0;
        }

        days[day_idx].total_minutes += entry->timespan.duration_minutes;
        days[day_idx].entry_count++;
    }

    /* Print daily summaries */
    int grand_total_minutes = 0;
    int grand_total_entries = 0;

    for (int i = 0; i < day_count; i++) {
        printf("%04d-%02d-%02d: %3dh %02dm (%d entries)\n",
               days[i].date.year, days[i].date.month, days[i].date.day,
               days[i].total_minutes / 60,
               days[i].total_minutes % 60,
               days[i].entry_count);

        grand_total_minutes += days[i].total_minutes;
        grand_total_entries += days[i].entry_count;
    }

    printf("\n");
    printf("Total days: %d\n", day_count);
    printf("Total entries: %d\n", grand_total_entries);
    printf("Total time: %dh %02dm\n",
           grand_total_minutes / 60, grand_total_minutes % 60);
    printf("Average per day: %dh %02dm\n",
           (grand_total_minutes / day_count) / 60,
           (grand_total_minutes / day_count) % 60);

    /* Free memory */
    free(days);
}

/* Print weekly summary */
void print_weekly_summary(logfile_t *file) {
    if (file->count == 0) {
        printf("No entries to summarize.\n");
        return;
    }

    printf("=== WEEKLY SUMMARY ===\n\n");

    /* Structure to hold weekly data */
    typedef struct {
        int year;
        int week;
        int total_minutes;
        int entry_count;
        date_t first_day;
        date_t last_day;
    } weekly_summary_t;

    /* Dynamic array for weekly summaries */
    int week_capacity = 10;
    weekly_summary_t *weeks = malloc(sizeof(weekly_summary_t) * week_capacity);
    int week_count = 0;

    /* Aggregate by week */
    for (int i = 0; i < file->count; i++) {
        logline_t *entry = file->entries[i];
        int week_num = get_iso_week(entry->date.year, entry->date.month, entry->date.day);

        /* Find or create week entry */
        int week_idx = -1;
        for (int j = 0; j < week_count; j++) {
            if (weeks[j].year == entry->date.year && weeks[j].week == week_num) {
                week_idx = j;
                break;
            }
        }

        if (week_idx == -1) {
            /* New week - check if we need to grow array */
            if (week_count >= week_capacity) {
                week_capacity *= 2;
                weeks = realloc(weeks, sizeof(weekly_summary_t) * week_capacity);
            }

            week_idx = week_count++;
            weeks[week_idx].year = entry->date.year;
            weeks[week_idx].week = week_num;
            weeks[week_idx].total_minutes = 0;
            weeks[week_idx].entry_count = 0;
            weeks[week_idx].first_day = entry->date;
            weeks[week_idx].last_day = entry->date;
        }

        weeks[week_idx].total_minutes += entry->timespan.duration_minutes;
        weeks[week_idx].entry_count++;

        /* Update first and last day of week */
        if (compare_dates(&entry->date, &weeks[week_idx].first_day) < 0) {
            weeks[week_idx].first_day = entry->date;
        }
        if (compare_dates(&entry->date, &weeks[week_idx].last_day) > 0) {
            weeks[week_idx].last_day = entry->date;
        }
    }

    /* Print weekly summaries */
    int grand_total_minutes = 0;
    int grand_total_entries = 0;

    for (int i = 0; i < week_count; i++) {
        printf("%04d Week %02d (%04d-%02d-%02d to %04d-%02d-%02d): %3dh %02dm (%d entries)\n",
               weeks[i].year, weeks[i].week,
               weeks[i].first_day.year, weeks[i].first_day.month, weeks[i].first_day.day,
               weeks[i].last_day.year, weeks[i].last_day.month, weeks[i].last_day.day,
               weeks[i].total_minutes / 60,
               weeks[i].total_minutes % 60,
               weeks[i].entry_count);

        grand_total_minutes += weeks[i].total_minutes;
        grand_total_entries += weeks[i].entry_count;
    }

    printf("\n");
    printf("Total weeks: %d\n", week_count);
    printf("Total entries: %d\n", grand_total_entries);
    printf("Total time: %dh %02dm\n",
           grand_total_minutes / 60, grand_total_minutes % 60);
    if (week_count > 0) {
        printf("Average per week: %dh %02dm\n",
               (grand_total_minutes / week_count) / 60,
               (grand_total_minutes / week_count) % 60);
    }

    /* Free memory */
    free(weeks);
}

/* Print monthly summary */
void print_monthly_summary(logfile_t *file) {
    if (file->count == 0) {
        printf("No entries to summarize.\n");
        return;
    }

    printf("=== MONTHLY SUMMARY ===\n\n");

    /* Structure to hold monthly data */
    typedef struct {
        int year;
        int month;
        int total_minutes;
        int entry_count;
        int days_with_entries;
    } monthly_summary_t;

    /* Dynamic array for monthly summaries */
    int month_capacity = 10;
    monthly_summary_t *months = malloc(sizeof(monthly_summary_t) * month_capacity);
    int month_count = 0;

    /* Track unique days per month */
    typedef struct {
        int year;
        int month;
        int day;
    } day_tracker_t;

    int day_tracker_capacity = 100;
    day_tracker_t *day_tracker = malloc(sizeof(day_tracker_t) * day_tracker_capacity);
    int day_tracker_count = 0;

    /* Aggregate by month */
    for (int i = 0; i < file->count; i++) {
        logline_t *entry = file->entries[i];

        /* Find or create month entry */
        int month_idx = -1;
        for (int j = 0; j < month_count; j++) {
            if (months[j].year == entry->date.year && months[j].month == entry->date.month) {
                month_idx = j;
                break;
            }
        }

        if (month_idx == -1) {
            /* New month - check if we need to grow array */
            if (month_count >= month_capacity) {
                month_capacity *= 2;
                months = realloc(months, sizeof(monthly_summary_t) * month_capacity);
            }

            month_idx = month_count++;
            months[month_idx].year = entry->date.year;
            months[month_idx].month = entry->date.month;
            months[month_idx].total_minutes = 0;
            months[month_idx].entry_count = 0;
            months[month_idx].days_with_entries = 0;
        }

        months[month_idx].total_minutes += entry->timespan.duration_minutes;
        months[month_idx].entry_count++;

        /* Track unique days */
        int day_exists = 0;
        for (int j = 0; j < day_tracker_count; j++) {
            if (day_tracker[j].year == entry->date.year &&
                day_tracker[j].month == entry->date.month &&
                day_tracker[j].day == entry->date.day) {
                day_exists = 1;
                break;
            }
        }

        if (!day_exists) {
            if (day_tracker_count >= day_tracker_capacity) {
                day_tracker_capacity *= 2;
                day_tracker = realloc(day_tracker, sizeof(day_tracker_t) * day_tracker_capacity);
            }
            day_tracker[day_tracker_count].year = entry->date.year;
            day_tracker[day_tracker_count].month = entry->date.month;
            day_tracker[day_tracker_count].day = entry->date.day;
            day_tracker_count++;
            months[month_idx].days_with_entries++;
        }
    }

    /* Print monthly summaries */
    int grand_total_minutes = 0;
    int grand_total_entries = 0;
    int grand_total_days = 0;

    const char *month_names[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    for (int i = 0; i < month_count; i++) {
        printf("%04d %s: %3dh %02dm (%d entries across %d days)\n",
               months[i].year,
               month_names[months[i].month - 1],
               months[i].total_minutes / 60,
               months[i].total_minutes % 60,
               months[i].entry_count,
               months[i].days_with_entries);

        grand_total_minutes += months[i].total_minutes;
        grand_total_entries += months[i].entry_count;
        grand_total_days += months[i].days_with_entries;
    }

    printf("\n");
    printf("Total months: %d\n", month_count);
    printf("Total days with entries: %d\n", grand_total_days);
    printf("Total entries: %d\n", grand_total_entries);
    printf("Total time: %dh %02dm\n",
           grand_total_minutes / 60, grand_total_minutes % 60);
    if (month_count > 0) {
        printf("Average per month: %dh %02dm\n",
               (grand_total_minutes / month_count) / 60,
               (grand_total_minutes / month_count) % 60);
    }
    if (grand_total_days > 0) {
        printf("Average per working day: %dh %02dm\n",
               (grand_total_minutes / grand_total_days) / 60,
               (grand_total_minutes / grand_total_days) % 60);
    }

    /* Free memory */
    free(months);
    free(day_tracker);
}

/* Print CSV format */
void print_csv(logfile_t *file) {
    printf("Date,Start,End,Duration_Minutes,Description,Tags,Percentage\n");

    for (int i = 0; i < file->count; i++) {
        logline_t *entry = file->entries[i];

        printf("%04d-%02d-%02d,%02d:%02d,%02d:%02d,%d,",
               entry->date.year, entry->date.month, entry->date.day,
               entry->timespan.start.hour, entry->timespan.start.minute,
               entry->timespan.end.hour, entry->timespan.end.minute,
               entry->timespan.duration_minutes);

        if (entry->description) {
            printf("%s", entry->description);
        }
        printf(",");

        if (entry->tags) {
            for (int j = 0; j < entry->tags->count; j++) {
                if (j > 0) printf(";");
                printf("#%s", entry->tags->tags[j]);
            }
        }
        printf(",");

        if (entry->percentage > 0) {
            printf("%d", entry->percentage);
        }

        printf("\n");
    }
}

/* Print JSON format */
void print_json(logfile_t *file) {
    printf("{\n");
    printf("  \"total_entries\": %d,\n", file->count);
    printf("  \"entries\": [\n");

    for (int i = 0; i < file->count; i++) {
        logline_t *entry = file->entries[i];

        printf("    {\n");
        printf("      \"date\": \"%04d-%02d-%02d\",\n",
               entry->date.year, entry->date.month, entry->date.day);
        printf("      \"start\": \"%02d:%02d\",\n",
               entry->timespan.start.hour, entry->timespan.start.minute);
        printf("      \"end\": \"%02d:%02d\",\n",
               entry->timespan.end.hour, entry->timespan.end.minute);
        printf("      \"duration_minutes\": %d,\n", entry->timespan.duration_minutes);

        printf("      \"description\": ");
        if (entry->description) {
            printf("\"");
            for (char *p = entry->description; *p; p++) {
                if (*p == '"') printf("\\\"");
                else if (*p == '\\') printf("\\\\");
                else printf("%c", *p);
            }
            printf("\"");
        } else {
            printf("null");
        }
        printf(",\n");

        printf("      \"tags\": [");
        if (entry->tags) {
            for (int j = 0; j < entry->tags->count; j++) {
                if (j > 0) printf(", ");
                printf("\"#%s\"", entry->tags->tags[j]);
            }
        }
        printf("]");

        if (entry->percentage > 0) {
            printf(",\n      \"percentage\": %d", entry->percentage);
        }

        printf("\n    }");
        if (i < file->count - 1) printf(",");
        printf("\n");
    }

    printf("  ]\n");
    printf("}\n");
}

/* Two-phase parsing implementation */

/* Phase 1: Classify line type based on simple patterns */
line_type_t classify_line(const char* line) {
    if (!line) return LINE_OTHER;

    /* Skip leading whitespace */
    while (*line == ' ' || *line == '\t') line++;

    /* Check for date pattern: # YYYY-MM-DD */
    if (line[0] == '#' && line[1] == ' ' &&
        strlen(line) >= 12 &&
        line[2] >= '1' && line[2] <= '2' &&  /* Year starts with 1 or 2 */
        line[3] >= '0' && line[3] <= '9' &&
        line[4] >= '0' && line[4] <= '9' &&
        line[5] >= '0' && line[5] <= '9' &&
        line[6] == '-' &&
        line[7] >= '0' && line[7] <= '1' &&  /* Month */
        line[8] >= '0' && line[8] <= '9' &&
        line[9] == '-' &&
        line[10] >= '0' && line[10] <= '3' && /* Day */
        line[11] >= '0' && line[11] <= '9') {
        return LINE_DATE;
    }

    /* Check for time pattern: HHMM-HHMM */
    if (strlen(line) >= 9 &&
        line[0] >= '0' && line[0] <= '2' &&  /* Hour */
        line[1] >= '0' && line[1] <= '9' &&
        line[2] >= '0' && line[2] <= '5' &&  /* Minute */
        line[3] >= '0' && line[3] <= '9' &&
        line[4] == '-' &&
        line[5] >= '0' && line[5] <= '2' &&  /* Hour */
        line[6] >= '0' && line[6] <= '9' &&
        line[7] >= '0' && line[7] <= '5' &&  /* Minute */
        line[8] >= '0' && line[8] <= '9' &&
        (line[9] == ' ' || line[9] == '\0' || line[9] == '\n')) {
        return LINE_TIME;
    }

    return LINE_OTHER;
}

/* Helper: Check if year is a leap year */
int is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/* Helper: Validate date values */
int validate_date(int year, int month, int day) {
    /* Basic range checks */
    if (year < 1900 || year > 2100) return 0;  /* Reasonable year range */
    if (month < 1 || month > 12) return 0;
    if (day < 1) return 0;

    /* Days in each month (non-leap year) */
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    /* Adjust February for leap year */
    if (month == 2 && is_leap_year(year)) {
        days_in_month[1] = 29;
    }

    /* Check day is within month's range */
    if (day > days_in_month[month - 1]) return 0;

    return 1;
}

/* Compare two dates. Returns: -1 if d1 < d2, 0 if equal, 1 if d1 > d2 */
int compare_dates(date_t *d1, date_t *d2) {
    if (d1->year != d2->year) return d1->year < d2->year ? -1 : 1;
    if (d1->month != d2->month) return d1->month < d2->month ? -1 : 1;
    if (d1->day != d2->day) return d1->day < d2->day ? -1 : 1;
    return 0;
}

/* Check if entry passes filters */
bool entry_passes_filters(logline_t *entry) {
    /* Check date range filter */
    if (filter_from.year > 0) {
        if (compare_dates(&entry->date, &filter_from) < 0) {
            return false;
        }
    }

    if (filter_to.year > 0) {
        if (compare_dates(&entry->date, &filter_to) > 0) {
            return false;
        }
    }

    /* Check tag filter */
    if (filter_tag && entry->tags) {
        bool has_tag = false;
        for (int i = 0; i < entry->tags->count; i++) {
            if (strcmp(entry->tags->tags[i], filter_tag) == 0) {
                has_tag = true;
                break;
            }
        }
        if (!has_tag) {
            return false;
        }
    }

    return true;
}

/* Phase 2: Parse date line "# YYYY-MM-DD" */
date_t parse_date_line(const char* line, int line_number) {
    date_t date = {0, 0, 0};

    /* Skip "# " */
    const char* datepart = line + 2;

    /* Extract year */
    date.year = (datepart[0] - '0') * 1000 +
                (datepart[1] - '0') * 100 +
                (datepart[2] - '0') * 10 +
                (datepart[3] - '0');

    /* Extract month */
    date.month = (datepart[5] - '0') * 10 + (datepart[6] - '0');

    /* Extract day */
    date.day = (datepart[8] - '0') * 10 + (datepart[9] - '0');

    /* Validate the date */
    if (!validate_date(date.year, date.month, date.day)) {
        fprintf(stderr, "Line %d: Warning: Invalid date %04d-%02d-%02d, using current date\n",
               line_number, date.year, date.month, date.day);
        /* Return current date as fallback */
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        date.year = tm->tm_year + 1900;
        date.month = tm->tm_mon + 1;
        date.day = tm->tm_mday;
    }

    return date;
}

/* Phase 2: Parse time line "HHMM-HHMM description #tags" */
logline_t* parse_time_line(const char* line, int line_number) {
    logline_t* entry = create_logline();
    entry->date = current_date;
    entry->percentage = 0;

    /* Extract start time */
    int start_hour = (line[0] - '0') * 10 + (line[1] - '0');
    int start_minute = (line[2] - '0') * 10 + (line[3] - '0');

    /* Validate start time */
    if (start_hour < 0 || start_hour > 23 || start_minute < 0 || start_minute > 59) {
        if (verbose) {
            fprintf(stderr, "Line %d: Error: Invalid start time %02d:%02d (hours must be 0-23, minutes 0-59)\n",
                   line_number, start_hour, start_minute);
        }
        free(entry);
        return NULL;
    }

    /* Extract end time */
    int end_hour = (line[5] - '0') * 10 + (line[6] - '0');
    int end_minute = (line[7] - '0') * 10 + (line[8] - '0');

    /* Validate end time */
    if (end_hour < 0 || end_hour > 23 || end_minute < 0 || end_minute > 59) {
        if (verbose) {
            fprintf(stderr, "Line %d: Error: Invalid end time %02d:%02d (hours must be 0-23, minutes 0-59)\n", line_number,
                   end_hour, end_minute);
        }
        free(entry);
        return NULL;
    }

    /* Create timespan */
    entry->timespan.start.hour = start_hour;
    entry->timespan.start.minute = start_minute;
    entry->timespan.end.hour = end_hour;
    entry->timespan.end.minute = end_minute;
    entry->timespan.duration_minutes = calculate_duration(&entry->timespan.start, &entry->timespan.end);

    /* Check for invalid duration (backwards span) */
    if (entry->timespan.duration_minutes < 0) {
        if (verbose) {
            fprintf(stderr, "Line %d: Error: Invalid backwards timespan %02d:%02d-%02d:%02d\n", line_number,
                   start_hour, start_minute, end_hour, end_minute);
        }
        free(entry);
        return NULL;
    }

    /* Parse rest of line for description and tags */
    const char* rest = line + 9;
    while (*rest == ' ') rest++; /* Skip spaces */

    if (*rest) {
        /* Find tags and percentage */
        entry->tags = create_taglist();

        char* line_copy = strdup(rest);
        char* current_pos = line_copy;
        char* desc_end = line_copy;

        /* Look for percentage pattern %NN */
        char* percent_pos = strstr(line_copy, "%");
        if (percent_pos && percent_pos[1] >= '0' && percent_pos[1] <= '9') {
            int percentage_value = atoi(percent_pos + 1);
            /* Validate percentage is within 0-100 range */
            if (percentage_value < 0 || percentage_value > 100) {
                fprintf(stderr, "Line %d: Warning: Invalid percentage %d%% (must be 0-100)\n", line_number, percentage_value);
                entry->percentage = 0;
            } else {
                entry->percentage = percentage_value;
            }
            /* Remove percentage from line by shifting text */
            char* after_percent = percent_pos;
            while (*after_percent && *after_percent != ' ') after_percent++;
            memmove(percent_pos, after_percent, strlen(after_percent) + 1);
        }

        /* Extract tags (#word) and find where description ends */
        char* first_tag = strchr(current_pos, '#');
        if (first_tag) {
            desc_end = first_tag;
        } else {
            desc_end = line_copy + strlen(line_copy);
        }

        /* Extract all tags */
        char* tag_pos = strchr(current_pos, '#');
        while (tag_pos) {
            /* Find end of tag */
            char* tag_end = tag_pos + 1;
            while (*tag_end && *tag_end != ' ' && *tag_end != '#' && *tag_end != '\n') {
                tag_end++;
            }

            /* Extract tag (skip the # symbol) */
            char tag_char = *tag_end;
            *tag_end = '\0';
            add_tag(entry->tags, tag_pos + 1);  /* Skip the # */
            *tag_end = tag_char;

            /* Find next tag */
            tag_pos = strchr(tag_end, '#');
        }

        /* Extract description (everything before first tag) */
        if (desc_end > line_copy) {
            *desc_end = '\0';
            entry->description = trim_string(line_copy);
        }

        free(line_copy);
    }

    return entry;
}

/* Main two-phase parsing function */
int parse_two_phase(FILE* input) {
    char line[4096];
    int line_number = 0;

    while (fgets(line, sizeof(line), input)) {
        line_number++;

        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }

        /* Classify and process line */
        line_type_t type = classify_line(line);

        switch (type) {
            case LINE_DATE: {
                current_date = parse_date_line(line, line_number);
                if (verbose) {
                    fprintf(stderr, "Line %d: Debug: Parsed date %04d-%02d-%02d\n",
                           line_number, current_date.year, current_date.month, current_date.day);
                }
                break;
            }

            case LINE_TIME: {
                logline_t* entry = parse_time_line(line, line_number);
                if (entry) {
                    if (verbose) {
                        fprintf(stderr, "Line %d: Debug: Parsed time entry %02d:%02d-%02d:%02d\n",
                               line_number, entry->timespan.start.hour, entry->timespan.start.minute,
                               entry->timespan.end.hour, entry->timespan.end.minute);
                    }

                    /* Apply filters before adding */
                    if (entry_passes_filters(entry)) {
                        add_entry(current_logfile, entry);
                    } else {
                        /* Free filtered entry */
                        if (entry->description) free(entry->description);
                        if (entry->tags) {
                            for (int i = 0; i < entry->tags->count; i++) {
                                free(entry->tags->tags[i]);
                            }
                            free(entry->tags->tags);
                            free(entry->tags);
                        }
                        free(entry);
                    }
                }
                break;
            }

            case LINE_OTHER:
                /* Ignore - no action needed */
                if (verbose && strlen(line) > 0) {
                    fprintf(stderr, "Line %d: Debug: Ignoring line: %.50s%s\n",
                           line_number, line, strlen(line) > 50 ? "..." : "");
                }
                break;
        }
    }

    return 0; /* Success */
}

/* Main function */
int main(int argc, char ** argv) {
    int opt;
    output_format_t format = FORMAT_TEXT;
    const char *input_file = NULL;
    const char *scan_path = NULL;
    bool show_daily = false;
    bool show_weekly = false;
    bool show_monthly = false;

    /* Scanning options */
    scan_config_t scan_config = {
        .recursive = false,
        .follow_symlinks = false,
        .date_from_filename = false,
        .date_from_path = false,
        .verbose = false,
        .max_depth = 10,
        .max_file_size = 10 * 1024 * 1024,  /* 10MB */
        .exclude_patterns = NULL,
        .exclude_count = 0,
        .include_patterns = NULL,
        .include_count = 0
    };

    /* Option parsing */
    static struct option long_options[] = {
        {"help",    no_argument,       0, 'h'},
        {"version", no_argument,       0, 'V'},
        {"format",  required_argument, 0, 'f'},
        {"tags",    no_argument,       0, 't'},
        {"daily",   no_argument,       0, 'd'},
        {"weekly",  no_argument,       0, 'w'},
        {"monthly", no_argument,       0, 'm'},
        {"verbose", no_argument,       0, 'v'},
        {"scan",    required_argument, 0, 'S'},
        {"recursive", no_argument,     0, 'R'},
        {"date-from-filename", no_argument, 0, 2001},
        {"date-from-path", no_argument, 0, 2002},
        {"include", required_argument, 0, 2003},
        {"exclude", required_argument, 0, 2004},
        {"from",    required_argument, 0, 1001},
        {"to",      required_argument, 0, 1002},
        {"tag",     required_argument, 0, 1003},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "hVf:tdwmvS:R", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'V':
                print_version(argv[0]);
                return 0;
            case 'f':
                if (strcmp(optarg, "text") == 0) {
                    format = FORMAT_TEXT;
                } else if (strcmp(optarg, "csv") == 0) {
                    format = FORMAT_CSV;
                } else if (strcmp(optarg, "json") == 0) {
                    format = FORMAT_JSON;
                } else {
                    fprintf(stderr, "Error: Unknown format '%s'\n", optarg);
                    print_usage(argv[0]);
                    return 1;
                }
                break;
            case 't':
                /* Tags are always shown in current implementation */
                break;
            case 'd':
                show_daily = true;
                break;
            case 'w':
                show_weekly = true;
                break;
            case 'm':
                show_monthly = true;
                break;
            case 'v':
                verbose = true;
                scan_config.verbose = true;
                break;
            case 'S':
                scan_path = optarg;
                break;
            case 'R':
                scan_config.recursive = true;
                break;
            case 2001: /* --date-from-filename */
                scan_config.date_from_filename = true;
                break;
            case 2002: /* --date-from-path */
                scan_config.date_from_path = true;
                break;
            case 2003: /* --include */
                /* Add include pattern */
                scan_config.include_count++;
                scan_config.include_patterns = realloc(scan_config.include_patterns,
                                                      scan_config.include_count * sizeof(char*));
                scan_config.include_patterns[scan_config.include_count - 1] = strdup(optarg);
                break;
            case 2004: /* --exclude */
                /* Add exclude pattern */
                scan_config.exclude_count++;
                scan_config.exclude_patterns = realloc(scan_config.exclude_patterns,
                                                      scan_config.exclude_count * sizeof(char*));
                scan_config.exclude_patterns[scan_config.exclude_count - 1] = strdup(optarg);
                break;
            case 1001: /* --from */
                if (sscanf(optarg, "%d-%d-%d", &filter_from.year, &filter_from.month, &filter_from.day) != 3) {
                    fprintf(stderr, "Error: Invalid date format for --from (use YYYY-MM-DD)\n");
                    return 1;
                }
                if (!validate_date(filter_from.year, filter_from.month, filter_from.day)) {
                    fprintf(stderr, "Error: Invalid date for --from\n");
                    return 1;
                }
                break;
            case 1002: /* --to */
                if (sscanf(optarg, "%d-%d-%d", &filter_to.year, &filter_to.month, &filter_to.day) != 3) {
                    fprintf(stderr, "Error: Invalid date format for --to (use YYYY-MM-DD)\n");
                    return 1;
                }
                if (!validate_date(filter_to.year, filter_to.month, filter_to.day)) {
                    fprintf(stderr, "Error: Invalid date for --to\n");
                    return 1;
                }
                break;
            case 1003: /* --tag */
                filter_tag = strdup(optarg);
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    /* Handle directory scanning if requested */
    if (scan_path) {
        /* Perform directory scan */
        scan_result_t *scan_result = scan_directory(scan_path, &scan_config);

        if (!scan_result || scan_result->file_count == 0) {
            fprintf(stderr, "No time log files found in %s\n", scan_path);
            return 1;
        }

        printf("Found %d time log files with %d total entries\n",
               scan_result->file_count, scan_result->entries_total);

        if (scan_result->files_without_dates > 0) {
            printf("Files with inferred dates: %d\n",
                   scan_result->files_with_dates - scan_result->files_without_dates);
        }

        /* Process scan results */
        current_logfile = process_scan_results(scan_result, &scan_config);

        free_scan_result(scan_result);

        /* Continue to display results */
        if (current_logfile && current_logfile->count > 0) {
            if (show_daily) {
                print_daily_summary(current_logfile);
            } else if (show_weekly) {
                print_weekly_summary(current_logfile);
            } else if (show_monthly) {
                print_monthly_summary(current_logfile);
            } else if (format == FORMAT_TEXT) {
                print_summary(current_logfile);
            } else if (format == FORMAT_CSV) {
                print_csv(current_logfile);
            } else if (format == FORMAT_JSON) {
                print_json(current_logfile);
            }
        }

        free_logfile(current_logfile);
        return 0;
    }

    /* Get input file if provided */
    FILE *input;
    if (optind < argc) {
        input_file = argv[optind];
        input = fopen(input_file, "r");
        if (!input) {
            fprintf(stderr, "Error: Cannot open file '%s'\n", input_file);
            return 1;
        }
    } else {
        input = stdin;
    }

    /* Initialize global data */
    current_logfile = create_logfile();

    /* Initialize current_date with today's date */
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    current_date.year = tm->tm_year + 1900;
    current_date.month = tm->tm_mon + 1;
    current_date.day = tm->tm_mday;

    /* Enable debug output if verbose flag is set */
    if (verbose) {
        fprintf(stderr, "Debug: Verbose mode enabled\n");
    }

    /* Parse the input using two-phase approach */
    int result = parse_two_phase(input);

    /* Print summary if parsing succeeded */
    if (result == 0 && current_logfile->count > 0) {
        if (show_daily) {
            /* Daily summary overrides format option */
            print_daily_summary(current_logfile);
        } else if (show_weekly) {
            /* Weekly summary overrides format option */
            print_weekly_summary(current_logfile);
        } else if (show_monthly) {
            /* Monthly summary overrides format option */
            print_monthly_summary(current_logfile);
        } else if (format == FORMAT_TEXT) {
            print_summary(current_logfile);
        } else if (format == FORMAT_CSV) {
            print_csv(current_logfile);
        } else if (format == FORMAT_JSON) {
            print_json(current_logfile);
        }
    }

    /* Close input file if opened */
    if (input_file && input) {
        fclose(input);
    }

    /* Clean up */
    free_logfile(current_logfile);

    /* Free include/exclude patterns */
    for (int i = 0; i < scan_config.include_count; i++) {
        free(scan_config.include_patterns[i]);
    }
    free(scan_config.include_patterns);

    for (int i = 0; i < scan_config.exclude_count; i++) {
        free(scan_config.exclude_patterns[i]);
    }
    free(scan_config.exclude_patterns);

    return result;
}
