# Example of Configuration for Production

## For an Optimized Version Without Logs

If you want to compile a production version **without any logging overhead**, follow these steps:

### 1. Modify the `strategy.h` file

In `ThirdParty/Strategies/include/strategy.h`, uncomment the following line:

```cpp
// Before (development mode - with logs)
// #define STRATEGY_DISABLE_LOGGING

// After (production mode - without logs)
#define STRATEGY_DISABLE_LOGGING
```

### 2. Recompile in release mode

```bash
./build_and_run.sh --clean --no-run
```

### 3. Expected Result

With `STRATEGY_DISABLE_LOGGING` enabled, the compiled binary will have:
- **None** of the logging method calls compiled
- **None** of the log string constructions
- **None** overhead of performance
- The generated code is optimal

### 4. Verification

You can verify the impact by compiling with and without the define, and then comparing:

```bash
# Binary size
ls -lh build-release/backtestApp/backtestapp

# Performance with a profiler
perf stat ./build-release/backtestApp/backtestapp
```

### Expected Benchmark

| Configuration | Overhead | Notes |
|--------------|----------|-------|
| Logs activated (user) | ~100% | Construction complete of logs |
| Logs deactivated (user) | ~5-10% | calls function empty |
| `STRATEGY_DISABLE_LOGGING` | **0%** | no code logging compiled |

## Recommended Configuration

### Development
```cpp
// #define STRATEGY_DISABLE_LOGGING
```
- keep all logs for debugging
- Allows profiling and analysis

### Production / Release
```cpp
#define STRATEGY_DISABLE_LOGGING
```
- Performance maximized
- Binary optimized
- No risk of information leakage via logs

### Benchmark / Profiling
Switch between the two modes to measure the exact impact of logs on your specific use cases.
