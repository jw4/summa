/*
 * summa_db.c - SQLite database implementation for Summa
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wordexp.h>
#include "summa_db.h"

/* External functions from summa.c */
extern logfile_t* create_logfile(void);
extern void add_entry(logfile_t *file, logline_t *entry);

/* SQL statements for schema creation */
static const char *schema_sql =
    "CREATE TABLE IF NOT EXISTS metadata ("
    "  key TEXT PRIMARY KEY,"
    "  value TEXT"
    ");"
    ""
    "CREATE TABLE IF NOT EXISTS files ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  filepath TEXT UNIQUE NOT NULL,"
    "  last_modified INTEGER,"
    "  last_scanned INTEGER,"
    "  entry_count INTEGER DEFAULT 0"
    ");"
    ""
    "CREATE TABLE IF NOT EXISTS entries ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  file_id INTEGER,"
    "  date TEXT,"
    "  start_time TEXT,"
    "  end_time TEXT,"
    "  duration_minutes INTEGER,"
    "  description TEXT,"
    "  percentage INTEGER,"
    "  line_number INTEGER,"
    "  created_at INTEGER DEFAULT (strftime('%s', 'now')),"
    "  FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE"
    ");"
    ""
    "CREATE TABLE IF NOT EXISTS tags ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT UNIQUE NOT NULL"
    ");"
    ""
    "CREATE TABLE IF NOT EXISTS entry_tags ("
    "  entry_id INTEGER,"
    "  tag_id INTEGER,"
    "  PRIMARY KEY (entry_id, tag_id),"
    "  FOREIGN KEY (entry_id) REFERENCES entries(id) ON DELETE CASCADE,"
    "  FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE"
    ");"
    ""
    "CREATE INDEX IF NOT EXISTS idx_entries_date ON entries(date);"
    "CREATE INDEX IF NOT EXISTS idx_entries_file ON entries(file_id);"
    "CREATE INDEX IF NOT EXISTS idx_tags_name ON tags(name);"
    "CREATE INDEX IF NOT EXISTS idx_entry_tags_entry ON entry_tags(entry_id);"
    "CREATE INDEX IF NOT EXISTS idx_entry_tags_tag ON entry_tags(tag_id);";

/* Expand ~ in path */
char* db_expand_path(const char *path) {
    if (!path || path[0] != '~') {
        return strdup(path);
    }

    wordexp_t exp;
    char *expanded = NULL;

    if (wordexp(path, &exp, 0) == 0 && exp.we_wordc > 0) {
        expanded = strdup(exp.we_wordv[0]);
        wordfree(&exp);
    } else {
        /* Fallback to manual expansion */
        const char *home = getenv("HOME");
        if (home) {
            size_t len = strlen(home) + strlen(path);
            expanded = malloc(len);
            snprintf(expanded, len, "%s%s", home, path + 1);
        } else {
            expanded = strdup(path);
        }
    }

    return expanded;
}

/* Open database connection */
summa_db_t* db_open(const char *path) {
    summa_db_t *db = calloc(1, sizeof(summa_db_t));
    if (!db) return NULL;

    /* Use default path if not specified */
    const char *db_path = path ? path : DEFAULT_DB_PATH;
    db->path = db_expand_path(db_path);

    /* Create directory if needed */
    char *dir = strdup(db->path);
    char *last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir(dir, 0755);  /* Create directory, ignore if exists */
    }
    free(dir);

    /* Open database */
    int rc = sqlite3_open(db->path, &db->db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error opening database %s: %s\n",
                db->path, sqlite3_errmsg(db->db));
        free(db->path);
        free(db);
        return NULL;
    }

    /* Enable foreign keys */
    sqlite3_exec(db->db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);

    /* Initialize schema if needed */
    if (!db_check_schema(db)) {
        if (!db_init_schema(db)) {
            db_close(db);
            return NULL;
        }
    }

    return db;
}

/* Close database connection */
void db_close(summa_db_t *db) {
    if (!db) return;

    if (db->in_transaction) {
        db_rollback_transaction(db);
    }

    if (db->db) {
        sqlite3_close(db->db);
    }

    free(db->path);
    free(db);
}

/* Initialize database schema */
bool db_init_schema(summa_db_t *db) {
    if (!db || !db->db) return false;

    char *err_msg = NULL;
    int rc = sqlite3_exec(db->db, schema_sql, NULL, NULL, &err_msg);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error initializing schema: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }

    /* Set database version */
    char version_sql[256];
    snprintf(version_sql, sizeof(version_sql),
             "INSERT OR REPLACE INTO metadata (key, value) VALUES ('version', '%d')",
             DB_VERSION);

    rc = sqlite3_exec(db->db, version_sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error setting version: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }

    return true;
}

