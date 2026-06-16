# Use platform-appropriate shell
ifeq ($(OS),Windows_NT)
  SHELL = cmd.exe
else
  SHELL = /bin/sh
endif

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -Iinclude

# Directories
SRC_DIR = src
TEST_DIR = tests
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = .

# Source files
SRC_FILES = $(SRC_DIR)/elevator.c $(SRC_DIR)/guardrails.c
SRC_OBJECTS = $(BUILD_DIR)/elevator.o $(BUILD_DIR)/guardrails.o

# Main program
MAIN_EXECUTABLE = $(BIN_DIR)/elevator_guardrails
MAIN_OBJECTS = $(SRC_OBJECTS) $(BUILD_DIR)/main.o

# Simple tests (no dependencies)
SIMPLE_EXECUTABLE = $(BIN_DIR)/test_simple
SIMPLE_OBJECTS = $(SRC_OBJECTS) $(BUILD_DIR)/test_simple.o

# CUnit tests (optional)
CUNIT_EXECUTABLE = $(BIN_DIR)/test_elevator_guardrails
CUNIT_OBJECTS = $(SRC_OBJECTS) $(BUILD_DIR)/test_elevator.o $(BUILD_DIR)/test_guardrails.o $(BUILD_DIR)/test_runner.o
CUNIT_LDFLAGS = -lm -lcunit

.PHONY: all test test-simple test-cunit run clean check-guardrails help

all: $(MAIN_EXECUTABLE) $(SIMPLE_EXECUTABLE)

# Create build directory
$(BUILD_DIR):
ifeq ($(OS),Windows_NT)
	@if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
else
	@mkdir -p $(BUILD_DIR)
endif

# Compilation rules for source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compilation rules for test files
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compilation rules for main
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Build main executable
$(MAIN_EXECUTABLE): $(MAIN_OBJECTS)
	@echo "Linking main executable..."
	$(CC) $(CFLAGS) -o $@ $^ -lm
	@echo "✓ Build complete: $@"

# Build simple test executable (no dependencies)
$(SIMPLE_EXECUTABLE): $(SIMPLE_OBJECTS)
	@echo "Linking simple test executable..."
	$(CC) $(CFLAGS) -o $@ $^ -lm
	@echo "✓ Build complete: $@"

# Build CUnit test executable (requires libcunit)
$(CUNIT_EXECUTABLE): $(CUNIT_OBJECTS)
	@echo "Linking CUnit test executable..."
	$(CC) $(CFLAGS) -o $@ $^ $(CUNIT_LDFLAGS)
	@echo "✓ Build complete: $@"

# Run simple tests (no dependencies)
test-simple: $(SIMPLE_EXECUTABLE)
	@echo "Running simple tests..."
	./$(SIMPLE_EXECUTABLE)

# Run CUnit tests (requires libcunit)
test-cunit: $(CUNIT_EXECUTABLE)
	@echo "Running CUnit tests..."
	./$(CUNIT_EXECUTABLE)

# Run simple tests by default
test: test-simple

# Run main program
run: $(MAIN_EXECUTABLE)
	@echo "Running main program..."
	./$(MAIN_EXECUTABLE)

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
ifeq ($(OS),Windows_NT)
	@if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)
	@if exist $(MAIN_EXECUTABLE) del $(MAIN_EXECUTABLE)
	@if exist $(SIMPLE_EXECUTABLE) del $(SIMPLE_EXECUTABLE)
	@if exist $(CUNIT_EXECUTABLE) del $(CUNIT_EXECUTABLE)
else
	rm -rf $(BUILD_DIR)
	rm -f $(MAIN_EXECUTABLE) $(SIMPLE_EXECUTABLE) $(CUNIT_EXECUTABLE)
endif
	@echo "✓ Clean complete"

# Check guardrails integrity
check-guardrails:
	@echo "Checking guardrails.c integrity..."
ifeq ($(OS),Windows_NT)
	@if not exist reference\reference_guardrails.c (echo ERROR: reference/reference_guardrails.c not found! && exit /b 1)
	@if not exist $(SRC_DIR)\guardrails.c (echo ERROR: $(SRC_DIR)/guardrails.c not found! && exit /b 1)
	@fc /b $(SRC_DIR)\guardrails.c reference\reference_guardrails.c >nul
	@if errorlevel 1 (echo ERROR: guardrails.c and reference_guardrails.c are DIFFERENT! && echo. && echo File comparison using: && echo fc $(SRC_DIR)\guardrails.c reference\reference_guardrails.c && exit /b 1) else (echo guardrails.c matches reference_guardrails.c)
else
	@if [ ! -f reference/reference_guardrails.c ]; then echo "ERROR: reference/reference_guardrails.c not found!"; exit 1; fi
	@if [ ! -f $(SRC_DIR)/guardrails.c ]; then echo "ERROR: $(SRC_DIR)/guardrails.c not found!"; exit 1; fi
	@if cmp -s $(SRC_DIR)/guardrails.c reference/reference_guardrails.c; then echo "guardrails.c matches reference_guardrails.c"; else echo "ERROR: guardrails.c and reference_guardrails.c are DIFFERENT!"; exit 1; fi
endif

# Help target
help:
	@echo "=========================================="
	@echo "  Elevator Control System - Build Targets"
	@echo "=========================================="
	@echo ""
	@echo "Usage: make [target]"
	@echo ""
	@echo "Main Targets:"
	@echo "  all              - Build main program and tests"
	@echo "  test             - Build and run simple tests (default)"
	@echo "  test-simple      - Build and run simple tests (no dependencies)"
	@echo "  test-cunit       - Build and run CUnit tests (requires libcunit)"
	@echo "  run              - Build and run main program"
	@echo "  check-guardrails - Verify guardrails.c matches reference"
	@echo "  clean            - Remove build artifacts"
	@echo "  help             - Display this message"
	@echo ""
	@echo "Project Structure:"
	@echo "  src/        - Source code (.c files)"
	@echo "  include/    - Header files (.h files)"
	@echo "  tests/      - Test files and test framework"
	@echo "  docs/       - Documentation (.md files)"
	@echo "  build/      - Build artifacts (generated)"
	@echo "  scripts/    - Build and utility scripts"
	@echo "  presentation/ - Presentation files"
	@echo ""
