# File System Analyzer

## Overview

This is a C-based file system utility that provides comprehensive directory analysis, including:
- Recursive directory traversal
- File type counting
- Duplicate file detection and removal
- Directory size calculation

## Features

- Scan a root directory recursively
- Count and categorize file types
- Identify and remove duplicate files
- Calculate directory size before and after file removal
- Uses multi-processing and inter-process communication techniques

## Prerequisites

- GCC compiler
- POSIX-compliant operating system (Linux/Unix)
- pthread library
- Basic system development libraries

## Compilation

```bash
gcc -o file_analyzer 1thProject-OS.c -lpthread
```

## Usage

1. Compile the program
2. Run the executable
3. Enter the full path of the root directory to analyze

```bash
./file_analyzer
# Prompt will ask you to enter root directory path
```

## Key Functions

- `traverseDirectory()`: Recursively scans directory structure
- `handleDuplicateFiles()`: Removes identified duplicate files
- `calculateDirectorySize()`: Computes total directory size
- `getFileType()`: Extracts file extension

## Inter-Process Communication

The program uses:
- Shared Memory: For storing file information
- Message Queues: For inter-process communication
- Mutex: For thread-safe operations

## Limitations

- Maximum 2000 files can be processed
- Maximum 100 unique file types supported
- Maximum 1000 duplicate files can be tracked

## Contributing

Contributions are welcome! Please submit pull requests or open issues.

## Author

Salman Hashemi