/* Check if schema exists and is current */
bool db_check_schema(summa_db_t *db) {
    if (!db || !db->db) return false;

    /* Check if metadata table exists */
    const char *check_sql = "SELECT value FROM metadata WHERE key = 'version'";
    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_prepare_v2(db->db, check_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return false;  /* Table doesn't exist */
    }

    int version = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (version == 0) {
        return false;  /* No version found */
    }

    if (version < DB_VERSION) {
        /* Need migration */
        return db_migrate_schema(db, version);
    }

    return version == DB_VERSION;
}

/* Migrate schema to current version */
bool db_migrate_schema(summa_db_t *db, int from_version) {
    (void)db; /* Suppress unused parameter warning - will be used in future migrations */

    /* Placeholder for future migrations */
    if (from_version < DB_VERSION) {
        fprintf(stderr, "Database migration from version %d to %d not implemented\n",
                from_version, DB_VERSION);
        return false;
    }
    return true;
}

/* Begin transaction */
bool db_begin_transaction(summa_db_t *db) {
    if (!db || !db->db || db->in_transaction) return false;

    char *err_msg = NULL;
    int rc = sqlite3_exec(db->db, "BEGIN TRANSACTION", NULL, NULL, &err_msg);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error beginning transaction: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }

    db->in_transaction = true;
    return true;
}

/* Commit transaction */
bool db_commit_transaction(summa_db_t *db) {
    if (!db || !db->db || !db->in_transaction) return false;

    char *err_msg = NULL;
    int rc = sqlite3_exec(db->db, "COMMIT", NULL, NULL, &err_msg);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error committing transaction: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }

    db->in_transaction = false;
    return true;
}

/* Rollback transaction */
bool db_rollback_transaction(summa_db_t *db) {
    if (!db || !db->db || !db->in_transaction) return false;

    char *err_msg = NULL;
    int rc = sqlite3_exec(db->db, "ROLLBACK", NULL, NULL, &err_msg);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error rolling back transaction: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }

    db->in_transaction = false;
    return true;
}

/* Get or create tag ID */
static int get_or_create_tag(summa_db_t *db, const char *tag_name) {
    const char *select_sql = "SELECT id FROM tags WHERE name = ?";
    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_prepare_v2(db->db, select_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, tag_name, -1, SQLITE_STATIC);

    int tag_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        tag_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (tag_id > 0) return tag_id;

    /* Create new tag */
    const char *insert_sql = "INSERT INTO tags (name) VALUES (?)";
    rc = sqlite3_prepare_v2(db->db, insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, tag_name, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        tag_id = (int)sqlite3_last_insert_rowid(db->db);
    }
    sqlite3_finalize(stmt);

    return tag_id;
}

/* Import a single entry */
bool db_import_entry(summa_db_t *db, const char *filepath, logline_t *entry) {
    if (!db || !db->db || !entry) return false;

    /* Get or create file record */
    const char *file_sql = "INSERT OR IGNORE INTO files (filepath) VALUES (?)";
    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_prepare_v2(db->db, file_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, filepath, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    /* Get file ID */
    const char *get_file_sql = "SELECT id FROM files WHERE filepath = ?";
    rc = sqlite3_prepare_v2(db->db, get_file_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, filepath, -1, SQLITE_STATIC);

    int file_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        file_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (file_id < 0) return false;

    /* Format date and times */
    char date_str[16];
    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d",
             entry->date.year, entry->date.month, entry->date.day);

    char start_str[8];
    snprintf(start_str, sizeof(start_str), "%02d:%02d",
             entry->timespan.start.hour, entry->timespan.start.minute);

    char end_str[8];
    snprintf(end_str, sizeof(end_str), "%02d:%02d",
             entry->timespan.end.hour, entry->timespan.end.minute);

    /* Insert entry */
    const char *entry_sql =
        "INSERT INTO entries (file_id, date, start_time, end_time, "
        "duration_minutes, description, percentage, line_number) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

    rc = sqlite3_prepare_v2(db->db, entry_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, file_id);
    sqlite3_bind_text(stmt, 2, date_str, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, start_str, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, end_str, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, entry->timespan.duration_minutes);
    sqlite3_bind_text(stmt, 6, entry->description, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, entry->percentage);
    sqlite3_bind_int(stmt, 8, 0);  /* line_number not tracked yet */

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);

    int entry_id = (int)sqlite3_last_insert_rowid(db->db);

    /* Add tags */
    if (entry->tags) {
        const char *tag_sql =
            "INSERT INTO entry_tags (entry_id, tag_id) VALUES (?, ?)";

        rc = sqlite3_prepare_v2(db->db, tag_sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) return false;

        for (int i = 0; i < entry->tags->count; i++) {
            int tag_id = get_or_create_tag(db, entry->tags->tags[i]);
            if (tag_id > 0) {
                sqlite3_bind_int(stmt, 1, entry_id);
                sqlite3_bind_int(stmt, 2, tag_id);
                sqlite3_step(stmt);
                sqlite3_reset(stmt);
            }
        }
        sqlite3_finalize(stmt);
    }

    /* Update file entry count */
    const char *update_sql =
        "UPDATE files SET entry_count = entry_count + 1 WHERE id = ?";

    rc = sqlite3_prepare_v2(db->db, update_sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, file_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    return true;
}

