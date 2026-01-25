# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**strgroup** is a Stata plugin that performs fuzzy string matching using the Levenshtein edit distance algorithm. It groups similar strings based on a user-specified similarity threshold. The project is authored by Julian Reif (University of Illinois).

## Architecture

Two-layer design:

1. **Stata ADO layer** (`src/strgroup.ado`, `src/levenshtein.ado`) — user-facing commands that handle syntax parsing, input validation, platform detection, and plugin loading.
2. **C plugin layer** (`src/c/strgroup.c`) — high-performance core implementing the Levenshtein distance algorithm and transitive-closure grouping. Entry point is `stata_call()` with three modes: calculate single distance, compute pairwise distance vectors, and group strings by threshold.

The C plugin uses dynamic programming with prefix/suffix stripping optimization. Grouping works by computing all pairwise normalized distances, building an NxN match matrix, applying transitive closure, then assigning group IDs to connected components.

Platform-specific compiled plugins are stored in `src/` (windows32, windows64, unix, macosx). The ADO script detects the platform at runtime and loads the appropriate `.plugin` file.

## Build Commands

All compilation is done from the `src/c/` directory. Compiled plugins should be copied to `src/` after building.

### Running Cygwin on Windows

Cygwin is typically installed at `C:\cygwin64`. To run commands inside Cygwin from Claude Code, use:

```bash
"C:\\cygwin64\\bin\\bash.exe" -l -c "cd '<project_root>/src/c' && <command>"
```

The `-l` flag loads the login shell environment (required for PATH to include `/usr/bin`). Replace `<project_root>` with the working directory path (use forward slashes).

### Windows 64-bit (Cygwin + mingw)

Run from `src/c/`:
```bash
x86_64-w64-mingw32-gcc -shared stplugin.c strgroup.c -O3 -funroll-loops -o strgroup.windows64.plugin
```

Then copy to `src/`:
```bash
cp src/c/strgroup.windows64.plugin src/strgroup.windows64.plugin
```

### Windows 32-bit (Cygwin)

Run from `src/c/`:
```bash
gcc -shared -mno-cygwin stplugin.c strgroup.c -O3 -funroll-loops -o strgroup.windows32.plugin
```

### Unix 64-bit

Run from `src/c/`:
```bash
gcc -shared -fPIC -DSYSTEM=OPUNIX stplugin.c strgroup.c -O3 -funroll-loops -o strgroup.unix.plugin
```

### macOS (universal binary)

Run from `src/c/`:
```bash
clang -bundle -o strgroup.macosx.x86_64 stplugin.c strgroup.c -DSYSTEM=APPLEMAC -target x86_64-apple-macos10.11
clang -bundle -o strgroup.macosx.arm64 stplugin.c strgroup.c -DSYSTEM=APPLEMAC -target arm64-apple-macos11
lipo -create -output strgroup.macosx.plugin strgroup.macosx.x86_64 strgroup.macosx.arm64
```

## Running Stata on Windows

Stata is typically located at `C:\Program Files\Stata19\StataMP-64.exe`. To run a `.do` file in batch mode, use PowerShell:

```bash
powershell.exe -Command "Start-Process -FilePath 'C:\Program Files\Stata19\StataMP-64.exe' -ArgumentList '/e do script.do' -WorkingDirectory '<directory>' -Wait -NoNewWindow"
```

- The `/e` flag tells Stata to execute the script and exit (batch mode).
- `-Wait` ensures the command blocks until Stata finishes.
- `-WorkingDirectory` sets the working directory for the Stata session (e.g., the `test/` subdirectory of the project).
- Output is written to a `.log` file in the working directory with the same base name as the `.do` file (e.g., `timing.do` produces `timing.log`).

## Testing

Tests use Stata's `cscript` framework. Run via Stata:
```stata
do test/examples.do
```

Expected outputs for regression comparison are in `test/compare/` (`.dta` files).

## Key Implementation Details

- String comparisons are byte-based (multi-byte Unicode characters are counted per-byte)
- Maximum string length: 2045 characters (Stata limit)
- O(N²) memory for match matrices; warns when N > 10,000 (overridable with `force` option)
- The `first` option partitions strings by initial character to reduce comparisons
- Normalization modes: `shorter` (default), `longer`, or `none`
- Case-sensitive matching; missing strings are never matched
- The `by:` prefix is supported for group-level processing

## Package Distribution

- `stata.toc` and `strgroup.pkg` define the Stata package manifest for `net install`
- Installation: `net install strgroup, from("https://raw.githubusercontent.com/reifjulian/strgroup/master") replace`