CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LIBS = -lncurses
TARGET = viewer
SOURCES = main.c csv_parser.c display.c navigation.c
OBJECTS = $(SOURCES:.c=.o)

# Default target
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

# Compile individual source files
%.o: %.c viewer.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Test with sample data
test: $(TARGET)
	./$(TARGET) test.csv

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
	@echo "Available targets:"
	@echo "  $(TARGET)     - Build the CSV viewer (default)"
	@echo "  clean         - Remove build artifacts"
	@echo "  test          - Build and test with sample data"
	@echo "  debug         - Build with debug symbols"
	@echo "  install-deps  - Install ncurses development libraries"
	@echo "  help          - Show this help message"

.PHONY: clean test install-deps debug help 