/* Import entire logfile */
bool db_import_file(summa_db_t *db, const char *filepath, logfile_t *logfile) {
    if (!db || !logfile) return false;

    bool success = true;
    db_begin_transaction(db);

    for (int i = 0; i < logfile->count; i++) {
        if (!db_import_entry(db, filepath, logfile->entries[i])) {
            success = false;
            break;
        }
    }

    if (success) {
        db_commit_transaction(db);
    } else {
        db_rollback_transaction(db);
    }

    return success;
}

/* Query entries by date range */
logfile_t* db_query_by_date_range(summa_db_t *db, date_t from, date_t to) {
    if (!db || !db->db) return NULL;

    char from_str[16], to_str[16];
    snprintf(from_str, sizeof(from_str), "%04d-%02d-%02d",
             from.year, from.month, from.day);
    snprintf(to_str, sizeof(to_str), "%04d-%02d-%02d",
             to.year, to.month, to.day);

    const char *query_sql =
        "SELECT e.id, e.date, e.start_time, e.end_time, e.duration_minutes, "
        "       e.description, e.percentage, f.filepath "
        "FROM entries e "
        "JOIN files f ON e.file_id = f.id "
        "WHERE e.date >= ? AND e.date <= ? "
        "ORDER BY e.date, e.start_time";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, query_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;

    sqlite3_bind_text(stmt, 1, from_str, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, to_str, -1, SQLITE_STATIC);

    logfile_t *result = create_logfile();

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        logline_t entry = {0};

        /* Parse date */
        const char *date_str = (const char *)sqlite3_column_text(stmt, 1);
        sscanf(date_str, "%d-%d-%d",
               &entry.date.year, &entry.date.month, &entry.date.day);

        /* Parse times */
        const char *start_str = (const char *)sqlite3_column_text(stmt, 2);
        sscanf(start_str, "%d:%d", &entry.timespan.start.hour, &entry.timespan.start.minute);

        const char *end_str = (const char *)sqlite3_column_text(stmt, 3);
        sscanf(end_str, "%d:%d", &entry.timespan.end.hour, &entry.timespan.end.minute);

        entry.timespan.duration_minutes = sqlite3_column_int(stmt, 4);
        entry.description = strdup((const char *)sqlite3_column_text(stmt, 5));
        entry.percentage = sqlite3_column_int(stmt, 6);

        int entry_id = sqlite3_column_int(stmt, 0);

        /* Get tags for this entry */
        const char *tags_sql =
            "SELECT t.name FROM tags t "
            "JOIN entry_tags et ON t.id = et.tag_id "
            "WHERE et.entry_id = ?";

        sqlite3_stmt *tag_stmt = NULL;
        rc = sqlite3_prepare_v2(db->db, tags_sql, -1, &tag_stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(tag_stmt, 1, entry_id);

            taglist_t *tags = NULL;

            while (sqlite3_step(tag_stmt) == SQLITE_ROW) {
                if (!tags) {
                    tags = malloc(sizeof(taglist_t));
                    tags->tags = malloc(10 * sizeof(char*));
                    tags->count = 0;
                    tags->capacity = 10;
                }

                const char *tag_name = (const char *)sqlite3_column_text(tag_stmt, 0);
                tags->tags[tags->count++] = strdup(tag_name);
            }

            entry.tags = tags;
            sqlite3_finalize(tag_stmt);
        }

        /* Create a heap-allocated copy for the logfile */
        logline_t *entry_copy = malloc(sizeof(logline_t));
        *entry_copy = entry;
        add_entry(result, entry_copy);
    }

    sqlite3_finalize(stmt);
    return result;
}

