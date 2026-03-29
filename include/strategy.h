#pragma once

#include "common.h"
#include "Managers/CandleManager.hpp"
#include "Managers/PositionManager.hpp"
#include "Managers/IndicatorManager.hpp"
#include "Managers/FilterEvaluator.hpp"
#include "Managers/LuaScriptEngine.hpp"

#include "MachineLearning/InferenceModel.hpp"
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
#include <cmath>
#include "Indicators/indicators.hpp"

// Utility function to parse an ISO date string
DateTime parse_iso_datetime(const std::string& iso_date);

// Function to get the day of the week (0=Monday, 6=Sunday)
int get_day_of_week(const DateTime& date);

class Strategy {
public:
    Strategy(const StrategyConfig& config, std::function<void(const std::string&)> callback = nullptr);
    virtual ~Strategy() = default;
    
    // Main update method - returns a Signal object
    Signal update_candle(const Candle& candle);
    
    // Getter for strategy name
    std::string getName() const { return base_config.name; }


protected:
    StrategyConfig base_config;
    PositionInfo position_info;
    std::unique_ptr<CandleManager> candle_manager;
    std::unique_ptr<IndicatorManager> indicator_manager;
    std::unique_ptr<ILogger> logger;

    // Filters that trigger a buy/sell signal (opening a position)
    std::vector<filter::GenericFilter> buyFilters;
    std::vector<filter::GenericFilter> sellFilters;
    // Filters that trigger a resale/rebuy signal (closing a position)
    std::vector<filter::GenericFilter> resaleFilters;
    std::vector<filter::GenericFilter> rebuyFilters;
    
    // Cache for time checking
    DateTime last_check_date;
    DateTime last_indicator_reset_day; // Track last day indicators were reset
    bool weekday_check = false;
    int weekday = -1;
    bool time_check = false;

    // Daily PnL tracking
    DateTime current_trading_day;
    double daily_pnl = 0.0;
    double daily_max_pnl = 0.0;  // Track the highest PnL reached during the day
    bool trading_suspended_for_day = false;
    
    // Cache for the last trade
    double last_trade_pnl = 0.0;

    // Machine Learning for entry signals
    std::unique_ptr<InferenceModel> ml_entry_model;
    bool ml_model_loaded = false;

    // Optional Lua scripting engine
    std::unique_ptr<LuaScriptEngine> lua_script_engine;

    // Core strategy methods to implement in derived classes
    virtual void registerFiltersIndicators();
    virtual void before() {}
    virtual void after() {}
    virtual Signal go(TradeDirection direction);
    
    double price() const;

private:
    bool update_indicators();
    double calculate_trade_risk(const Signal& signal);
    bool is_trade_risk_acceptable(double risk);
    bool is_daily_max_profit_reached();
    bool is_daily_drawdown_reached();
    bool is_new_trading_day();
    void update_daily_pnl_tracking();
    void check_and_reset_indicators_for_new_day();

    // Method to check if we are within trading hours
    bool check_time();
    
    std::optional<Signal> check_break_even();
    Signal generate_liquidation_signal();
    
    std::optional<Signal> execute_long();
    std::optional<Signal> execute_short();
    std::optional<Signal> execute_lua_script();
    bool executeFilters(std::vector<filter::GenericFilter>& filters);
    Signal execute();
    
    // ML-specific methods
    bool load_ml_model();
    std::vector<float> prepare_ml_features();
    float normalize_price(double price);
    int interpret_ml_prediction(float prediction);
    // Executes ML entry model and returns: 0 = no signal, 1 = BUY, 2 = SELL
    int execute_ml_filters();
};
