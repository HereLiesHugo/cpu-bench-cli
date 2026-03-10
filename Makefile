# CPU Benchmark Tool Makefile
# Cross-platform support for Linux, macOS, and Windows (via MinGW)

# Compiler and flags
CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -pthread
LDFLAGS = -pthread -lm

# Platform detection
UNAME_S := $(shell uname -s)

# Platform-specific adjustments
ifeq ($(UNAME_S),Darwin)
    # macOS
    CFLAGS += -DPLATFORM_MACOS=1 -DPLATFORM_LINUX=0 -DPLATFORM_WINDOWS=0
    # macOS needs explicit link for some math functions
    LDFLAGS += -framework CoreFoundation 2>/dev/null || true
else ifeq ($(UNAME_S),Linux)
    # Linux
    CFLAGS += -DPLATFORM_LINUX=1 -DPLATFORM_MACOS=0 -DPLATFORM_WINDOWS=0 -D_GNU_SOURCE
else ifneq ($(findstring MINGW,$(UNAME_S)),)
    # Windows (MinGW)
    CFLAGS += -DPLATFORM_WINDOWS=1 -DPLATFORM_LINUX=0 -DPLATFORM_MACOS=0
    CC = x86_64-w64-mingw32-gcc
    LDFLAGS = -lpthread -lm
else
    # Generic fallback
    CFLAGS += -DPLATFORM_LINUX=1 -DPLATFORM_MACOS=0 -DPLATFORM_WINDOWS=0
endif

# Source files
SOURCES = main.c sysinfo.c workloads.c bench.c score.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = cpu-bench

# Windows executable name
ifeq ($(UNAME_S),Windows_NT)
    TARGET = cpu-bench.exe
endif

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Dependencies
main.o: main.c common.h sysinfo.h workloads.h bench.h score.h
sysinfo.o: sysinfo.c sysinfo.h common.h
workloads.o: workloads.c workloads.h common.h
bench.o: bench.c bench.h common.h sysinfo.h workloads.h
score.o: score.c score.h common.h workloads.h

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Run the benchmark
run: $(TARGET)
	./$(TARGET) --all --verbose

# Run single-core only
run-single: $(TARGET)
	./$(TARGET) --single --verbose

# Run multi-core only
run-multi: $(TARGET)
	./$(TARGET) --multi --verbose

# JSON output test
run-json: $(TARGET)
	./$(TARGET) --all --json

# Rebuild from scratch
rebuild: clean all

# Debug build (no optimization, with debug symbols)
debug:
	$(CC) -std=c11 -O0 -g -Wall -Wextra -pthread -c main.c -o main.o
	$(CC) -std=c11 -O0 -g -Wall -Wextra -pthread -c sysinfo.c -o sysinfo.o
	$(CC) -std=c11 -O0 -g -Wall -Wextra -pthread -c workloads.c -o workloads.o
	$(CC) -std=c11 -O0 -g -Wall -Wextra -pthread -c bench.c -o bench.o
	$(CC) -std=c11 -O0 -g -Wall -Wextra -pthread -c score.c -o score.o
	$(CC) main.o sysinfo.o workloads.o bench.o score.o -o $(TARGET)-debug -pthread -lm

.PHONY: all clean run run-single run-multi run-json rebuild debug
