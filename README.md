# SC3020 Database System - Project 1

## Installation Guide

### Prerequisites
- C compiler (GCC/Clang/MSVC)
- CMake (version 3.10 or higher)

### Build Instructions

1. **Create required directories:**
```bash
mkdir binary # to store binary files
mkdir build
```

2. **Configure and build the project:**
```bash
cd build
cmake ..
cmake --build .
```

3. **Run the application:**
```bash
cd ..
./nba_storage.exe
```

### Directory Structure
- `binary/` - Contains generated binary database files
- `build/` - CMake build files and compiled executables
- `src/` - Source code files
- `include/` - Header files