#pragma once
#include "common.h"
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <chrono>

enum class LogCategory {
    GENERAL,
    INDICATOR,
    FILTER,
    SIGNAL,
    EXECUTION,
    RISK,
    TIME
};

class ILogger {
public:
    virtual ~ILogger() = default;
    
    // Logger configuration methods
    virtual void set_enabled(bool state) = 0;
    virtual void set_verbosity(int level) = 0;
    virtual LogLevel get_verbosity() const = 0;
    virtual void set_current_candle(const Candle& candle) = 0;
    virtual void clear() = 0;
    
    // Methods for timing
    virtual void start_chrono() = 0;
    virtual int64_t stop_chrono_and_log() = 0;
    
    // Finalisation and sending logs
    virtual void finalize_and_send_logs() = 0;
    
    // General logs
    virtual void log_general(const std::string& message, int level = LogLevel::INFO) = 0;
    virtual void log_general(std::string&& message, int level = LogLevel::INFO) = 0;
    virtual void log_general_BE_activated(double position_sign, double ref_price, double break_even_price, double threshold) = 0;
    
    // Indicator logs
    virtual void log_indicator_value(const std::string& name, double value, int level = LogLevel::DEBUG) = 0;
    virtual void log_indicator_value(const std::string& name, std::pair<double, double> values, int level = LogLevel::DEBUG) = 0;
    virtual void log_indicator_value(const std::string& name, std::pair<double, int> values, int level = LogLevel::DEBUG) = 0;
    virtual void log_indicator_value(const std::string& name, const filter::MACDResult& values, int level = LogLevel::DEBUG) = 0;
    // Bollinger Bands result (middle, upper, lower)
    virtual void log_indicator_value(const std::string& name, const filter::BBResult& values, int level = LogLevel::DEBUG) = 0;
    // Swing Structure result (swingHigh, swingLow, trend)
    virtual void log_indicator_value(const std::string& name, const filter::SwingStructureResult& values, int level = LogLevel::DEBUG) = 0;
    virtual void log_indicator_comparison(const std::string& name, double value, double threshold, const std::string& comparison_op, bool result, int level = LogLevel::DEBUG) = 0;
    
    // Filter logs
    virtual void log_filter_result(const std::string& name, bool passed, const std::string& detail = "", int level = LogLevel::DEBUG) = 0;

    virtual void log_filter_result(const filter::GenericFilter& filter, double leftValue, double rightValue, bool result, int offset, int level = LogLevel::INFO) = 0;
    
    // Signal logs
    virtual void log_signal(const Signal& signal, int level = LogLevel::INFO) = 0;

    // Execution logs
    virtual void log_execution_step(const std::string& step, bool success, int level = LogLevel::INFO) = 0;
    virtual void log_execution_time(int64_t duration_us, int level = LogLevel::DEBUG) = 0;
    
    // Risk logs
    virtual void log_risk_calculation(double risk_amount, double risk_percentage, int level = LogLevel::INFO) = 0;
    virtual void log_position_sizing(double raw_size, double adjusted_size, const std::string& reason, int level = LogLevel::INFO) = 0;
    
    // Time logs
    virtual void log_time_check(bool in_trading_days, int weekday, bool in_trading_hours, const Time& current_time, int level = LogLevel::INFO) = 0;
    
    // Obtention of every log for current candle
    virtual std::string get_all_logs() const = 0;

    virtual std::string fast_double_to_string(double value, int precision = 4) = 0;
    virtual std::string fast_int_to_string(int64_t value) = 0;
};

class LoggerManager : public ILogger {
private:
    bool enabled = true;
    LogLevel verbosity_level = LogLevel::DEBUG;
    std::function<void(const std::string&)> callback;
    DateTime current_candle_date;
    mutable char buffer[64];
    mutable std::string msgBuffer;
    // mutable char numBuffer[64];

    // Variables for chrono timing
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    bool chrono_running = false;
    