/* Query entries by tag */
logfile_t* db_query_by_tag(summa_db_t *db, const char *tag) {
    if (!db || !db->db || !tag) return NULL;

    const char *query_sql =
        "SELECT e.id, e.date, e.start_time, e.end_time, e.duration_minutes, "
        "       e.description, e.percentage, f.filepath "
        "FROM entries e "
        "JOIN files f ON e.file_id = f.id "
        "JOIN entry_tags et ON e.id = et.entry_id "
        "JOIN tags t ON et.tag_id = t.id "
        "WHERE t.name = ? "
        "ORDER BY e.date, e.start_time";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, query_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;

    sqlite3_bind_text(stmt, 1, tag, -1, SQLITE_STATIC);

    logfile_t *result = create_logfile();

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        logline_t entry = {0};

        /* Parse date */
        const char *date_str = (const char *)sqlite3_column_text(stmt, 1);
        sscanf(date_str, "%d-%d-%d",
               &entry.date.year, &entry.date.month, &entry.date.day);

        /* Parse times */
        const char *start_str = (const char *)sqlite3_column_text(stmt, 2);
        sscanf(start_str, "%d:%d", &entry.timespan.start.hour, &entry.timespan.start.minute);

        const char *end_str = (const char *)sqlite3_column_text(stmt, 3);
        sscanf(end_str, "%d:%d", &entry.timespan.end.hour, &entry.timespan.end.minute);

        entry.timespan.duration_minutes = sqlite3_column_int(stmt, 4);
        entry.description = strdup((const char *)sqlite3_column_text(stmt, 5));
        entry.percentage = sqlite3_column_int(stmt, 6);

        int entry_id = sqlite3_column_int(stmt, 0);

        /* Get all tags for this entry */
        const char *tags_sql =
            "SELECT t.name FROM tags t "
            "JOIN entry_tags et ON t.id = et.tag_id "
            "WHERE et.entry_id = ?";

        sqlite3_stmt *tag_stmt = NULL;
        rc = sqlite3_prepare_v2(db->db, tags_sql, -1, &tag_stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(tag_stmt, 1, entry_id);

            taglist_t *tags = NULL;

            while (sqlite3_step(tag_stmt) == SQLITE_ROW) {
                if (!tags) {
                    tags = malloc(sizeof(taglist_t));
                    tags->tags = malloc(10 * sizeof(char*));
                    tags->count = 0;
                    tags->capacity = 10;
                }

                const char *tag_name = (const char *)sqlite3_column_text(tag_stmt, 0);
                tags->tags[tags->count++] = strdup(tag_name);
            }

            entry.tags = tags;
            sqlite3_finalize(tag_stmt);
        }

        /* Create a heap-allocated copy for the logfile */
        logline_t *entry_copy = malloc(sizeof(logline_t));
        *entry_copy = entry;
        add_entry(result, entry_copy);
    }

    sqlite3_finalize(stmt);
    return result;
}

/* Get database statistics */
db_stats_t* db_get_stats(summa_db_t *db) {
    if (!db || !db->db) return NULL;

    db_stats_t *stats = calloc(1, sizeof(db_stats_t));
    if (!stats) return NULL;

    /* Count entries */
    const char *count_sql =
        "SELECT COUNT(*) as entries, "
        "       COUNT(DISTINCT file_id) as files, "
        "       COUNT(DISTINCT tag_id) as tags, "
        "       SUM(duration_minutes) as minutes, "
        "       MIN(date) as min_date, "
        "       MAX(date) as max_date "
        "FROM entries e "
        "LEFT JOIN entry_tags et ON e.id = et.entry_id";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, count_sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats->total_entries = sqlite3_column_int(stmt, 0);
            stats->total_files = sqlite3_column_int(stmt, 1);
            stats->total_tags = sqlite3_column_int(stmt, 2);
            stats->total_minutes = sqlite3_column_int(stmt, 3);

            const char *min_date = (const char *)sqlite3_column_text(stmt, 4);
            if (min_date) {
                sscanf(min_date, "%d-%d-%d",
                       &stats->earliest_date.year,
                       &stats->earliest_date.month,
                       &stats->earliest_date.day);
            }

            const char *max_date = (const char *)sqlite3_column_text(stmt, 5);
            if (max_date) {
                sscanf(max_date, "%d-%d-%d",
                       &stats->latest_date.year,
                       &stats->latest_date.month,
                       &stats->latest_date.day);
            }
        }
        sqlite3_finalize(stmt);
    }

    return stats;
}

/* Vacuum database */
bool db_vacuum(summa_db_t *db) {
    if (!db || !db->db) return false;

    char *err_msg = NULL;
    int rc = sqlite3_exec(db->db, "VACUUM", NULL, NULL, &err_msg);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error vacuuming database: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }

    return true;
}

/* Backup database */
bool db_backup(summa_db_t *db, const char *backup_path) {
    if (!db || !db->db || !backup_path) return false;

    char *expanded_path = db_expand_path(backup_path);

    /* Open backup database */
    sqlite3 *backup_db;
    int rc = sqlite3_open(expanded_path, &backup_db);
    if (rc != SQLITE_OK) {
        free(expanded_path);
        return false;
    }

    /* Perform backup */
    sqlite3_backup *backup = sqlite3_backup_init(backup_db, "main",
                                                 db->db, "main");
    if (backup) {
        sqlite3_backup_step(backup, -1);
        sqlite3_backup_finish(backup);
    }

    rc = sqlite3_errcode(backup_db);
    sqlite3_close(backup_db);
    free(expanded_path);

    return rc == SQLITE_OK;
}