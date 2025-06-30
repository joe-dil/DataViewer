# Inherit compiler and flags from the parent Makefile for consistency
CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99 -g -O0 -I../include -I.
LIBS ?= -lncurses -lpthread

# Directories
SRCDIR = ../src
TEST_FRAMEWORK_DIR = framework
TEST_UNIT_DIR = unit
OBJDIR = ../obj/test
BINDIR = ../bin

# Test executable
TEST_TARGET = $(BINDIR)/test_runner

# Source files
# Include all application sources EXCEPT main.c
APP_SOURCES = $(filter-out $(SRCDIR)/main.c, $(wildcard $(SRCDIR)/*.c))
# Include all test framework and unit test sources
TEST_SOURCES = $(wildcard $(TEST_FRAMEWORK_DIR)/*.c) $(wildcard $(TEST_UNIT_DIR)/*.c) test_main.c

# Objects
APP_OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/app/%.o,$(APP_SOURCES))
TEST_OBJECTS = $(patsubst %.c,$(OBJDIR)/%.o,$(TEST_SOURCES))

# Phony targets
.PHONY: all test clean valgrind

# Default target
all: $(TEST_TARGET)

# Link the test runner
$(TEST_TARGET): $(APP_OBJECTS) $(TEST_OBJECTS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
	@echo "Test runner built: $@"

# Compile application source files for testing
$(OBJDIR)/app/%.o: $(SRCDIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test source files
$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Run tests
test: all
	@$(TEST_TARGET)

# Run tests with Valgrind for memory leak detection
valgrind: all
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes -s $(TEST_TARGET)

# Clean up test-related build artifacts
clean:
	@rm -rf $(OBJDIR) $(TEST_TARGET)
	@echo "Cleaned test build artifacts." 