    // Buffer to store logs by category
    std::vector<std::string> general_logs;
    std::vector<std::string> indicator_logs;
    std::vector<std::string> filter_logs;
    std::vector<std::string> signal_logs;
    std::vector<std::string> execution_logs;
    std::vector<std::string> risk_logs;
    std::vector<std::string> time_logs;
    
    // Indentation for hierarchical logs
    std::string indent(int level) const {
        return std::string(level * 2, ' ');
    }
    
    // Add a log to the appropriate category
    void add_log(LogCategory category, std::string&& message, int level = LogLevel::INFO) {
        if (!enabled || level < verbosity_level)
            return;
        
        std::vector<std::string>* target_logs;
        
        // Use a pointer to the appropriate vector
        switch (category) {
            case LogCategory::GENERAL:   target_logs = &general_logs; break;
            case LogCategory::INDICATOR: target_logs = &indicator_logs; break;
            case LogCategory::FILTER:    target_logs = &filter_logs; break;
            case LogCategory::SIGNAL:    target_logs = &signal_logs; break;
            case LogCategory::EXECUTION: target_logs = &execution_logs; break;
            case LogCategory::RISK:      target_logs = &risk_logs; break;
            case LogCategory::TIME:      target_logs = &time_logs; break;
        }

        target_logs->push_back(std::move(message));  // Use move to avoid a copy
    }

public:
    LoggerManager(std::function<void(const std::string&)> callback) : callback(std::move(callback)) {
        // Pre-allocate memory for the message buffer
        msgBuffer.reserve(256);
        
        // Pre-allocate memory for log vectors (avoid reallocations)
        general_logs.reserve(50);
        indicator_logs.reserve(50);
        filter_logs.reserve(50);
        signal_logs.reserve(20);
        execution_logs.reserve(20);
        risk_logs.reserve(20);
        time_logs.reserve(10);
    }

