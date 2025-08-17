#pragma once
#include <string>
#include <sstream>
#include <charconv>
#include <array>
#include <functional>


struct Time {
    int hour = 0;
    int minute = 0;
    int second = 0;

    bool operator<(const Time& other) const;
    bool operator<=(const Time& other) const;
    bool operator==(const Time& other) const;
    bool operator!=(const Time& other) const;

    Time() = default;

    Time(int h, int m, int s = 0)
        : hour(h), minute(m), second(s) {}
};

// TODO: could be shared with the Date class from backtestEngine
struct DateTime {
    int year = 0;
    int month = 0;
    int day = 0;
    Time time;

    bool is_valid() const;
    bool operator==(const DateTime& other) const;
    bool operator!=(const DateTime& other) const;
    bool operator<(const DateTime& other) const;
    bool operator<=(const DateTime& other) const;
    bool operator>(const DateTime& other) const;
    bool operator>=(const DateTime& other) const;
    std::string to_string() const;
};

// Logging levels for the strategy
enum LogLevel {
    CRITICAL = 50,
    FATAL = CRITICAL,
    ERROR = 40,
    WARNING = 30,
    WARN = WARNING,
    INFO = 20,
    DEBUG = 10,
    NOTSET = 0
};

// Overload the << operator for LogLevel to print it as a string
inline std::ostream& operator<<(std::ostream& os, const LogLevel& level) {
    switch (level) {
        case LogLevel::CRITICAL:
            return os << "CRITICAL";
        case LogLevel::ERROR:
            return os << "ERROR";
        case LogLevel::WARNING:
            return os << "WARNING";
        case LogLevel::INFO:
            return os << "INFO";
        case LogLevel::DEBUG:
            return os << "DEBUG";
        case LogLevel::NOTSET:
            return os << "NOTSET";
        default:
            return os << "UNKNOWN(" << static_cast<int>(level) << ")";
    }
}

struct BasicCandle {
    DateTime date;
    double open;
    double high;
    double low;
    double close;

    BasicCandle() = default;

    BasicCandle(const DateTime& dt, double o, double h, double l, double c)
        : date(dt), open(o), high(h), low(l), close(c) {}
};

// Structure for position/trading data
struct PositionInfo {
    // For break-even, if the strategy does not use break-even, not necessary
    double entry_price = 0.0;
    double take_profit_price = 0.0;

    // For daily maximum loss, if the strategy does not use daily maximum loss, not necessary
    double closed_trade_pnl = 0.0;

    PositionInfo() = default;
};

// Composition rather than inheritance for the structure used in strategies
struct Candle {
    BasicCandle ohlc;
    PositionInfo position;

    Candle() = default;

    // Convenient constructor for OHLC data
    Candle(const DateTime& dt, double o, double h, double l, double c)
        : ohlc(dt, o, h, l, c) {}

    // Complete constructor
    Candle(const BasicCandle& basic, const PositionInfo& pos)
        : ohlc(basic), position(pos) {}

    // Convenient accessors to avoid writing candle.ohlc.xxx
    double open() const { return ohlc.open; }
    double high() const { return ohlc.high; }
    double low() const { return ohlc.low; }
    double close() const { return ohlc.close; }
    const DateTime& date() const { return ohlc.date; }
};

enum class StopLossMethod {
    Unset = -1,
    Fixed = 0,
    ATR = 1,
    MinMax = 2
};

enum class TakeProfitMethod {
    Unset = -1,
    Fixed = 0,
    ATR = 1,
    SLRatio = 2,
    SuperTrend = 3,
    RL = 4,
    NthHeikinAshi = 5
};

struct StrategyBaseConfig {
    LogLevel logLevel = LogLevel::DEBUG;
    bool enable_logging = true; // Enable or disable logging

    // Time settings
    Time trading_from;
    Time trading_to;
    bool trading_days_array[7]; // (0 = Monday, 6 = Sunday)

    // Fixed SL/TP values
    double take_profit_distance;
    double stop_loss_distance;

    // SL/TP Methods
    StopLossMethod sl_method = StopLossMethod::Unset;
    TakeProfitMethod tp_method = TakeProfitMethod::Unset;

    // ATR parameters for SL and TP
    int atr_period;
    double stop_loss_atr_multiplier;
    double take_profit_atr_multiplier;
    double min_stop_loss_distance;
    double min_take_profit_distance;

    // New Min/Max parameters for SL
    int sl_minmax_periods;
    double sl_minmax_delta;

    // New parameter for TP based on SL
    double tp_sl_ratio;

    // New parameter for TP based on SuperTrend
    int tp_supertrend_atr_period;
    double tp_supertrend_multiplier;

    // New parameters for TP based on ML/RL
    std::string rl_model_path = "./models/general_tp_model_lookback_150.onnx"; // Path to the ML model
    int rl_lookback_periods; // Number of historical candles to include in features
    double rl_tp_max_multiplier; // Maximum TP distance as multiple of SL distance
    double rl_tp_min_multiplier; // Minimum TP distance as multiple of SL distance

    // New parameter for nth Heikin-Ashi take profit
    int nth_heikin_ashi_count; // Number of opposite Heikin-Ashi candles to wait for

    // Risk management
    bool use_risk_based_sizing;
    double risk_percentage;
    double cash;
    double leverage_limit;

    // Break-even parameters
    bool use_break_even;
    double break_even_threshold;
    double break_even_offset_per_mille; // Per mille of entry price to move BE relative to entry price

    // Daily maximum loss
    bool use_daily_max_loss;
    double daily_max_loss_percentage;
    // DEPRECATED ! a supprimer c'est redondant
    // double daily_max_loss_amount; // Calculated from cash and daily_max_loss_percentage

