# Summa Time Tracker

A fast and flexible command-line time tracking log file parser that helps you analyze and summarize your time entries with support for tags, percentages, and multiple output formats.

## Features

- **Multiple Summary Views**: Daily, weekly, and monthly summaries
- **Flexible Filtering**: Filter by date range or specific tags
- **Multiple Output Formats**: Text, CSV, and JSON
- **Tag-based Categorization**: Group and analyze time by hashtags
- **Percentage Tracking**: Track effort levels with percentage markers
- **Comprehensive Validation**: Detects invalid dates, times, and percentages
- **Unicode Support**: Handle international characters and emojis
- **Fast Performance**: Process thousands of entries in milliseconds

## Installation

### From Source

```bash
# Clone the repository
git clone https://github.com/jw4/summa.git
cd summa

# Build the binary
make

# Install to ~/bin (default)
make install

# Or install to custom location
make install PREFIX=/usr/local
```

### Requirements

- C compiler (gcc or clang)
- Make build tool
- Standard C library

## Log Format

Summa parses time log files with entries in the following format:

```
# YYYY-MM-DD
HHMM-HHMM Description #tag1 #tag2
HHMM-HHMM %75 Task with 75% effort #work
```

### Entry Components

- **Date Header**: Lines starting with `# YYYY-MM-DD` set the current date
- **Time Span**: `HHMM-HHMM` format (24-hour)
- **Percentage** (optional): `%NN` to indicate effort level (0-100)
- **Description**: Free text description of the activity
- **Tags**: Hashtags for categorization (e.g., `#meeting`, `#coding`)

### Format Examples

#### Basic Time Entry

```
0900-1000 Team standup meeting
```

- Time: 9:00 AM to 10:00 AM (1 hour)
- Description: "Team standup meeting"
- No tags or percentage

#### Entry with Tags

```
1000-1200 Feature development #coding #backend
```

- Time: 10:00 AM to 12:00 PM (2 hours)
- Description: "Feature development"
- Tags: #coding, #backend

#### Entry with Percentage (Billable/Effort)

```
1300-1500 %80 Code review session #review
```

- Time: 1:00 PM to 3:00 PM (2 hours)
- Percentage: 80% (e.g., 80% billable or focused)
- Description: "Code review session"
- Tags: #review

#### Complete Example Log File

```markdown
# 2024-03-15

0900-1000 Morning standup #meeting #team
1000-1200 Feature development #coding #backend
1200-1300 Lunch break #break
1300-1500 %80 Code review session #review #mentoring
1500-1630 Bug fixes #bugfix #urgent
1630-1700 Daily wrap-up #admin

# 2024-03-16

0830-1000 Sprint planning #meeting #planning
1000-1130 Documentation writing #docs
1130-1200 Email catch-up #email #admin

# 2024-03-17

0900-0930 Check emails #email
0930-1130 %100 Client presentation prep #client #important
1130-1200 Team sync #meeting
1200-1300 #lunch
1300-1430 Client presentation #client #meeting
1430-1500 Follow-up notes #admin #client
1500-1700 Development work #coding

---

Notes: Client was happy with the presentation!
TODO: Send follow-up email by Monday
```

### Important Notes

- **Any line not matching the time pattern is ignored** - Use this for notes, separators, or comments
- **Tags are optional** but useful for categorization and filtering
- **Percentages must be 0-100** - Values outside this range trigger warnings
- **Times use 24-hour format** - 1400 means 2:00 PM
- **Date headers persist** - All entries below a date header use that date until a new header
- **Unicode is supported** - Descriptions can include emojis and international characters: `1000-1100 Meeting with 北京 team 🌏 #international`

## Usage

### Basic Usage

```bash
# Parse a file and show summary
summa logfile.md

# Parse from stdin
cat logfile.md | summa

# Show help
summa -h
```

### Summary Options

```bash
# Daily summary - shows time per day
summa -d logfile.md

# Weekly summary - shows time per week
summa -w logfile.md

# Monthly summary - shows time per month
summa -m logfile.md
```

### Output Formats

```bash
# Default text format with tag summaries
summa logfile.md

# CSV format for spreadsheet import
summa -f csv logfile.md > report.csv

# JSON format for programmatic processing
summa -f json logfile.md > data.json
```

### Filtering

```bash
# Filter by date range
summa --from 2024-03-01 --to 2024-03-31 logfile.md

# Filter by specific tag
summa --tag meeting logfile.md

# Combine filters
summa --from 2024-03-01 --tag coding logfile.md

# Filter with weekly summary
summa -w --tag urgent logfile.md
```

### Advanced Examples

```bash
# Monthly summary for Q1 2024
summa -m --from 2024-01-01 --to 2024-03-31 logfile.md

# Export all coding time to CSV
summa -f csv --tag coding logfile.md > coding_time.csv

# Weekly report for meetings in JSON
summa -w -f json --tag meeting logfile.md > meetings.json

# Verbose mode for debugging
summa -v logfile.md
```

### Directory Scanning

```bash
# Scan a directory for time logs
summa --scan ~/logs

# Recursive scan with date inference
summa -S ~/notes -R --date-from-filename --date-from-path

# Scan only markdown files
summa --scan ~/work --include .md --recursive

# Generate weekly report from all logs in directory
summa -S ~/logs -R -w --from 2024-01-01
```

## Documentation

Full documentation is available via the man page:

```bash
man summa
```

## Command-Line Options

