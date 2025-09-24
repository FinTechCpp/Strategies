#pragma once

#include "common.h"
#include "Managers/CandleManager.hpp"
#include "Managers/PositionManager.hpp"
#include "Managers/IndicatorManager.hpp"
#include "LoggerFactory.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "Indicators/indicators.hpp"
#include "Indicators/supertrend.hpp"

// Fonction utilitaire pour parser une chaîne de date ISO
DateTime parse_iso_datetime(const std::string& iso_date);

// Fonction pour obtenir le jour de la semaine (0=lundi, 6=dimanche)
int get_day_of_week(const DateTime& date);

struct Signal {
    std::string action = ""; // "BUY", "SELL", "LIQUIDATE", "MOVE_SL"
    double quantity = 0.0;
    double price = 0.0;
    double take_profit = 0.0;
    double stop_loss = 0.0;
    double new_sl = 0.0;  // For MOVE_SL action
};

class Strategy {
public:
    Strategy(const StrategyBaseConfig& config);
    virtual ~Strategy() = default;
    
    // Main update method
    Signal* update_candle(const Candle& candle);

    void set_log_callback(std::function<void(const std::string&, int)> callback) {
        logger->set_log_callback(callback);
    }    

protected:
    StrategyBaseConfig base_config;
    CandleManager candle_manager;
    std::unique_ptr<ILogger> logger;
    PositionInfo position_info;
    std::vector<std::function<bool()>> active_filters;
    std::unique_ptr<IndicatorManager> indicator_manager;


    // Signal components
    double buy_quantity = 0.0;
    double buy_price = 0.0;
    double sell_quantity = 0.0;
    double sell_price = 0.0;
    double take_profit_distance = 0.0;
    double stop_loss_distance = 0.0;
    
    // Execution control
    std::unique_ptr<Signal> signal;
    
    // Cache for time checking
    DateTime last_check_date;
    bool weekday_check = false;
    bool time_check = false;

    // Suivi des pertes journalières
    DateTime current_trading_day;
    double daily_pnl = 0.0;
    double daily_max_pnl = 0.0;  // Track the highest PnL reached during the day
    bool trading_suspended_for_day = false;
    
    // Cache pour le dernier trade
    double last_trade_pnl = 0.0;
    
    // SuperTrend pour TP - values updated by derived classes
    int previous_supertrend_direction = 0;
    double current_supertrend = 0.0;
    int current_supertrend_direction = 0;

    // Nth Heikin-Ashi TP tracking
    int opposite_heikin_ashi_count = 0;  // Counter for opposite Heikin-Ashi candles
    bool is_position_long = false;       // Track whether current position is long or short
    // Core strategy methods to implement in derived classes
    virtual void before() {}
    virtual void after() {}
    virtual bool should_long() { 
        logger->log_general("should_long not implemented, defaulting to false", LogLevel::WARNING);
        return false; }
    virtual bool should_short() { 
        logger->log_general("should_short not implemented, defaulting to false", LogLevel::WARNING);
        return false; }
    virtual void go() { } // a mettre virtual pure
    // a supprimer 
    virtual void go_long() {
        logger->log_general("go_long deprecated, use go() instead", LogLevel::WARNING);
        go();
    }
    // a supprimer
    virtual void go_short() {
        logger->log_general("go_short deprecated, use go() instead", LogLevel::WARNING);
        go();
    }
    virtual void updateLocalValues() {}
    
    double price() const;

private:
    bool update_indicators();
    double calculate_trade_risk(bool is_long);
    bool is_trade_risk_acceptable(double risk);
    bool is_daily_max_profit_reached();
    bool is_daily_drawdown_reached();
    bool is_new_trading_day();
    void update_daily_pnl_tracking();

    // Méthode pour vérifier si on est dans les horaires de trading
    bool check_time();
    
    std::unique_ptr<Signal> check_break_even();
    std::unique_ptr<Signal> check_supertrend_exit();
    std::unique_ptr<Signal> check_nth_heikin_ashi_exit();
    std::unique_ptr<Signal> generate_buy_signal();
    std::unique_ptr<Signal> generate_sell_signal();
    std::unique_ptr<Signal> generate_liquidation_signal();
    
    void reset();
    void execute_long();
    void execute_short();
    bool execute_filters();
    void execute();

    void set_log_level(int level) {
        logger->set_verbosity(level);
    }

    void set_log_enabled(bool enabled) {
        // Remplacer le logger si nécessaire
        if (LoggerFactory::isLoggingEnabled() != enabled) {
            LoggerFactory::setLoggingEnabled(enabled);
            logger = LoggerFactory::createLogger();
        }
    }
};