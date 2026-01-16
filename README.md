# zLogger

Flexible logging library for ESP32 with FreeRTOS support and multiple log sinks.

## Features

- **Multiple Log Levels**: Trace, Debug, Info, Warning, Error, Critical, Test
- **Color-Coded Output**: ANSI color codes for easy visual distinction of log levels
- **Thread-Safe**: Uses FreeRTOS semaphores for safe logging from multiple tasks
- **Buffered Logging**: Uses FreeRTOS stream buffers for non-blocking log operations
- **Multiple Sinks**: Support for different output destinations (Serial, and extensible for others)
- **Binary Buffer Dump**: Utility for dumping binary data in hex format
- **Compile-Time Configuration**: Customize default log level via defines

## Dependencies

- **zGlobals**: Status codes and utility macros - `https://github.com/zmeiresearch/zGlobals.git`
- **ESP32 Arduino Framework**: Arduino.h
- **FreeRTOS**: Included with ESP-IDF

## Installation

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps =
    https://github.com/zmeiresearch/zLogger.git
```

For a specific version or branch:

```ini
lib_deps =
    https://github.com/zmeiresearch/zLogger.git#v1.0.0
```

## Usage

### Basic Setup

```c
#include <logger.h>

#define CMP_NAME "MyComponent"

void setup() {
    // Initialize the logger
    LogInit(NULL);

    // Set log level (optional, defaults to eLogInfo)
    LogSetLevel(eLogDebug);
}

void loop() {
    // Log messages at different levels
    LOG(eLogInfo, "System initialized");
    LOG(eLogDebug, "Counter value: %d", counter);
    LOG(eLogWarn, "Temperature high: %d°C", temp);
    LOG(eLogError, "Sensor failure detected");
}
```

### FreeRTOS Task

The logger requires a FreeRTOS task to handle the actual writing to log sinks:

```c
void loggerTask(void *pvParameters) {
    while(1) {
        LogTask();  // Process log buffer and write to sinks
    }
}

void setup() {
    LogInit(NULL);

    xTaskCreate(
        loggerTask,
        "Logger",
        2048,
        NULL,
        1,
        NULL
    );
}
```

### Log Levels

The library supports the following log levels (in increasing severity):

```c
typedef enum _eLogLevel {
    eLogTrace,      // T - Detailed trace information
    eLogDebug,      // D - Debug information
    eLogInfo,       // I - Informational messages
    eLogWarn,       // W - Warning messages
    eLogError,      // E - Error messages
    eLogCrit,       // C - Critical errors
    eLogTest,       // S - Test messages
    eLogDisabled,   // Y - Disabled
} eLogLevel;
```

### Binary Buffer Dump

Dump binary data in hex format:

```c
uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
LOG_DUMP_BUFFER(eLogDebug, data, sizeof(data));
```

Output:
```
01 02 03 04 05
```

### Configuring Default Log Level

Set the default log level at compile time:

```c
// In your build flags or before including logger.h
#define LOG_LEVEL_DEFAULT eLogDebug
```

Or in `platformio.ini`:

```ini
build_flags =
    -DLOG_LEVEL_DEFAULT=eLogDebug
```

### Changing Log Level at Runtime

```c
LogSetLevel(eLogDebug);  // Show debug and higher
LogSetLevel(eLogError);  // Only show errors and critical
```

### Adding Human-Readable Time

By default, the logger only displays millisecond timestamps from FreeRTOS. You can add human-readable time (e.g., from an RTC or time library) by defining the `LOG_TIME_STRING` macro:

**Option 1: In your code before including logger**
```c
// If using a time library that returns a String
#define LOG_TIME_STRING() Time_GetTimeString().c_str()
// Or for a function returning const char*
#define LOG_TIME_STRING() MyGetTimeString()

#include <logger.h>
```

**Option 2: In platformio.ini**
```ini
build_flags =
    -DLOG_TIME_STRING=Time_GetTimeString().c_str()
```

**Example with zTime library:**
```c
#include <zTime.h>
#define LOG_TIME_STRING() Time_GetTimeString().c_str()
#include <logger.h>

