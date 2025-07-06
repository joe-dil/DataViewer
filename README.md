# DataViewer

A blazingly fast, memory-efficient, terminal-based viewer for CSV files and other delimited data.

## Features

- **High Performance**: Uses memory-mapped I/O to handle files of any size with minimal memory usage.
- **Smart Parsing**: Automatically detects common delimiters (`,`, `;`, `|`, `\t`).
- **Encoding Support**: Correctly renders UTF-8 and Latin-1 encoded files.

## Building

### Dependencies
- A C compiler (`gcc` or `clang`)
- `make`
- `ncurses` development libraries

On macOS (with Homebrew):
```bash
brew install ncurses
```
On Debian/Ubuntu:
```bash
sudo apt-get update && sudo apt-get install build-essential libncurses-dev
```

### Compilation
Build the optimized release version:
```bash
make
```
Build a debug version with address sanitizer:
```bash
make debug
```

## Usage

To view a file:
```bash
./bin/dv <your_file.csv>
```

### In-App Commands
| Key(s)       | Action                         |
|--------------|--------------------------------|
| `q`          | Quit the application           |
| `h`          | Show the help screen           |
| `Arrow Keys` | Navigate between cells/rows    |
| `Page Up/Dn` | Scroll up or down by one page  |
| `Home`/`End` | Jump to the top/bottom of file |
| `y`          | Copy cell content to clipboard |

## Development

Run the test suite:
```bash
make test
```
Clean the build artifacts:
```bash
make clean
```