    std::string fast_double_to_string(double value, int precision = 4) override {
        char buffer[64];
        auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, precision);
        return std::string(buffer, ptr - buffer);
    }

    std::string fast_int_to_string(int64_t value) override {
        char buffer[64];
        auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
        return std::string(buffer, ptr - buffer);
    }
    
    // Logger configuration
    void set_enabled(bool state) override { enabled = state; }
    void set_verbosity(int level) override { verbosity_level = static_cast<LogLevel>(level); }
    LogLevel get_verbosity() const override { return verbosity_level; }
    void set_current_candle(const Candle& candle) override { current_candle_date = candle.ohlc.date; }
    void clear() override {
        general_logs.clear();
        indicator_logs.clear();
        filter_logs.clear();
        signal_logs.clear();
        execution_logs.clear();
        risk_logs.clear();
        time_logs.clear();
    }
    
    // Chrono control methods
    void start_chrono() override {
        start_time = std::chrono::high_resolution_clock::now();
        chrono_running = true;
    }

    int64_t stop_chrono_and_log() override {
        if (!chrono_running) {
            log_general("Attempted to stop chrono while it is not running", LogLevel::WARNING);
            return 0;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        int64_t duration_us = duration.count();
        
        // Log execution time
        log_execution_time(duration_us);
        
        chrono_running = false;
        return duration_us;
    }
    
    // New method to finalize logs and send them
    void finalize_and_send_logs() override {
        // If chrono is still running, stop it and log the time
        if (chrono_running)
            stop_chrono_and_log();
        
        // Send all logs
        std::string message(get_all_logs());
        if (message.empty() || !callback) return;

        callback(message);
    }
    
    // General logs
    void log_general(std::string&& message, int level = LogLevel::INFO) override {
        add_log(LogCategory::GENERAL, std::move(message), level);
    }

    void log_general(const std::string& message, int level = LogLevel::INFO) override {
        // Make a copy and move it to avoid a double copy
        std::string msg_copy = message;
        add_log(LogCategory::GENERAL, std::move(msg_copy), level);
    }

    void log_general_BE_activated(double position_sign, double ref_price, double break_even_price, double threshold) override {
        std::string msg = "Break-even activated: " + 
                          std::string(position_sign > 0 ? "High" : "Low") + "=" + 
                          fast_double_to_string(ref_price) + 
                          " " + std::string(position_sign > 0 ? ">=" : "<=") + 
                          " threshold (" + fast_double_to_string(break_even_price) + 
                          "), " + fast_double_to_string(threshold * 100) + 
                          "% of the way to TP";
        add_log(LogCategory::GENERAL, std::move(msg), LogLevel::INFO);
    }
    
    // Indicator logs
    void log_indicator_value(const std::string& name, double value, int level = LogLevel::DEBUG) override {
        int precision = 8;
        std::string msg = "Indicator " + name + " = " + fast_double_to_string(value, precision);
        add_log(LogCategory::INDICATOR, std::move(msg), level);
    }

    void log_indicator_value(const std::string& name, std::pair<double, double> values, int level = LogLevel::DEBUG) override {
        std::string msg = "Indicator " + name + " = [" + fast_double_to_string(values.first) + ", " + fast_double_to_string(values.second) + "]";
        add_log(LogCategory::INDICATOR, std::move(msg), level);
    }

    void log_indicator_value(const std::string& name, std::pair<double, int> values, int level = LogLevel::DEBUG) override {
        std::string msg = "Indicator " + name + " = [" + fast_double_to_string(values.first) + ", " + fast_int_to_string(values.second) + "]";
        add_log(LogCategory::INDICATOR, std::move(msg), level);
    }

    void log_indicator_value(const std::string& name, const filter::MACDResult& values, int level = LogLevel::DEBUG) override {
        std::string msg = "Indicator " + name + " = [macd=" + fast_double_to_string(values.macdLine) + ", signal=" + fast_double_to_string(values.signalLine) + ", hist=" + fast_double_to_string(values.histogram) + "]";
        add_log(LogCategory::INDICATOR, std::move(msg), level);
    }

    void log_indicator_value(const std::string& name, const filter::BBResult& values, int level = LogLevel::DEBUG) override {
        std::string msg = "Indicator " + name + " = [middle=" + fast_double_to_string(values.middle) + ", upper=" + fast_double_to_string(values.upper) + ", lower=" + fast_double_to_string(values.lower) + "]";
        add_log(LogCategory::INDICATOR, std::move(msg), level);
    }

    void log_indicator_value(const std::string& name, const filter::SwingStructureResult& values, int level = LogLevel::DEBUG) override {
        std::string msg = "Indicator " + name + " = [swingHigh=" + fast_double_to_string(values.swingHigh) + ", swingLow=" + fast_double_to_string(values.swingLow) + ", trend=" + fast_int_to_string(values.trend) + "]";
        add_log(LogCategory::INDICATOR, std::move(msg), level);
    }
    
    void log_indicator_comparison(const std::string& name, double value, double threshold, const std::string& comparison_op, bool result, int level = LogLevel::DEBUG) override {
        std::string status = result ? "VALID" : "REJECTED";
        std::string msg = "Indicator " + name + " " + status + ": " + fast_double_to_string(value) + " " + comparison_op + " " + fast_double_to_string(threshold);
        add_log(LogCategory::INDICATOR, std::move(msg), level);
    }
    
    // Filter logs
    void log_filter_result(const std::string& name, bool passed, const std::string& detail = "", int level = LogLevel::INFO) override {
        std::string status = passed ? "PASSED" : "REJECTED";
        std::string msg = "Filter " + name + ": " + status;
        add_log(LogCategory::FILTER, std::move(msg), level);
        if (!detail.empty())
            add_log(LogCategory::FILTER, indent(1) + detail, level);
    }

    void log_filter_result(const filter::GenericFilter& filter, double leftValue, double rightValue, bool result, int offset, int level = LogLevel::INFO) override {
        std::string status = result ? "PASSED" : "FAILED";
        std::string msg = "Filter evaluation [T-" + fast_int_to_string(offset) + "]:" + filter.description + " : " + status;

        add_log(LogCategory::FILTER, std::move(msg), level);
    }
    
    // Signal logs
    void log_signal(const Signal& signal, int level = LogLevel::INFO) override {
        std::string action;
        switch (signal.type) {
            case SignalType::BUY: action = "BUY"; break;
            case SignalType::SELL: action = "SELL"; break;
            case SignalType::LIQUIDATE: action = "LIQUIDATE"; break;
            case SignalType::MOVE_SL: action = "MOVE SL"; break;
            default: action = "UNKNOWN"; break;
        }

        std::string msg = "Signal " + action + " generated: Price=" + fast_double_to_string(signal.price) + 
                          ", Quantity=" + fast_double_to_string(signal.quantity);
        
        if (signal.take_profit > 0.0)
            msg += ", TP=" + fast_double_to_string(signal.take_profit);
        if (signal.stop_loss > 0.0)
            msg += ", SL=" + fast_double_to_string(signal.stop_loss);
        if (signal.new_sl > 0.0 && signal.type == SignalType::MOVE_SL)
            msg += ", New SL=" + fast_double_to_string(signal.new_sl);

        add_log(LogCategory::SIGNAL, std::move(msg), level);
    }
    
    // Execution logs
    void log_execution_step(const std::string& step, bool success, int level = LogLevel::INFO) override {
        std::string status = success ? "success" : "failure";
        std::string msg = "Step '" + step + "': " + status;
        add_log(LogCategory::EXECUTION, std::move(msg), level);
    }
    
    // Risk logs
    void log_risk_calculation(double risk_amount, double risk_percentage, int level = LogLevel::INFO) override {
        std::string msg = "Calculated risk: " + fast_double_to_string(risk_amount) + " (" + fast_double_to_string(risk_percentage) + "% of capital)";
        add_log(LogCategory::RISK, std::move(msg), level);
    }

    void log_position_sizing(double raw_size, double adjusted_size, const std::string& reason, int level = LogLevel::INFO) override {
        std::string msg = "Position sizing: " + fast_double_to_string(raw_size) + " -> " + fast_double_to_string(adjusted_size) + " (" + reason + ")";
        add_log(LogCategory::RISK, std::move(msg), level);
    }
    
    // Time logs
    void log_time_check(bool in_trading_days, int weekday, bool in_trading_hours, const Time& current_time, int level = LogLevel::INFO) override {
        if (!in_trading_days) {
            std::string dayName = (weekday == 0 ? "Monday" : weekday == 1 ? "Tuesday" : weekday == 2 ? "Wednesday" : 
                               weekday == 3 ? "Thursday" : weekday == 4 ? "Friday" : 
                               weekday == 5 ? "Saturday" : "Sunday");
            std::string msg = "Trading not allowed on day: " + dayName;
            add_log(LogCategory::TIME, std::move(msg), level);
            return;
        }

        if (!in_trading_hours) {
            std::string msg = "Outside trading hours: " +
                              fast_int_to_string(current_time.hour) + ":" +
                              fast_int_to_string(current_time.minute);
            add_log(LogCategory::TIME, std::move(msg), level);
            return;
        }

        std::string msg = "Within trading hours: " +
                          fast_int_to_string(current_time.hour) + ":" +
                          fast_int_to_string(current_time.minute);
        add_log(LogCategory::TIME, std::move(msg), LogLevel::DEBUG);
    }

    // Performance logs
    void log_execution_time(int64_t duration_us, int level = LogLevel::DEBUG) override {
        std::string msg = "Execution time: " + fast_int_to_string(duration_us) + " us";

        // Change level if processing takes too long
        if (duration_us > 20) {  // More than 20 us
            level = LogLevel::WARNING;
            msg += " (SLOW)";
        } else if (duration_us > 10) {  // More than 10 us
            level = LogLevel::INFO;
            msg += " (Moderate)";
        }

        add_log(LogCategory::EXECUTION, std::move(msg), level);
    }
    
    // Get all logs for the current candle
    std::string get_all_logs() const override {
        if (general_logs.empty() && indicator_logs.empty() && filter_logs.empty() && 
            signal_logs.empty() && execution_logs.empty() && risk_logs.empty() && time_logs.empty()) {
            return "";
        }

        // Estimate total required size to avoid reallocations
        size_t total_size = 200; // Base header
        
        // Add estimated size for each section
        total_size += general_logs.size() * 50;    // Average estimated per log
        total_size += indicator_logs.size() * 50;
        total_size += filter_logs.size() * 50;
        total_size += signal_logs.size() * 50;
        total_size += execution_logs.size() * 50;
        total_size += risk_logs.size() * 50;
        total_size += time_logs.size() * 50;
        
        // Pre-allocate final string
        std::string result;
        result.reserve(total_size);
        
        // Build candle header directly into the string
        result += "\n";
        result += "╔══════════════════════════════════════════════════════╗\n";
        result += "║             CANDLE: ";
        result += current_candle_date.to_string();
        result += std::string(14, ' ');
        result += "║\n";
        result += "╚══════════════════════════════════════════════════════╝\n";

        // Build each section
        auto append_section = [&result, this](const std::vector<std::string>& logs, const char* title) {
            if (logs.empty()) return;
            
            result += title;
            result += "\n";
            
            for (const auto& log : logs) {
                result += indent(1);
                result += log;
                result += "\n";
            }
        };

        append_section(general_logs, "GENERAL");
        append_section(indicator_logs, "INDICATORS");
        append_section(filter_logs, "FILTERS");
        append_section(signal_logs, "SIGNALS");
        append_section(execution_logs, "EXECUTION");
        append_section(risk_logs, "RISK");
        append_section(time_logs, "TIME");

        return result;
    }
};