void setup() {
    LogInit(NULL);
    LOG(eLogInfo, "System started");
    // Output: 2026-01-16T14:30:22|000012345|I|MyComponent|setup:System started
}
```

**Note:** The time string function should return a `const char*` or a type that can be formatted with `%s` in printf. If it returns a temporary object (like Arduino String), use `.c_str()` to convert it.

## Log Format

Log messages follow this format:

```
<color><optional_time_string>|<timestamp>|<level>|<component>|<function>:<message><reset>
```

Example (without custom time):
```
000123456|I|MyComponent|setup:System initialized
000123789|W|MyComponent|loop:Temperature high: 85°C
```

Example (with LOG_TIME_STRING defined):
```
2026-01-16T14:30:22|000123456|I|MyComponent|setup:System initialized
2026-01-16T14:30:25|000123789|W|MyComponent|loop:Temperature high: 85°C
```

With colors enabled (default), each log level has a distinct color:
- Trace: Blue
- Debug: White
- Info: Green
- Warning: Yellow
- Error: Red
- Critical: Bright Red
- Test: Cyan

## API Reference

### Initialization

```c
eStatus LogInit(void * params);
```

Initializes the logger subsystem. Call once during setup.

### Task Function

```c
eStatus LogTask(void);
```

Processes the log buffer and writes to sinks. Must be called regularly from a FreeRTOS task.

### Logging

```c
eStatus Log(const eLogLevel level, const char * component, const char * function, ...);
```

Direct logging function. Usually called via the `LOG()` macro.

**Macro:**
```c
LOG(level, format, ...);
```

### Set Log Level

```c
eStatus LogSetLevel(const eLogLevel level);
```

Changes the current log level filter.

### Buffer Dump

```c
eStatus LogDumpBuffer(const eLogLevel level, const char * component,
                      const char * function, const uint8_t * buffer, size_t buffer_size);
```

Dumps binary buffer in hex format. Usually called via the `LOG_DUMP_BUFFER()` macro.

**Macro:**
```c
LOG_DUMP_BUFFER(level, buffer, size);
```

## Advanced: Adding Custom Log Sinks

You can extend the logger to write to additional destinations (files, network, displays, etc.).

1. Implement the three required functions:
```c
eStatus MyCustomSinkInit(void);
size_t MyCustomSinkGetWriteSize(void);
size_t MyCustomSinkWrite(const uint8_t * buffer, size_t toSend);
```

2. Add your sink to the `sinks` array in `logger.cpp`:
```c
static const LogSink sinks[] = {
    { "Serial", LogSinkSerialInit, LogSinkSerialGetWriteSize, LogSinkSerialWrite },
    { "Custom", MyCustomSinkInit, MyCustomSinkGetWriteSize, MyCustomSinkWrite },
};
```

## Configuration Options

### Disable Colors

In `logger.cpp`, comment out or set to 0:
```c
#define LOG_USE_COLOR 0
```

### Adjust Buffer Sizes

In `logger.cpp`:
```c
#define LOG_BUFFER_SIZE     4096    // Total buffer for all log messages
#define LOG_MAX_LINE_SIZE   224     // Maximum single log line
```

### Serial Baud Rate

In `log_sink_serial.cpp`:
```c
Serial.begin(115200);  // Change to your preferred baud rate
```

### Custom Time String

Define at compile time to add human-readable timestamps:
```c
// In platformio.ini
build_flags =
    -DLOG_TIME_STRING=Time_GetTimeString().c_str()
```

Or define before including logger.h in your code. See "Adding Human-Readable Time" section for details.

## Notes

- The `CMP_NAME` macro should be defined in each source file to identify the component
- Logging from ISR context is not supported and will return `eUNSUPPORTED`
- If the logger is not initialized, log calls return `eNOTINITIALIZED`
- The logger uses static or dynamic allocation depending on FreeRTOS configuration

## License

MIT License - see LICENSE file for details

## Author

Ivan Vasilev, Zmei Research Ltd.