    // Daily maximum profit
    bool use_daily_max_profit;
    double daily_max_profit_percentage;
    // DEPRECATED ! a supprimer c'est redondant
    // double daily_max_profit_amount; // Calculated from cash and daily_max_profit_percentage

    // Daily maximum drawdown
    bool use_daily_max_drawdown;
    double daily_max_drawdown_percentage;
    // DEPRECATED ! a supprimer c'est redondant
    // double daily_max_drawdown_amount; // Calculated from cash and daily_max_drawdown_percentage
};


// Overload the << operator for StopLossMethod
inline std::ostream& operator<<(std::ostream& os, const StopLossMethod& method) {
    switch (method) {
        case StopLossMethod::Unset:
            return os << "Unset";
        case StopLossMethod::Fixed:
            return os << "Fixed";
        case StopLossMethod::ATR:
            return os << "ATR";
        case StopLossMethod::MinMax:
            return os << "MinMax";
        default:
            return os << "Unknown(" << static_cast<int>(method) << ")";
    }
}

// Overload the << operator for StopLossMethod
inline std::ostream& operator<<(std::ostream& os, const TakeProfitMethod& method) {
    switch (method) {
        case TakeProfitMethod::Unset:
            return os << "Unset";
        case TakeProfitMethod::Fixed:
            return os << "Fixed";
        case TakeProfitMethod::ATR:
            return os << "ATR";
        case TakeProfitMethod::SLRatio:
            return os << "SLRatio";
        case TakeProfitMethod::SuperTrend:
            return os << "SuperTrend";
        case TakeProfitMethod::RL:
            return os << "RL";
        case TakeProfitMethod::NthHeikinAshi:
            return os << "NthHeikinAshi";
        default:
            return os << "Unknown(" << static_cast<int>(method) << ")";
    }
}

// Overload of the stream operator for StrategyBaseConfig
inline std::ostream& operator<<(std::ostream& os, const StrategyBaseConfig& config) {
    os << "StrategyBaseConfig {\n";

    // Log level
    os << "  Log level: " << config.logLevel << "\n";
    os << "  Enable logging: " << (config.enable_logging ? "Yes" : "No") << "\n";
    
    // Time settings
    os << "  Trading hours: " << config.trading_from.hour << ":" << config.trading_from.minute 
       << " - " << config.trading_to.hour << ":" << config.trading_to.minute << "\n";
    
    static const char* day_names[7] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
    os << "  Trading days: ";
    bool first = true;
    for (size_t i = 0; i < 7; ++i) {
        if (config.trading_days_array[i]) {
            if (!first) os << ", ";
            os << day_names[i];
            first = false;
        }
    }
    if (first) os << "None";
    os << "\n";

    os << "  Stop loss method: " << config.sl_method << "\n";
    os << "  Take profit method: " << config.tp_method << "\n";
    
    // SL/TP values
    os << "  Take profit distance: " << config.take_profit_distance << "\n";
    os << "  Stop loss distance: " << config.stop_loss_distance << "\n";
    
    // ATR parameters
    os << "  ATR period: " << config.atr_period << "\n";
    os << "  SL ATR multiplier: " << config.stop_loss_atr_multiplier << "\n";
    os << "  TP ATR multiplier: " << config.take_profit_atr_multiplier << "\n";
    os << "  Min SL distance: " << config.min_stop_loss_distance << "\n";
    os << "  Min TP distance: " << config.min_take_profit_distance << "\n";
    
    // Min/Max parameters
    os << "  SL Min/Max periods: " << config.sl_minmax_periods << "\n";
    os << "  SL Min/Max delta: " << config.sl_minmax_delta << "\n";
    
    // SL ratio for TP
    os << "  TP = SL * ratio: " << config.tp_sl_ratio << "\n";

    // SuperTrend parameters for TP
    os << "  TP SuperTrend ATR period: " << config.tp_supertrend_atr_period << "\n";
    os << "  TP SuperTrend multiplier: " << config.tp_supertrend_multiplier << "\n";

    // Nth Heikin-Ashi parameters for TP
    os << "  Nth Heikin-Ashi count: " << config.nth_heikin_ashi_count << "\n";
    
    // Risk management
    os << "  Use risk-based sizing: " << (config.use_risk_based_sizing ? "Yes" : "No") << "\n";
    os << "  Risk percentage: " << config.risk_percentage << "%\n";
    os << "  Leverage limit: " << config.leverage_limit << "\n";
    os << "  Cash: " << config.cash << "\n";
    
    // Break-even parameters
    os << "  Use break-even: " << (config.use_break_even ? "Yes" : "No") << "\n";
    os << "  Break-even threshold: " << config.break_even_threshold << "\n";
    os << "  Break-even offset percentage: " << config.break_even_offset_per_mille << "%\n";
    
    // Daily maximum loss
    os << "  Use daily max loss: " << (config.use_daily_max_loss ? "Yes" : "No") << "\n";
    os << "  Daily max loss %: " << config.daily_max_loss_percentage << "%\n";
    // os << "  Daily max loss amount: " << config.daily_max_loss_amount << "\n";
    
    // Daily maximum profit
    os << "  Use daily max profit: " << (config.use_daily_max_profit ? "Yes" : "No") << "\n";
    os << "  Daily max profit %: " << config.daily_max_profit_percentage << "%\n";
    // os << "  Daily max profit amount: " << config.daily_max_profit_amount << "\n";
    
    // Daily maximum drawdown
    os << "  Use daily max drawdown: " << (config.use_daily_max_drawdown ? "Yes" : "No") << "\n";
    os << "  Daily max drawdown %: " << config.daily_max_drawdown_percentage << "%\n";
    // os << "  Daily max drawdown amount: " << config.daily_max_drawdown_amount << "\n";
    
    os << "}";
    return os;
}