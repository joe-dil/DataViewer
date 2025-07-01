CC = gcc
CFLAGS_BASE = -Wall -Wextra -std=c99 -Iinclude
CFLAGS_RELEASE = $(CFLAGS_BASE) -O3 -flto -DNDEBUG
CFLAGS_DEBUG = $(CFLAGS_BASE) -g -O0 -DDEBUG -fsanitize=address
CFLAGS = $(CFLAGS_RELEASE)
LIBS = -lncurses -lpthread
SRCDIR = src
OBJDIR = obj
BINDIR = bin
DEPDIR = deps
TARGET = $(BINDIR)/viewer

# Source files
SOURCES = $(SRCDIR)/main.c $(SRCDIR)/viewer.c $(SRCDIR)/display.c $(SRCDIR)/navigation.c \
          $(SRCDIR)/parser.c $(SRCDIR)/file_io.c $(SRCDIR)/analysis.c $(SRCDIR)/config.c \
          $(SRCDIR)/logging.c $(SRCDIR)/utils.c $(SRCDIR)/cache.c $(SRCDIR)/buffer_pool.c \
          $(SRCDIR)/error_context.c $(SRCDIR)/input_router.c $(SRCDIR)/encoding.c

# All .c files in the src directory, excluding main.c
LIB_SOURCES = $(filter-out $(SRCDIR)/main.c, $(wildcard $(SRCDIR)/*.c))
LIB_OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(LIB_SOURCES))
MAIN_OBJECT = $(OBJDIR)/main.o
DEPENDENCIES = $(patsubst $(SRCDIR)/%.c,$(DEPDIR)/%.d,$(wildcard $(SRCDIR)/*.c))

.PHONY: all clean test valgrind release debug

# Default target is release build
all: release

# Release build (optimized)
release: CFLAGS = $(CFLAGS_RELEASE)
release: $(TARGET)

# Debug build (with debugging symbols and address sanitizer)
debug: CFLAGS = $(CFLAGS_DEBUG)
debug: LIBS += -fsanitize=address
debug: $(TARGET)

$(TARGET): $(MAIN_OBJECT) $(LIB_OBJECTS)
	@mkdir -p $(@D)
	$(CC) $^ -o $(TARGET) $(LIBS)

# Object compilation with automatic dependency generation
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(@D) $(DEPDIR)
	$(CC) $(CFLAGS) -MMD -MP -MF $(DEPDIR)/$*.d -c $< -o $@

# Include dependency files if they exist
-include $(DEPENDENCIES)

clean:
	@rm -rf $(OBJDIR) $(BINDIR) $(DEPDIR)

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

.PHONY: all clean test valgrind install-deps release debug help 