// Null logger that does nothing (for optimization in backtest mode)
class NullLogger : public ILogger {
public:
    void set_enabled(bool) override {}
    void set_verbosity(int) override {}
    LogLevel get_verbosity() const override { return LogLevel::INFO; }
    void set_current_candle(const Candle&) override {}
    void clear() override {}
    void start_chrono() override {}
    int64_t stop_chrono_and_log() override { return 0; }
    void finalize_and_send_logs() override {}
    void log_general(const std::string&, int) override {}
    void log_general(std::string&&, int) override {}
    void log_general_BE_activated(double position_sign, double ref_price, double break_even_price, double threshold) override {}
    void log_indicator_value(const std::string&, double, int) override {}
    void log_indicator_value(const std::string&, std::pair<double, double>, int) override {}
    void log_indicator_value(const std::string&, std::pair<double, int>, int) override {}
    void log_indicator_value(const std::string&, const filter::MACDResult&, int) override {}
    void log_indicator_value(const std::string&, const filter::BBResult&, int) override {}
    void log_indicator_value(const std::string&, const filter::SwingStructureResult&, int) override {}
    void log_indicator_comparison(const std::string&, double, double, const std::string&, bool, int) override {}
    void log_filter_result(const std::string&, bool, const std::string&, int) override {}
    void log_filter_result(const filter::GenericFilter& filter, double leftValue, double rightValue, bool result, int offset, int level = LogLevel::INFO) override {}
    void log_signal(const Signal& signal, int level = LogLevel::INFO) override {}
    void log_execution_step(const std::string&, bool, int) override {}
    void log_execution_time(int64_t, int) override {}
    void log_risk_calculation(double, double, int) override {}
    void log_position_sizing(double, double, const std::string&, int) override {}
    void log_time_check(bool in_trading_days, int weekday, bool in_trading_hours, const Time& current_time, int level = LogLevel::INFO) override {}
    std::string get_all_logs() const override { return ""; }

    std::string fast_double_to_string(double value, int precision = 4) override { return ""; }
    std::string fast_int_to_string(int64_t value) override { return ""; }
};
