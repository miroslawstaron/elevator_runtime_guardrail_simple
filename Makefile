CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g

# CUnit test files
CUNIT_SOURCES = elevator.c guardrails.c test_elevator.c test_guardrails.c test_runner.c
CUNIT_OBJECTS = $(CUNIT_SOURCES:.c=.o)
CUNIT_EXECUTABLE = test_elevator_guardrails
CUNIT_LDFLAGS = -lm -lcunit

# Simple test files (no external dependencies)
SIMPLE_SOURCES = elevator.c guardrails.c test_simple.c
SIMPLE_OBJECTS = $(patsubst %.c, test_build/%.o, $(SIMPLE_SOURCES))
SIMPLE_EXECUTABLE = test_simple

# Main program files (separate from tests)
MAIN_SOURCES = elevator.c guardrails.c main.c
MAIN_OBJECTS = $(MAIN_SOURCES:.c=.o)
MAIN_EXECUTABLE = elevator_guardrails

.PHONY: all test test-simple test-cunit run clean clean-tests help

all: $(MAIN_EXECUTABLE) $(SIMPLE_EXECUTABLE)

# Build simple test executable (no external dependencies)
test_build:
	@mkdir -p test_build

$(SIMPLE_OBJECTS): test_build/%.o : %.c | test_build
	@echo "Compiling $< for simple tests..."
	$(CC) $(CFLAGS) -c $< -o $@

$(SIMPLE_EXECUTABLE): $(SIMPLE_OBJECTS)
	@echo "Linking simple test executable..."
	$(CC) $(CFLAGS) -o $@ $^ -lm
	@echo "Build complete: $@"

# Build main executable
$(MAIN_EXECUTABLE): $(MAIN_OBJECTS)
	@echo "Linking main executable..."
	$(CC) $(CFLAGS) -o $@ $^ -lm
	@echo "Build complete: $@"

# Build CUnit test executable (requires libcunit)
$(CUNIT_EXECUTABLE): $(CUNIT_OBJECTS)
	@echo "Linking CUnit test executable..."
	$(CC) $(CFLAGS) -o $@ $^ $(CUNIT_LDFLAGS)
	@echo "Build complete: $@"

# Compile C files to object files (for main program)
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Run simple tests (no dependencies)
test-simple: $(SIMPLE_EXECUTABLE)
	@echo "Running simple tests (no external dependencies)..."
	./$(SIMPLE_EXECUTABLE)

# Run CUnit tests (requires libcunit)
test-cunit: $(CUNIT_EXECUTABLE)
	@echo "Running CUnit tests (requires libcunit)..."
	./$(CUNIT_EXECUTABLE)

# Run simple tests by default
test: test-simple

# Run main program
run: $(MAIN_EXECUTABLE)
	./$(MAIN_EXECUTABLE)

# Clean build artifacts
clean: clean-tests
	@echo "Cleaning build artifacts..."
	rm -f $(MAIN_OBJECTS) $(MAIN_EXECUTABLE)
	@echo "Clean complete"

clean-tests:
	@echo "Cleaning test artifacts..."
	rm -f $(CUNIT_OBJECTS) $(SIMPLE_OBJECTS) $(CUNIT_EXECUTABLE) $(SIMPLE_EXECUTABLE)
	rm -rf test_build
	@echo "Test clean complete"

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build main program and simple tests"
	@echo "  test         - Build and run simple tests (no dependencies)"
	@echo "  test-simple  - Build and run simple tests (no dependencies)"
	@echo "  test-cunit   - Build and run CUnit tests (requires libcunit)"
	@echo "  run          - Build and run main program"
	@echo "  clean        - Remove all build artifacts"
	@echo "  clean-tests  - Remove test artifacts only"
	@echo "  help         - Display this help message"
