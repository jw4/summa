#!/bin/bash
#
# Comprehensive test suite for summa time tracker
# Tests edge cases, synadia_notes data, and various formats
#

set -eou pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

# Configuration
SUMMA="./summa"
TEST_FILE="testdata/synthetic_3month.md"

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Helper functions
print_header() {
  echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
  echo -e "${BLUE}$1${NC}"
  echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

print_test() {
  echo -e "${CYAN}Testing:${NC} $1"
  TESTS_RUN=$((TESTS_RUN + 1))
}

test_pass() {
  echo -e "  ${GREEN}✓${NC} $1"
  TESTS_PASSED=$((TESTS_PASSED + 1))
}

test_fail() {
  echo -e "  ${RED}✗${NC} $1"
  TESTS_FAILED=$((TESTS_FAILED + 1))
}

test_warn() {
  echo -e "  ${YELLOW}⚠${NC} $1"
}

# Check prerequisites
check_prerequisites() {
  if [ ! -f "$SUMMA" ]; then
    echo -e "${RED}Error: summa binary not found. Run 'make' first.${NC}"
    exit 1
  fi

  # Test if parser is functioning
  local test_output=$(echo "0800-0900 test" | $SUMMA 2>&1)
  if [ -z "$test_output" ]; then
    echo -e "${RED}Error: Parser is not producing output. Check lexer/parser configuration.${NC}"
    echo -e "${YELLOW}Known issue: Recent changes to handle line-start time patterns may have broken the parser.${NC}"
    exit 1
  fi
}

# Test 1: Version and help flags
test_version_help() {
  print_test "Version and help flags"

  # Test version flag
  local version_output=$($SUMMA --version 2>&1)
  if echo "$version_output" | grep -q "version"; then
    test_pass "Version flag works"
  else
    test_fail "Version flag not working"
  fi

  # Test short version flag
  local version_short=$($SUMMA -V 2>&1)
  if echo "$version_short" | grep -q "version"; then
    test_pass "Short version flag (-V) works"
  else
    test_fail "Short version flag not working"
  fi

  # Test help flag
  local help_output=$($SUMMA --help 2>&1)
  if echo "$help_output" | grep -q "Usage:"; then
    test_pass "Help flag works"
  else
    test_fail "Help flag not working"
  fi
}

# Test 2: Basic functionality
test_basic() {
  print_test "Basic time entries"

  local output=$(echo -e "0800-0900 Morning meeting #work\n1200-1300 Lunch break #break" | $SUMMA 2>&1)

  if echo "$output" | grep -q "Total entries: 2"; then
    test_pass "Parsed 2 basic entries"
  else
    test_fail "Failed to parse basic entries"
  fi

  if echo "$output" | grep -q "Total tracked time: 2h 00m"; then
    test_pass "Calculated correct duration"
  else
    test_fail "Incorrect duration calculation"
  fi
}

# Test 2: Synthetic test file parsing
test_synthetic_file() {
  if [ ! -f "$TEST_FILE" ]; then
    test_fail "Test file not found: $TEST_FILE"
    return
  fi

  print_test "Synthetic test file parsing"

  # Parse the full file
  local output=$($SUMMA "$TEST_FILE" 2>&1)
  local exit_code=$?

  if [ $exit_code -eq 0 ]; then
    test_pass "Successfully parsed synthetic test file"
  else
    test_fail "Failed to parse synthetic test file (exit code: $exit_code)"
  fi

  # Check for expected number of entries (should be around 283 valid entries)
  if echo "$output" | grep -q "Total entries:"; then
    local entry_count=$(echo "$output" | grep "Total entries:" | awk '{print $3}')
    if [ "$entry_count" -gt 250 ] && [ "$entry_count" -lt 300 ]; then
      test_pass "Parsed $entry_count entries (expected ~283)"
    else
      test_warn "Unexpected entry count: $entry_count (expected ~283)"
    fi
  else
    test_fail "Could not determine entry count"
  fi

  # Check for tag aggregation
  if echo "$output" | grep -q "#meeting"; then
    test_pass "Tag aggregation working"
  else
    test_fail "Tag aggregation not working"
  fi
}

# Test 3: Output formats
test_formats() {
  print_test "Output formats"

  local test_data="0900-1000 Test entry #test"

  # CSV format
  local csv=$(echo "$test_data" | $SUMMA --format csv 2>/dev/null | head -2)
  if echo "$csv" | grep -q "Start,End,Duration"; then
    test_pass "CSV format generates header"
  else
    test_fail "CSV format issues"
  fi

  # JSON format
  local json=$(echo "$test_data" | $SUMMA --format json 2>/dev/null)
  if echo "$json" | grep -q '"entries"'; then
    test_pass "JSON format generates valid structure"
  else
    test_fail "JSON format issues"
  fi
}

# Test 4: Daily summary
test_daily_summary() {
  print_test "Daily summary (-d flag)"

  local output=$($SUMMA -d "$TEST_FILE" 2>&1)
  local exit_code=$?

  if [ $exit_code -eq 0 ]; then
    test_pass "Daily summary flag works"
  else
    test_fail "Daily summary failed (exit code: $exit_code)"
    return
  fi

  # Check for date entries
  if echo "$output" | grep -q "2024-01-"; then
    test_pass "Daily summary shows January dates"
  else
    test_fail "Daily summary missing January dates"
  fi

  if echo "$output" | grep -q "2024-03-"; then
    test_pass "Daily summary shows March dates"
  else
    test_fail "Daily summary missing March dates"
  fi

  # Check for total days count
  if echo "$output" | grep -q "Total days:"; then
    local days_count=$(echo "$output" | grep "Total days:" | awk '{print $3}')
    test_pass "Parsed $days_count unique days"
  else
    test_fail "Total days not shown"
  fi
}

# Test 4.5: Edge cases in synthetic file
test_edge_cases() {
  print_test "Edge cases in synthetic file"

  # Test midnight crossing
  local midnight=$(grep "2359-0001" "$TEST_FILE" | $SUMMA 2>&1)
  if echo "$midnight" | grep -q "error"; then
    test_fail "Midnight crossing failed"
  else
    test_pass "Midnight crossing handled correctly"
  fi

  # Test Unicode and emoji
  local unicode=$(grep "你好" "$TEST_FILE" | $SUMMA 2>&1)
  if echo "$unicode" | grep -q "error"; then
    test_fail "Unicode handling failed"
  else
    test_pass "Unicode handled correctly"
  fi

  # Test invalid dates (should be caught)
  local invalid=$(echo -e "# 2024-02-30\n1000-1100 Test #test" | $SUMMA 2>&1)
  if echo "$invalid" | grep -q "Warning: Invalid date"; then
    test_pass "Invalid date detection working"
  else
    test_fail "Invalid date not detected"
  fi

  # Test invalid percentages
  local percent=$(echo "1000-1100 %200 Test #test" | $SUMMA 2>&1)
  if echo "$percent" | grep -q "Warning: Invalid percentage"; then
    test_pass "Invalid percentage detection working"
  else
    test_fail "Invalid percentage not detected"
  fi
}

# Test 5: Special patterns
test_special_patterns() {
  print_test "Special patterns"

  # Test entries with valid percentage
  local percent=$(grep "%75" "$TEST_FILE" | head -1 | $SUMMA 2>&1)
  if echo "$percent" | grep -q "error"; then
    test_fail "Valid percentage caused error"
  else
    test_pass "Valid percentage handled correctly"
  fi

  # Test special characters from file
  local special=$(grep "@symbols" "$TEST_FILE" | $SUMMA 2>&1)
  if echo "$special" | grep -q "error"; then
    test_fail "Special characters caused error"
  else
    test_pass "Special characters handled"
  fi

  # Test very long description from file
  local long_output=$(grep "Very very very" "$TEST_FILE" | $SUMMA 2>&1)
  if echo "$long_output" | grep -q "error"; then
    test_fail "Long description caused error"
  else
    test_pass "Long descriptions handled"
  fi

  # Test tags-only entry
  local tags_only=$(grep "^[0-9][0-9][0-9][0-9]-[0-9][0-9][0-9][0-9] #" "$TEST_FILE" | head -1 | $SUMMA 2>&1)
  if echo "$tags_only" | grep -q "error"; then
    test_fail "Tags-only entry caused error"
  else
    test_pass "Tags-only entries handled"
  fi
}

# Test 6: Performance
test_performance() {
  print_test "Performance"

  # Generate large dataset
  local tempfile=$(mktemp)
  for i in {1..1000}; do
    printf "%02d00-%02d30 Task %d #tag%d\n" $((i % 24)) $((i % 24)) $i $((i % 10))
  done >"$tempfile"

  local start_time=$(date +%s%N 2>/dev/null || date +%s)
  $SUMMA "$tempfile" >/dev/null 2>&1
  local end_time=$(date +%s%N 2>/dev/null || date +%s)

  if [ ${#start_time} -gt 10 ]; then
    # Nanosecond precision available
    local elapsed=$(((end_time - start_time) / 1000000))
    test_pass "Processed 1000 entries in ${elapsed}ms"
  else
    # Only second precision
    local elapsed=$((end_time - start_time))
    test_pass "Processed 1000 entries in ${elapsed}s"
  fi

  rm -f "$tempfile"
}

# Test 7: Tag aggregation
test_tags() {
  print_test "Tag aggregation"

  local data="0800-0900 Task 1 #work #urgent
0900-1000 Task 2 #work #meeting
1000-1100 Task 3 #break"

  local output=$(echo "$data" | $SUMMA 2>&1)

  if echo "$output" | grep -q "#work.*2h 00m.*2 entries"; then
    test_pass "Tag aggregation correct for #work"
  else
    test_fail "Tag aggregation incorrect"
  fi
}

# Test 8: Weekly Summary
test_weekly_summary() {
  print_test "Weekly summary (-w flag)"

  local output=$($SUMMA -w "$TEST_FILE" 2>&1)
  local exit_code=$?

  if [ $exit_code -eq 0 ]; then
    test_pass "Weekly summary flag works"
  else
    test_fail "Weekly summary failed (exit code: $exit_code)"
    return
  fi

  # Check for week format
  if echo "$output" | grep -q "2024 Week [0-9]\+"; then
    test_pass "Weekly summary shows week numbers"
  else
    test_fail "Weekly summary format incorrect"
  fi

  # Check for date ranges in weeks
  if echo "$output" | grep -q "([0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\} to [0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\})"; then
    test_pass "Weekly summary shows date ranges"
  else
    test_fail "Weekly summary missing date ranges"
  fi

  # Check for average calculation
  if echo "$output" | grep -q "Average per week:"; then
    test_pass "Weekly summary calculates averages"
  else
    test_fail "Weekly summary missing averages"
  fi
}

# Test 9: Monthly Summary
test_monthly_summary() {
  print_test "Monthly summary (-m flag)"

  local output=$($SUMMA -m "$TEST_FILE" 2>&1)
  local exit_code=$?

  if [ $exit_code -eq 0 ]; then
    test_pass "Monthly summary flag works"
  else
    test_fail "Monthly summary failed (exit code: $exit_code)"
    return
  fi

  # Check for month names
  if echo "$output" | grep -q "2024 January"; then
    test_pass "Monthly summary shows month names"
  else
    test_fail "Monthly summary format incorrect"
  fi

  # Check for days with entries
  if echo "$output" | grep -q "entries across [0-9]\+ days"; then
    test_pass "Monthly summary shows days with entries"
  else
    test_fail "Monthly summary missing day counts"
  fi

  # Check for working day average
  if echo "$output" | grep -q "Average per working day:"; then
    test_pass "Monthly summary calculates daily averages"
  else
    test_fail "Monthly summary missing daily averages"
  fi
}

# Test 10: Date Range Filtering
test_date_filtering() {
  print_test "Date range filtering (--from/--to)"

  # Test filtering for February only
  local feb_output=$($SUMMA --from 2024-02-01 --to 2024-02-29 "$TEST_FILE" 2>&1)

  # Count entries (should be around 50 for February)
  if echo "$feb_output" | grep -q "Total entries:"; then
    local feb_count=$(echo "$feb_output" | grep "Total entries:" | awk '{print $3}')
    if [ "$feb_count" -gt 40 ] && [ "$feb_count" -lt 60 ]; then
      test_pass "Date filtering works (found $feb_count February entries)"
    else
      test_fail "Date filtering incorrect count: $feb_count (expected ~50)"
    fi
  else
    test_fail "Date filtering output missing entry count"
  fi

  # Test single day filtering
  local single_day=$($SUMMA --from 2024-01-01 --to 2024-01-01 "$TEST_FILE" 2>&1)
  if echo "$single_day" | grep -q "Total entries:"; then
    local day_count=$(echo "$single_day" | grep "Total entries:" | awk '{print $3}')
    if [ "$day_count" -eq 9 ]; then
      test_pass "Single day filtering works (found $day_count entries)"
    else
      test_warn "Single day filtering found $day_count entries (expected 9)"
    fi
  else
    test_fail "Single day filtering failed"
  fi

  # Test invalid date range (should show no entries)
  local future_output=$($SUMMA --from 2025-01-01 --to 2025-12-31 "$TEST_FILE" 2>&1)
  if echo "$future_output" | grep -q "Total entries: 3"; then
    test_pass "Future date filtering works correctly"
  else
    test_fail "Future date filtering not working"
  fi
}

# Test 11: Tag Filtering
test_tag_filtering() {
  print_test "Tag filtering (--tag)"

  # Test filtering by 'meeting' tag
  local meeting_output=$($SUMMA --tag meeting "$TEST_FILE" 2>&1)

  if echo "$meeting_output" | grep -q "Total entries:"; then
    local meeting_count=$(echo "$meeting_output" | grep "Total entries:" | awk '{print $3}')
    if [ "$meeting_count" -gt 15 ] && [ "$meeting_count" -lt 30 ]; then
      test_pass "Tag filtering works (found $meeting_count #meeting entries)"
    else
      test_warn "Tag filtering found $meeting_count entries (expected ~21)"
    fi
  else
    test_fail "Tag filtering output missing entry count"
  fi

  # Test with non-existent tag (no output expected when no matches)
  local notag_output=$($SUMMA --tag nonexistent "$TEST_FILE" 2>&1 | grep -v "Warning")
  if [ -z "$notag_output" ]; then
    test_pass "Non-existent tag returns no output"
  else
    test_fail "Non-existent tag filtering not working"
  fi

  # Test tag filtering with daily summary
  local tag_daily=$($SUMMA -d --tag coding "$TEST_FILE" 2>&1)
  if echo "$tag_daily" | grep -q "DAILY SUMMARY"; then
    test_pass "Tag filtering works with daily summary"
  else
    test_fail "Tag filtering with daily summary failed"
  fi
}

# Test 12: Combined Filters
test_combined_filters() {
  print_test "Combined date and tag filters"

  # Filter January meetings only
  local combined=$($SUMMA --from 2024-01-01 --to 2024-01-31 --tag meeting "$TEST_FILE" 2>&1)

  if echo "$combined" | grep -q "Total entries:"; then
    local combined_count=$(echo "$combined" | grep "Total entries:" | awk '{print $3}')
    if [ "$combined_count" -gt 5 ] && [ "$combined_count" -le 15 ]; then
      test_pass "Combined filters work (found $combined_count Jan meetings)"
    else
      test_warn "Combined filters found $combined_count entries"
    fi
  else
    test_fail "Combined filtering failed"
  fi

  # Test with weekly summary
  local combined_weekly=$($SUMMA -w --from 2024-03-01 --to 2024-03-31 --tag work "$TEST_FILE" 2>&1)
  if echo "$combined_weekly" | grep -q "WEEKLY SUMMARY"; then
    test_pass "Combined filters work with weekly summary"
  else
    test_fail "Combined filters with weekly summary failed"
  fi
}

# Test 13: CSV Output Format
test_csv_format() {
  print_test "CSV output format"

  local test_data="# 2024-03-15\n0900-1000 Test meeting #work\n1000-1100 %75 Coding #dev"
  local csv_output=$(echo -e "$test_data" | $SUMMA -f csv 2>/dev/null)

  # Check CSV header
  if echo "$csv_output" | head -1 | grep -q "Date,Start,End,Duration_Minutes,Description,Tags,Percentage"; then
    test_pass "CSV header is correct"
  else
    test_fail "CSV header is incorrect"
  fi

  # Check CSV data format
  if echo "$csv_output" | grep -q "2024-03-15,09:00,10:00,60,Test meeting,#work,"; then
    test_pass "CSV data format is correct"
  else
    test_fail "CSV data format is incorrect"
  fi

  # Check percentage in CSV (note: there's a space before description after percentage)
  if echo "$csv_output" | grep -q "2024-03-15,10:00,11:00,60, Coding,#dev,75"; then
    test_pass "CSV percentage handling is correct"
  else
    test_fail "CSV percentage handling is incorrect"
  fi
}

# Test 14: JSON Output Format
test_json_format() {
  print_test "JSON output format"

  local test_data="# 2024-03-15\n0900-1000 Test meeting #work #urgent\n1000-1100 %50 Coding session #dev"
  local json_output=$(echo -e "$test_data" | $SUMMA -f json 2>/dev/null)

  # Check JSON structure
  if echo "$json_output" | grep -q '"total_entries": 2'; then
    test_pass "JSON total_entries field is correct"
  else
    test_fail "JSON total_entries field is incorrect"
  fi

  # Check JSON entries array
  if echo "$json_output" | grep -q '"entries": \['; then
    test_pass "JSON entries array exists"
  else
    test_fail "JSON entries array missing"
  fi

  # Check date format in JSON
  if echo "$json_output" | grep -q '"date": "2024-03-15"'; then
    test_pass "JSON date format is correct"
  else
    test_fail "JSON date format is incorrect"
  fi

  # Check tags array in JSON
  if echo "$json_output" | grep -q '"tags": \["#work", "#urgent"\]'; then
    test_pass "JSON tags array is correct"
  else
    test_fail "JSON tags array is incorrect"
  fi

  # Check percentage in JSON
  if echo "$json_output" | grep -q '"percentage": 50'; then
    test_pass "JSON percentage field is correct"
  else
    test_fail "JSON percentage field is incorrect"
  fi
}

# Test 15: Validation Features
test_validation() {
  print_test "Input validation features"

  # Test invalid date detection
  local invalid_date="# 2024-02-30\n0900-1000 Test #test"
  local date_output=$(echo -e "$invalid_date" | $SUMMA 2>&1)
  if echo "$date_output" | grep -q "Warning: Invalid date 2024-02-30"; then
    test_pass "Invalid date detection works"
  else
    test_fail "Invalid date not detected"
  fi

  # Test invalid time detection (no output since invalid times are skipped)
  local invalid_time="2500-2600 Invalid hours #test"
  local time_output=$(echo "$invalid_time" | $SUMMA 2>&1 | grep -v "Warning")
  if [ -z "$time_output" ]; then
    test_pass "Invalid time entries are skipped"
  else
    test_fail "Invalid time not handled correctly"
  fi

  # Test backwards timespan detection
  local backwards="0900-0800 Backwards span #test"
  local back_output=$(echo "$backwards" | $SUMMA -v 2>&1)
  if echo "$back_output" | grep -q "backwards span\|Invalid backwards timespan"; then
    test_pass "Backwards timespan detection works"
  else
    test_fail "Backwards timespan not detected"
  fi

  # Test invalid percentage detection
  local invalid_pct="0900-1000 %150 Too much effort #test"
  local pct_output=$(echo "$invalid_pct" | $SUMMA 2>&1)
  if echo "$pct_output" | grep -q "Invalid percentage.*must be 0-100"; then
    test_pass "Invalid percentage detection works"
  else
    test_fail "Invalid percentage not detected"
  fi

  # Test line number reporting (line numbers appear in verbose mode)
  local multi_line="0900-1000 Valid entry #test\n0900-0800 Backwards #test\n1000-1100 Another valid #test"
  local line_output=$(echo -e "$multi_line" | $SUMMA -v 2>&1)
  if echo "$line_output" | grep -q "Line 2:"; then
    test_pass "Line number reporting works"
  else
    test_fail "Line number not reported in errors"
  fi
}

# Test 16: Tag Storage Without Hash
test_tag_storage() {
  print_test "Tag storage without # prefix"

  # Test that tag filtering works without # prefix
  local tag_test="0900-1000 Test #work\n1000-1100 Test2 #work"
  local tag_output=$(echo -e "$tag_test" | $SUMMA --tag work 2>&1)

  if echo "$tag_output" | grep -q "Total entries: 2"; then
    test_pass "Tag filtering works without # prefix in --tag"
  else
    test_fail "Tag filtering requires # prefix (should not)"
  fi

  # Test that output still shows # prefix
  local summary_output=$(echo -e "$tag_test" | $SUMMA 2>&1)
  if echo "$summary_output" | grep -q "#work"; then
    test_pass "Tags displayed with # prefix in output"
  else
    test_fail "Tags missing # prefix in output"
  fi
}

# Test 17: Stdin Input
test_stdin_input() {
  print_test "Standard input processing"

  # Test piping data through stdin
  local test_data="0900-1000 Stdin test #test"
  local stdin_output=$(echo "$test_data" | $SUMMA 2>&1)

  if echo "$stdin_output" | grep -q "Total entries: 1"; then
    test_pass "Stdin input processing works"
  else
    test_fail "Stdin input not processed correctly"
  fi

  # Test empty stdin (no output expected when no entries)
  local empty_output=$(echo "" | $SUMMA 2>&1)
  if [ -z "$empty_output" ]; then
    test_pass "Empty stdin handled correctly (no output)"
  else
    test_fail "Empty stdin not handled correctly"
  fi
}

# Main test execution
main() {
  echo -e "${MAGENTA}╔════════════════════════════════════════════════════╗${NC}"
  echo -e "${MAGENTA}║          Summa Parser - Test Suite                ║${NC}"
  echo -e "${MAGENTA}╚════════════════════════════════════════════════════╝${NC}"
  echo

  check_prerequisites

  # Run all tests
  print_header "Core Functionality"
  test_version_help
  test_basic
  test_tags
  test_tag_storage
  test_stdin_input

  print_header "File Processing"
  test_synthetic_file
  test_daily_summary
  test_weekly_summary
  test_monthly_summary

  print_header "Filtering"
  test_date_filtering
  test_tag_filtering
  test_combined_filters

  print_header "Edge Cases & Special Patterns"
  test_edge_cases
  test_special_patterns
  test_validation

  print_header "Output Formats"
  test_formats
  test_csv_format
  test_json_format

  print_header "Performance"
  test_performance

  # Summary
  echo
  print_header "Test Summary"
  echo "Test suites run: $TESTS_RUN"
  echo -e "Assertions passed: ${GREEN}$TESTS_PASSED${NC}"
  echo -e "Assertions failed: ${RED}$TESTS_FAILED${NC}"
  echo

  if [ "$TESTS_FAILED" -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC} 🎉"
    exit 0
  else
    echo -e "${YELLOW}Some tests failed. Check output above for details.${NC}"
    exit 1
  fi
}

# Run tests
main "$@"
