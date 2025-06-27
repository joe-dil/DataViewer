CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -Iinclude
LIBS = -lncurses
SRCDIR = src
OBJDIR = obj
BINDIR = bin
TARGET = $(BINDIR)/viewer
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf $(OBJDIR) $(BINDIR)

test: all
	@./$(TARGET) "Bird Strikes Test.csv"

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
	@echo "  test       - Build and run a test"
	@echo "  help       - Show this help message"

.PHONY: clean test install-deps debug help 