| Option      | Long Form              | Description                                    |
| ----------- | ---------------------- | ---------------------------------------------- |
| `-h`        | `--help`               | Show help message                              |
| `-f FORMAT` | `--format FORMAT`      | Output format: text, csv, json (default: text) |
| `-d`        | `--daily`              | Show daily summary                             |
| `-w`        | `--weekly`             | Show weekly summary                            |
| `-m`        | `--monthly`            | Show monthly summary                           |
| `-v`        | `--verbose`            | Enable verbose output for debugging            |
| `-S PATH`   | `--scan PATH`          | Scan directory/file for time logs              |
| `-R`        | `--recursive`          | Scan directories recursively                   |
|             | `--date-from-filename` | Extract dates from filenames                   |
|             | `--date-from-path`     | Extract dates from directory paths             |
|             | `--include PATTERN`    | Include only files matching pattern            |
|             | `--exclude PATTERN`    | Exclude files matching pattern                 |
|             | `--from DATE`          | Filter entries from DATE (YYYY-MM-DD)          |
|             | `--to DATE`            | Filter entries to DATE (YYYY-MM-DD)            |
|             | `--tag TAG`            | Filter entries by TAG (without #)              |

## Output Examples

### Text Summary (Default)

```
Total entries: 47

=== TAG SUMMARY ===
#coding: 12h 30m (8 entries)
#meeting: 6h 45m (12 entries)
#admin: 3h 15m (10 entries)
#break: 5h 00m (5 entries)

Total tracked time: 27h 30m
```

### Daily Summary (-d)

```
=== DAILY SUMMARY ===

2024-03-15:  8h 00m (6 entries)
2024-03-16:  3h 30m (3 entries)
2024-03-17:  7h 15m (5 entries)

Total days: 3
Total entries: 14
Total time: 18h 45m
Average per day: 6h 15m
```

### Weekly Summary (-w)

```
=== WEEKLY SUMMARY ===

2024 Week 11 (2024-03-11 to 2024-03-17): 42h 30m (35 entries)
2024 Week 12 (2024-03-18 to 2024-03-24): 38h 15m (32 entries)

Total weeks: 2
Total entries: 67
Total time: 80h 45m
Average per week: 40h 22m
```

### Monthly Summary (-m)

```
=== MONTHLY SUMMARY ===

2024 March: 168h 30m (143 entries across 22 days)
2024 April: 156h 45m (138 entries across 20 days)

Total months: 2
Total days with entries: 42
Total entries: 281
Total time: 325h 15m
Average per month: 162h 37m
Average per working day: 7h 44m
```

## Validation and Error Handling

Summa performs comprehensive validation and provides helpful warnings:

- **Invalid Dates**: Detects impossible dates (e.g., February 30)
- **Invalid Times**: Validates hours (0-23) and minutes (0-59)
- **Backwards Time Spans**: Detects when end time is before start time
- **Invalid Percentages**: Ensures percentages are between 0-100
- **Line Numbers**: All warnings include line numbers for easy debugging

### Example Warnings

```
Line 42: Warning: Invalid date 2024-02-30, using current date
Line 78: Warning: Invalid percentage 150% (must be 0-100)
Line 92: Warning: Invalid time span 0900-0800 (backwards or >20 hours)
```

## Performance

Summa is designed for speed and can process large log files efficiently:

- Parses 1,000 entries in ~5ms
- Handles files with thousands of entries
- Minimal memory footprint
- Line-by-line processing for large files

## Tips and Best Practices

1. **Consistent Date Headers**: Always use `# YYYY-MM-DD` format for dates
2. **Tag Organization**: Use consistent, meaningful tags for better analysis
3. **Percentage Usage**: Use percentages to track effort or focus level
4. **Regular Summaries**: Generate weekly/monthly reports for insights
5. **Filtering**: Combine filters to drill down into specific activities
6. **Automation**: Use cron jobs to generate regular reports

## Testing

```bash
# Run the test suite
make test

# Or run directly
./test_summa.sh
```

## Planned Enhancements

### SQLite Database Support (Coming Soon)

A comprehensive plan has been developed to add optional SQLite database storage to Summa, which will enable:

- **Persistent Storage**: Store parsed entries in a local database for long-term analysis
- **Advanced Querying**: Use SQL to perform complex queries across your entire time tracking history
- **Multi-file Management**: Import and manage entries from multiple log files
- **Performance**: Handle datasets with 100,000+ entries efficiently
- **Data Export**: Export to various formats including SQL dumps
- **Backward Compatibility**: All existing features will continue to work without a database

See [SQLITE_PLAN.md](SQLITE_PLAN.md) for the detailed implementation plan.

### Intelligent Directory Scanning (Planned)

A comprehensive system for automatically discovering and parsing time entries across entire directory structures:

- **Auto-Discovery**: Automatically find all files containing time entries in a directory tree
- **Smart Date Inference**: Intelligently deduce dates from filenames, paths, and context when headers are missing
- **Multiple Format Support**: Handle various file types (.md, .txt, .log) and naming conventions
- **Flexible Structure**: Support different organizational patterns (flat, year/month/day, project-based)
- **Conflict Resolution**: Handle duplicate entries and conflicting date information gracefully
- **Performance**: Parallel processing and caching for efficient scanning of large directories

Example usage:

```bash
summa --scan-dir ~/notes --date-from-filename  # Scan with date extraction
summa -S ~/work --recursive --parallel         # Fast recursive scan
```

See [DIRECTORY_SCAN_PLAN.md](DIRECTORY_SCAN_PLAN.md) for the detailed implementation plan.

## Contributing

Contributions are welcome! Please ensure:

1. Code follows existing style conventions
2. All tests pass
3. New features include tests
4. Documentation is updated

## License

MIT License

Copyright (c) 2024 jw4

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABOTY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Support

For issues, questions, or suggestions, please [open an issue](https://github.com/jw4/summa/issues).

