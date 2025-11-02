# Optimization of strategy logs

## Overview

This system allows for the complete disabling of logging method calls at compile time, thereby eliminating the overhead of function calls even when logging is disabled. This is particularly useful in production environments where performance is critical, and logging is not required.

## Configuration

### Enable/Disable Logging

In the file `include/strategy.h`, you will find the following definition:

```cpp
// Macro for enabling/disabling strategy logging
// Uncomment the following line to completely disable logging in production
// #define STRATEGY_DISABLE_LOGGING
```

**To disable logging in production:**
```cpp
#define STRATEGY_DISABLE_LOGGING
```

**To enable logging (default mode):**
```cpp
// #define STRATEGY_DISABLE_LOGGING
```

## Available Macros

### STRATEGY_LOG
Used for method calls with parameters:
```cpp
STRATEGY_LOG(logger, log_general, "Log message", LogLevel::INFO);
STRATEGY_LOG(logger, log_signal, signal);
```

### STRATEGY_LOG_VOID
Used for method calls without parameters:
```cpp
STRATEGY_LOG_VOID(logger, start_chrono);
STRATEGY_LOG_VOID(logger, clear);
```

## Behavior

### Mode with logging (default)
  - Every call to logging methods is executed normally
- Logs are generated as expected
  - Slight overhead due to function calls

### Mode without logging (STRATEGY_DISABLE_LOGGING defined)
- **All calls are replaced with `((void)0)` at compile time**
- No logging code is generated in the binary
- **Zero overhead**: no function calls, no string construction
- Maximum optimization for production

## Advantages

1. **Performance optimal in production** : Zero overhead when logs are disabled
2. **Code clean** : No need for `#ifdef` everywhere in the code
3. **Flexibility** : a simple `#define` to switch between modes
4. **Safety** : Type errors are caught at compile time even in no-logging mode

## Typical Usage

### Development and Debug
Leave logging enabled (default):
```cpp
// #define STRATEGY_DISABLE_LOGGING
```

### Production and Release
Activate full disabling:
```cpp
#define STRATEGY_DISABLE_LOGGING
```

### Benchmark and Profiling
Compare performance with and without logging to measure the precise impact.

## Migration of Existing Code

Replace :
```cpp
logger->log_general("Message");
logger->start_chrono();
```

With :
```cpp
STRATEGY_LOG(logger, log_general, "Message");
STRATEGY_LOG_VOID(logger, start_chrono);
```

## Technical Notes

- Macros use `((void)0)` which is completely eliminated by the compiler
- Arguments of macros are not even evaluated in deactivated mode
- Cela enables to avoid the construction of costly strings (like `fast_double_to_string`)
- Code remains type-safe thanks to the variadic macro mechanism
