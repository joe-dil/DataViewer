CC = gcc

INCLUDES = -Iinclude \
           -Iinclude/util \
           -Iinclude/memory \
           -Iinclude/core \
           -Iinclude/ui \
           -Iinclude/app

CFLAGS_BASE = -Wall -Wextra -std=c99 $(INCLUDES)
CFLAGS_RELEASE = $(CFLAGS_BASE) -O3 -flto -DNDEBUG
CFLAGS_DEBUG = $(CFLAGS_BASE) -g -O0 -DDEBUG -fsanitize=address
CFLAGS = $(CFLAGS_RELEASE)
LIBS = -lncurses -lpthread
SRCDIR = src
OBJDIR = obj
BINDIR = bin
DEPDIR = deps
TARGET = bin/dv

# Source files discovery
ALL_SOURCES   = $(shell find $(SRCDIR) -name '*.c')
LIB_SOURCES   = $(filter-out $(SRCDIR)/app/main.c, $(ALL_SOURCES))
MAIN_SOURCE   = $(SRCDIR)/app/main.c

# Object and dependency lists
LIB_OBJECTS   = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(LIB_SOURCES))
MAIN_OBJECT   = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(MAIN_SOURCE))
DEPENDENCIES  = $(patsubst $(SRCDIR)/%.c,$(DEPDIR)/%.d,$(ALL_SOURCES))

# Default run arguments
ARGS ?= data/real_estate.csv
CONFIG_FILE ?= dsv_config.conf

.PHONY: all clean test valgrind release debug run help

# Default target is release build
all: $(TARGET)

# Release build (optimized)
release: CFLAGS = $(CFLAGS_RELEASE)
release: $(TARGET)

# Debug build (with debugging symbols and address sanitizer)
debug: CFLAGS = $(CFLAGS_DEBUG)
debug: LIBS += -fsanitize=address
debug: $(TARGET)

# Main application binary
$(TARGET): $(MAIN_OBJECT) $(LIB_OBJECTS)
	@mkdir -p $(@D)
	$(CC) $^ -o $(TARGET) $(LIBS)

# Object compilation with automatic dependency generation
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@) $(dir $(DEPDIR)/$*.d)
	$(CC) $(CFLAGS) -MMD -MP -MF $(DEPDIR)/$*.d -c $< -o $@

# Include dependency files if they exist
-include $(DEPENDENCIES)

clean:
	@rm -rf $(OBJDIR) $(TARGET) $(DEPDIR)

test:
	@echo "Running tests..."
	@$(MAKE) -C tests test

valgrind:
	@echo "Running tests with Valgrind..."
	@$(MAKE) -C tests valgrind

# Install dependencies (for systems that need ncurses dev packages)
install-deps:
	@echo "Installing ncurses development libraries..."
	@if command -v brew >/dev/null 2>&1; then \
		echo "macOS detected, using Homebrew..."; \
		brew install ncurses; \
	elif command -v apt-get >/dev/null 2>&1; then \
		echo "Debian/Ubuntu detected..."; \
		sudo apt-get update && sudo apt-get install libncurses5-dev libncurses5; \
	elif command -v yum >/dev/null 2>&1; then \
		echo "RHEL/CentOS detected..."; \
		sudo yum install ncurses-devel; \
	else \
		echo "Please install ncurses development libraries for your system"; \
	fi

# Help target
help:
	@echo "Usage: make [target]"
	@echo "Targets:"
	@echo "  all        - Build release version (default)"
	@echo "  release    - Build optimized release version"
	@echo "  debug      - Build debug version with address sanitizer"
	@echo "  clean      - Remove all build artifacts"
	@echo "  test       - Build and run all unit tests"
	@echo "  valgrind   - Run unit tests with Valgrind"
	@echo "  help       - Show this help message"

run: all
	./$(TARGET) $(ARGS) --config $(CONFIG_FILE)

.PHONY: all clean test valgrind install-deps release debug help run 