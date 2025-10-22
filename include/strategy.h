#pragma once

#include "common.h"
#include "Managers/CandleManager.hpp"
#include "Managers/PositionManager.hpp"
#include "Managers/IndicatorManager.hpp"
#include "Managers/FilterEvaluator.hpp"
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

// Macro pour activer/désactiver les logs de la stratégie
// Décommenter la ligne suivante pour désactiver complètement les logs en production
//#define STRATEGY_DISABLE_LOGGING

#ifdef STRATEGY_DISABLE_LOGGING
    // En mode sans logging, toutes les appels sont remplacés par des no-ops
    #define STRATEGY_LOG(logger_ptr, method, ...) ((void)0)
    #define STRATEGY_LOG_VOID(logger_ptr, method) ((void)0)
#else
    // En mode avec logging, les appels sont normaux
    #define STRATEGY_LOG(logger_ptr, method, ...) (logger_ptr)->method(__VA_ARGS__)
    #define STRATEGY_LOG_VOID(logger_ptr, method) (logger_ptr)->method()
#endif

// Fonction utilitaire pour parser une chaîne de date ISO
DateTime parse_iso_datetime(const std::string& iso_date);

// Fonction pour obtenir le jour de la semaine (0=lundi, 6=dimanche)
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

    // Méthode pour vérifier si on est dans les horaires de trading
    bool check_time();
    
    std::optional<Signal> check_break_even();
    Signal generate_liquidation_signal();
    
    std::optional<Signal> execute_long();
    std::optional<Signal> execute_short();
    bool executeFilters(std::vector<filter::GenericFilter>& filters);
    Signal execute();
};