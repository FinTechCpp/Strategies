#pragma once

#include "common.h"
#include "Managers/CandleManager.hpp"
#include "Managers/PositionManager.hpp"
#include "Managers/IndicatorManager.hpp"
#include "Managers/FilterEvaluator.hpp"
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

// Fonction utilitaire pour parser une chaîne de date ISO
DateTime parse_iso_datetime(const std::string& iso_date);

// Fonction pour obtenir le jour de la semaine (0=lundi, 6=dimanche)
int get_day_of_week(const DateTime& date);

class Strategy {
public:
    Strategy(const StrategyConfig& config);
    virtual ~Strategy() = default;
    
    // Main update method
    Signal* update_candle(const Candle& candle);

    void set_log_callback(std::function<void(const std::string&)> callback) {
        logger->set_log_callback(callback);
    }

    // Log the strategy configuration (call after set_log_callback)
    void log_configuration();

protected:
    StrategyConfig base_config;
    PositionInfo position_info;
    std::unique_ptr<CandleManager> candle_manager;
    std::unique_ptr<IndicatorManager> indicator_manager;
    std::unique_ptr<ILogger> logger;
    std::vector<filter::GenericFilter> filters;
    // Filters that trigger liquidation when true
    std::vector<filter::GenericFilter> resale_filters;

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
    int weekday = -1;
    bool time_check = false;

    // Suivi des pertes journalières
    DateTime current_trading_day;
    double daily_pnl = 0.0;
    double daily_max_pnl = 0.0;  // Track the highest PnL reached during the day
    bool trading_suspended_for_day = false;
    
    // Cache pour le dernier trade
    double last_trade_pnl = 0.0;

    // Core strategy methods to implement in derived classes
    virtual void registerFiltersIndicators();
    virtual void before() {}
    virtual void after() {}
    virtual void go();
    
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
    std::unique_ptr<Signal> generate_buy_signal();
    std::unique_ptr<Signal> generate_sell_signal();
    std::unique_ptr<Signal> generate_liquidation_signal();
    
    void reset();
    void execute_long();
    void execute_short();
    bool execute_filters();
    bool execute_resale_filters();
    void execute();
};