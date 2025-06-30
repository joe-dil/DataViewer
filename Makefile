CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O3 -flto -Iinclude
LIBS = -lncurses -lpthread
SRCDIR = src
OBJDIR = obj
BINDIR = bin
TARGET = $(BINDIR)/viewer

# All .c files in the src directory, excluding main.c
LIB_SOURCES = $(filter-out $(SRCDIR)/main.c, $(wildcard $(SRCDIR)/*.c))
LIB_OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(LIB_SOURCES))
MAIN_OBJECT = $(OBJDIR)/main.o

.PHONY: all clean test valgrind

all: $(TARGET)

$(TARGET): $(MAIN_OBJECT) $(LIB_OBJECTS)
	@mkdir -p $(@D)
	$(CC) $^ -o $(TARGET) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf $(OBJDIR) $(BINDIR)

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

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Help target
help:
	@echo "Usage: make [target]"
	@echo "Targets:"
	@echo "  all        - Build the DSV viewer (default)"
	@echo "  clean      - Remove build artifacts"
	@echo "  test       - Build and run all unit tests"
	@echo "  valgrind   - Run unit tests with Valgrind"
	@echo "  help       - Show this help message"

.PHONY: clean test valgrind install-deps debug help 