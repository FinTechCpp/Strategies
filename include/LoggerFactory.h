#pragma once

#include "Managers/LoggerManager.hpp"

class LoggerFactory {
private:
    static bool logging_enabled;
    
public:
    static void setLoggingEnabled(bool enabled) {
        logging_enabled = enabled;
    }
    
    static bool isLoggingEnabled() {
        return logging_enabled;
    }
    
    static std::unique_ptr<ILogger> createLogger() {
        if (logging_enabled) {
            return std::make_unique<LoggerManager>();
        } else {
            return std::make_unique<NullLogger>();
        }
    }
};

