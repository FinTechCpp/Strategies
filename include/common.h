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

// TODO : on pourrait mettre en commun avec la class Date du backtestEngine
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

// Niveaux de log
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

// Surcharge de l'opérateur de flux pour LogLevel
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

// Structure pour les données de position/trading
struct PositionInfo {
    // Pour le break-even, si la strategie n'utilise pas le break-even, pas necessaire
    double entry_price = 0.0;
    double take_profit_price = 0.0;

    // Pour la perte maximale journalière, si la stratégie n'utilise pas la perte maximale journalière, pas nécessaire
    double closed_trade_pnl = 0.0;

    PositionInfo() = default;
};

// Composition plutôt qu'héritage pour la structure utilisée dans les stratégies
struct Candle {
    BasicCandle ohlc;
    PositionInfo position;

    Candle() = default;

    // Constructeur pratique pour les données OHLC
    Candle(const DateTime& dt, double o, double h, double l, double c)
        : ohlc(dt, o, h, l, c) {}

    // Constructeur complet
    Candle(const BasicCandle& basic, const PositionInfo& pos)
        : ohlc(basic), position(pos) {}

    // Accesseurs pratiques pour éviter d'écrire candle.ohlc.xxx
    double open() const { return ohlc.open; }
    double high() const { return ohlc.high; }
    double low() const { return ohlc.low; }
    double close() const { return ohlc.close; }
    const DateTime& date() const { return ohlc.date; }
};

struct StrategyBaseConfig {
    LogLevel logLevel = LogLevel::DEBUG;
    bool enable_logging = true; // Enable or disable logging

    // Time settings
    Time trading_from = {7, 0, 0};   // 7:00 AM
    Time trading_to = {23, 0, 0};    // 11:00 PM
    std::vector<int> trading_days = {0, 1, 2, 3, 4};  // 0=Monday, 6=Sunday
    
    // Fixed SL/TP values
    double take_profit_distance = 30.0;
    double stop_loss_distance = 20.0;
    
    // Paramètres ATR pour SL et TP
    bool use_atr_for_sl = false;     // Important: valeur par défaut false
    bool use_atr_for_tp = false;     // Important: valeur par défaut false
    int atr_period = 14;
    double stop_loss_atr_multiplier = 2.0; // sl_atr_multiple
    double take_profit_atr_multiplier = 3.0; // tp_atr_multiple
    double min_stop_loss_distance = 5.0;
    double min_take_profit_distance = 5.0;
    
    // Nouveaux paramètres Min/Max pour SL
    bool use_minmax_for_sl = false;
    int sl_minmax_periods = 5;
    double sl_minmax_delta = 5.0;
    
    // Nouveau paramètre pour TP basé sur SL
    bool use_sl_ratio_for_tp = false;
    double tp_sl_ratio = 2.0;  // TP = SL * ratio
        
    // Risk management
    bool use_risk_based_sizing = false;
    double risk_percentage = 1.0; // risk_per_trade_pct
    double cash = 100000.0;
    double max_position_percentage = 100.0;
    double leverage_limit = 20.0;
    
    // Break-even parameters
    bool use_break_even = false;
    double break_even_threshold = 0.7;

    // Perte maximale journalière
    bool use_daily_max_loss = false;
    double daily_max_loss_percentage = 2.0;
    double daily_max_loss_amount = 0.0; // Calculé à partir de cash et daily_max_loss_percentage
    
    // Profit maximal journalier
    bool use_daily_max_profit = false;
    double daily_max_profit_percentage = 5.0;
    double daily_max_profit_amount = 0.0; // Calculé à partir de cash et daily_max_profit_percentage
};

// Surcharge de l'opérateur de flux pour StrategyBaseConfig
inline std::ostream& operator<<(std::ostream& os, const StrategyBaseConfig& config) {
    os << "StrategyBaseConfig {\n";

    // Log level
    os << "  Log level: " << config.logLevel << "\n";
    os << "  Enable logging: " << (config.enable_logging ? "Yes" : "No") << "\n";
    
    // Time settings
    os << "  Trading hours: " << config.trading_from.hour << ":" << config.trading_from.minute 
       << " - " << config.trading_to.hour << ":" << config.trading_to.minute << "\n";
    
    os << "  Trading days: ";
    for (size_t i = 0; i < config.trading_days.size(); ++i) {
        if (i > 0) os << ", ";
        switch(config.trading_days[i]) {
            case 0: os << "Monday"; break;
            case 1: os << "Tuesday"; break;
            case 2: os << "Wednesday"; break;
            case 3: os << "Thursday"; break;
            case 4: os << "Friday"; break;
            case 5: os << "Saturday"; break;
            case 6: os << "Sunday"; break;
            default: os << "Unknown"; break;
        }
    }
    os << "\n";
    
    // SL/TP values
    os << "  Take profit distance: " << config.take_profit_distance << "\n";
    os << "  Stop loss distance: " << config.stop_loss_distance << "\n";
    
    // ATR parameters
    os << "  Use ATR for SL: " << (config.use_atr_for_sl ? "Yes" : "No") << "\n";
    os << "  Use ATR for TP: " << (config.use_atr_for_tp ? "Yes" : "No") << "\n";
    os << "  ATR period: " << config.atr_period << "\n";
    os << "  SL ATR multiplier: " << config.stop_loss_atr_multiplier << "\n";
    os << "  TP ATR multiplier: " << config.take_profit_atr_multiplier << "\n";
    os << "  Min SL distance: " << config.min_stop_loss_distance << "\n";
    os << "  Min TP distance: " << config.min_take_profit_distance << "\n";
    
    // Min/Max parameters
    os << "  Use Min/Max for SL: " << (config.use_minmax_for_sl ? "Yes" : "No") << "\n";
    os << "  SL Min/Max periods: " << config.sl_minmax_periods << "\n";
    os << "  SL Min/Max delta: " << config.sl_minmax_delta << "\n";
    
    // SL ratio for TP
    os << "  Use SL ratio for TP: " << (config.use_sl_ratio_for_tp ? "Yes" : "No") << "\n";
    os << "  TP = SL * ratio: " << config.tp_sl_ratio << "\n";
    
    // Risk management
    os << "  Use risk-based sizing: " << (config.use_risk_based_sizing ? "Yes" : "No") << "\n";
    os << "  Risk percentage: " << config.risk_percentage << "%\n";
    os << "  Cash: " << config.cash << "\n";
    os << "  Max position %: " << config.max_position_percentage << "%\n";
    
    // Break-even parameters
    os << "  Use break-even: " << (config.use_break_even ? "Yes" : "No") << "\n";
    os << "  Break-even threshold: " << config.break_even_threshold << "\n";
    
    // Daily maximum loss
    os << "  Use daily max loss: " << (config.use_daily_max_loss ? "Yes" : "No") << "\n";
    os << "  Daily max loss %: " << config.daily_max_loss_percentage << "%\n";
    os << "  Daily max loss amount: " << config.daily_max_loss_amount << "\n";
    
    // Daily maximum profit
    os << "  Use daily max profit: " << (config.use_daily_max_profit ? "Yes" : "No") << "\n";
    os << "  Daily max profit %: " << config.daily_max_profit_percentage << "%\n";
    os << "  Daily max profit amount: " << config.daily_max_profit_amount << "\n";
    
    os << "}";
    return os;
}