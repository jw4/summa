# Summa Time Tracker Makefile
# ============================

# Program name
TARGET := summa

# Version from git
GIT_VERSION := $(shell git describe --abbrev=4 --dirty --always --tags 2>/dev/null || echo "unknown")

# Installation directories
PREFIX ?= $(HOME)
BINDIR := $(PREFIX)/bin

# Compiler settings
CC ?= cc
CFLAGS := -Wall -Wextra -std=c99
CPPFLAGS := -DVERSION=\"$(GIT_VERSION)\"
LDFLAGS :=

# Build mode (debug or release)
BUILD_MODE ?= release

ifeq ($(BUILD_MODE),debug)
	CFLAGS += -g -O0 -DDEBUG
else
	CFLAGS += -O2
endif

# Source and object files
SRCS := summa.c summa_scan.c
OBJS := $(SRCS:.c=.o)

# Default target
.DEFAULT_GOAL := build

# ============= Main Targets =============

.PHONY: help
help:
	@echo "Summa Build System"
	@echo "=================="
	@echo ""
	@echo "Usage: make [target]"
	@echo ""
	@echo "Main targets:"
	@echo "  build       Build the summa executable (default)"
	@echo "  test        Run the test suite"
	@echo "  install     Install to $(BINDIR)"
	@echo "  uninstall   Remove from $(BINDIR)"
	@echo "  clean       Remove build artifacts"
	@echo ""
	@echo "Build options:"
	@echo "  debug       Build with debugging symbols"
	@echo "  release     Build optimized version"
	@echo ""
	@echo "Variables:"
	@echo "  PREFIX=dir  Installation prefix (default: $(PREFIX))"
	@echo "  CC=compiler C compiler (default: $(CC))"

.PHONY: build
build: $(TARGET)

.PHONY: all
all: build

# ============= Build Rules =============

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@
	@echo "Built $(TARGET) ($(BUILD_MODE) mode)"

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# ============= Build Modes =============

.PHONY: debug
debug:
	$(MAKE) BUILD_MODE=debug clean build

.PHONY: release
release:
	$(MAKE) BUILD_MODE=release clean build

# ============= Testing =============

.PHONY: test
test: build
	@echo "Running test suite..."
	@./test_summa.sh

.PHONY: check
check: test

# ============= Installation =============

.PHONY: install
install: build
	@echo "Installing $(TARGET) to $(BINDIR)..."
	@mkdir -p $(BINDIR)
	@cp $(TARGET) $(BINDIR)/
	@chmod 755 $(BINDIR)/$(TARGET)
	@echo "Installation complete"

.PHONY: uninstall
uninstall:
	@echo "Removing $(TARGET) from $(BINDIR)..."
	@rm -f $(BINDIR)/$(TARGET)
	@echo "Uninstall complete"

# ============= Maintenance =============

.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -f $(TARGET) $(OBJS)
	@echo "Clean complete"

.PHONY: distclean
distclean: clean
	@rm -f *~ *.bak

# ============= Development Helpers =============

.PHONY: run-example
run-example: build
	@echo "# 2024-01-15" | ./$(TARGET)
	@echo "0900-1000 Team meeting #meeting" | ./$(TARGET)
	@echo "1000-1200 Development #coding" | ./$(TARGET)

.PHONY: valgrind
valgrind: debug
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET) testdata/synthetic_3month.md

.PHONY: format
format:
	@echo "Note: No automatic formatting for C files"
	@echo "Please ensure code follows project style guidelines"

# ============= Dependencies =============

summa.o: summa.c

# Print Makefile variables for debugging
.PHONY: print-%
print-%:
	@echo '$*=$($*)'