# DSV Viewer

A fast, terminal-based viewer for delimiter-separated value files (CSV, TSV, etc.).

## Quick Start

```bash
# Build optimized release version
make

# Build debug version with address sanitizer
make debug

# View a CSV file
./bin/viewer file.csv

# View with custom delimiter
./bin/viewer file.tsv -d $'\t'
```

## Build System

The project uses an enhanced Makefile with:

- **Incremental compilation**: Only changed files are recompiled
- **Automatic dependency tracking**: Header changes trigger recompilation of dependent files
- **Release/Debug targets**: Optimized production builds and debug builds with sanitizers
- **Clean separation**: Library code separate from main executable

### Build Targets

- `make` or `make release` - Optimized release build
- `make debug` - Debug build with address sanitizer and debugging symbols
- `make clean` - Remove all build artifacts
- `make test` - Run unit tests
- `make help` - Show all available targets

### Dependencies

- **ncurses** - Terminal UI library
- **pthread** - Threading support (linked but not actively used)

## Navigation

- **Arrow Keys** - Navigate between fields
- **Page Up/Down** - Scroll pages
- **Home/End** - Go to beginning/end
- **q** - Quit
- **h** - Help

## Features

- Memory-mapped file loading for large files
- Automatic delimiter detection
- Unicode/UTF-8 support
- Column width analysis
- Terminal resize handling 