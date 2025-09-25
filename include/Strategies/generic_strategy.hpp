#pragma once

#include "strategy.h"
#include "Filters.h"
#include "Managers/FilterEvaluator.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <functional>

struct GenericStrategyConfig {
    std::string name; // à mettre dans StrategyBaseConfig
    std::optional<bool> go_direction = std::nullopt;  // true <=> LONG, false <=> SHORT

    // List of filters to apply (in order)
    // c'est pas mal mais il faut encore travailler la structure de description des filtres
    std::vector<GenericFilter> filters;

    int ema_short_period;
    int ema_long_period;
    int stoch_fastk;
    int stoch_slowk;
    int stoch_slowd;
    int stoch_threshold;
    int rsi_period;
    int rsi_threshold;
    int supertrend_atr_period;
    double supertrend_multiplier;
    int previous_ha_candle_red_filter_n;

    int rsi_history_periods;
    int stoch_history_periods;
    
    // ATR filter parameters
    int atr_filter_period;
    double atr_threshold;
    int atr_history_periods;
    
    bool use_ema_short_filter = false;
    bool use_ema_long_filter = false;
    bool use_stoch_filter = false;
    bool use_rsi_filter = false;
    bool use_previous_ha_candle_red_filter = false;
    bool use_supertrend_filter = false;
    bool use_atr_filter = false;

    // Overload the << operator for easy printing
    friend std::ostream& operator<<(std::ostream& os, const GenericStrategyConfig& config) {
        os << "GenericStrategyConfig {\n"
           << "  Go Direction: " << (config.go_direction == std::nullopt ? "Not Set" : (config.go_direction.value() ? "LONG" : "SHORT")) << "\n"
           << "  EMA Short Period: " << config.ema_short_period << " (Used: " << (config.use_ema_short_filter ? "Yes" : "No") << ")\n"
           << "  EMA Long Period: " << config.ema_long_period << " (Used: " << (config.use_ema_long_filter ? "Yes" : "No") << ")\n"
           << "  Stochastic (Used: " << (config.use_stoch_filter ? "Yes" : "No") << "):\n"
           << "    Fast K: " << config.stoch_fastk << "\n"
           << "    Slow K: " << config.stoch_slowk << "\n"
           << "    Slow D: " << config.stoch_slowd << "\n"
           << "    Threshold: " << config.stoch_threshold << "\n"
           << "    History Periods: " << config.stoch_history_periods << "\n"
           << "  RSI (Used: " << (config.use_rsi_filter ? "Yes" : "No") << "):\n"
           << "    Period: " << config.rsi_period << "\n"
           << "    Threshold: " << config.rsi_threshold << "\n"
           << "    History Periods: " << config.rsi_history_periods << "\n"
           << "  Supertrend (Used: " << (config.use_supertrend_filter ? "Yes" : "No") << "):\n"
           << "    ATR Period: " << config.supertrend_atr_period << "\n"
           << "    Multiplier: " << config.supertrend_multiplier << "\n"
           << "  ATR Filter (Used: " << (config.use_atr_filter ? "Yes" : "No") << "):\n"
           << "    Period: " << config.atr_filter_period << "\n"
           << "    Threshold: " << config.atr_threshold << "\n"
           << "    History Periods: " << config.atr_history_periods << "\n"
           << "  Use Previous HA Candle Red Filter: " << (config.use_previous_ha_candle_red_filter ? "Yes" : "No") << "\n"
           << "  Previous HA Candle Red Filter N: " << config.previous_ha_candle_red_filter_n << "\n"     
           << "}";
        return os;
    }
};

class GenericStrategy : public Strategy {
private:
    GenericStrategyConfig config;

    EMAParams ema_short_params;
    EMAParams ema_long_params;
    StochasticParams stoch_params;
    RSIParams rsi_params;
    ATRParams atrlog_params;
    ATRParams atrlog_filter_params;
    SuperTrendParams supertrend_filter_params;
    SuperTrendParams supertrend_tp_params;

    // TODO a mettre dans la class mere
    std::unique_ptr<FilterEvaluator> filterEvaluator;
    std::vector<GenericFilter> generic_filters;
    
    // Indicator values
    std::vector<std::pair<double, double>> stoch_kd_values;
    std::vector<double> rsi_values;
    std::vector<double> atr_values;

    void before() override {        
        // Get latest candle for logging
        BasicCandle ha_current = candle_manager.get_latest_heikin_ashi();
        bool is_green = candle_manager.is_candle_green(ha_current);
        
        logger->log_general("Bougie HA courante calculée: Open=" + logger->fast_double_to_string(ha_current.open) + 
                        ", Close=" + logger->fast_double_to_string(ha_current.close) + 
                        ", Green=" + std::string(is_green ? "Oui" : "Non"));

        if (config.use_stoch_filter && !stoch_kd_values.empty()) {
            stoch_kd_values[0] = indicator_manager->getStochasticValue(stoch_params);
        }
        
        if (config.use_rsi_filter && !rsi_values.empty()) {
            rsi_values[0] = indicator_manager->getRSIValue(rsi_params);
        }
        
        if (config.use_atr_filter && !atr_values.empty()) {
            atr_values[0] = indicator_manager->getATRValue(atrlog_filter_params);
        }
    }
    
    bool should_long() override {
        return true;
    }

    void go() override {
        logger->log_general("Préparation d'un signal d'entrée", LogLevel::INFO);

        // Calculate Stop Loss
        stop_loss_distance = PositionManager::calculateStopLoss(
            base_config, 
            price(), 
            indicator_manager->getATRValue(atrlog_params), 
            config.go_direction.value(),  // is_long = true 
            candle_manager, 
            candle_manager.get_latest_candle(), 
            logger
        );

        // Calculate Take Profit
        take_profit_distance = PositionManager::calculateTakeProfit(
            base_config,
            price(),
            indicator_manager->getATRValue(atrlog_params),
            stop_loss_distance,
            candle_manager,
            logger
        );

        // Calculate position size
        double quantity = PositionManager::calculatePositionSize(
            base_config,
            price(),
            stop_loss_distance,
            logger
        );
        
        if (config.go_direction.value()) { // true => LONG
            buy_quantity = quantity;
            buy_price = price();
        }
        else {
            sell_quantity = quantity;
            sell_price = price();
        }
    }

    void after() override {
        // Perform historical value shifting after each update

        // Shift stochastic values
        if (config.use_stoch_filter && !stoch_kd_values.empty() && stoch_kd_values[0].first > 0) {
            // Shift all values by one position
            for (int i = stoch_kd_values.size() - 1; i > 0; i--) {
                stoch_kd_values[i] = stoch_kd_values[i-1];
            }
        }

        // Shift RSI values
        if (config.use_rsi_filter && !rsi_values.empty() && rsi_values[0] > 0) {
            // Shift all values by one position
            for (int i = rsi_values.size() - 1; i > 0; i--) {
                rsi_values[i] = rsi_values[i-1];
            }
        }

        // Shift ATR values
        if (config.use_atr_filter && !atr_values.empty() && atr_values[0] > 0) {
            // Shift all values by one position
            for (int i = atr_values.size() - 1; i > 0; i--) {
                atr_values[i] = atr_values[i-1];
            }
        }
    }

    void registerFilters() {
        active_filters.clear();
        
        // il faut ajouter un filtre sur le nombre minimum de bougies a avoir dans le candle manager avant de commencer a trader
        active_filters.push_back([this]() {
            if (candle_manager.size() < 3) {
                logger->log_general("Pas assez d'historique (min 3 bougies)", LogLevel::WARNING);
                return false;
            }

            if (base_config.sl_method == StopLossMethod::MinMax && candle_manager.size() < static_cast<size_t>(base_config.sl_minmax_periods)) {
                logger->log_general("Pas assez d'historique pour le calcul Min/Max SL", LogLevel::WARNING);
                return false;
            }

            return true;
        });


        // Nouveau filtre pour vérifier si la bougie HA précédente est rouge ou verte qui remplace la condition dans should_long et should_short
        if (config.go_direction.value()) // true => LONG
            active_filters.push_back([this]() {
                return Filters::previousHACandlesGreen(candle_manager, 1, 0, "Bougie HA précédente", logger.get());
            });
        else // false => SHORT
            active_filters.push_back([this]() {
                return Filters::previousHACandlesRed(candle_manager, 1, 0, "Bougie HA précédente", logger.get());
            });


        filterEvaluator->evaluateAll(generic_filters);

        // if (config.use_ema_short_filter)
        //     active_filters.push_back([this]() {
        //         return Filters::priceSupEMA(price(), indicator_manager->getEMAValue(ema_short_params), "EMA Short", logger.get());
        //     });
        if (config.use_ema_long_filter)
            active_filters.push_back([this]() {
                return Filters::priceSupEMA(price(), indicator_manager->getEMAValue(ema_long_params), "EMA Long", logger.get());
            });
        if (config.use_stoch_filter)
            active_filters.push_back([this]() {
                return Filters::stochInfThreshold(stoch_kd_values, config.stoch_threshold, "Stoch", logger.get());
            });
        if (config.use_rsi_filter)
            active_filters.push_back([this]() {
                return Filters::rsiInfThreshold(rsi_values, config.rsi_threshold, "RSI", logger.get());
            });
        if (config.use_atr_filter)
            active_filters.push_back([this]() {
                return Filters::atrAboveThreshold(atr_values, config.atr_threshold, "ATR", logger.get());
            });
        if (config.use_previous_ha_candle_red_filter)
            active_filters.push_back([this]() {
                return Filters::previousHACandlesRed(candle_manager, config.previous_ha_candle_red_filter_n, 1, "Bougie HA précédente", logger.get());
            });
        if (config.use_supertrend_filter)
            active_filters.push_back([this]() {
                return Filters::priceSupSupertrend(price(), indicator_manager->getSuperTrendValue(supertrend_filter_params), "Supertrend", logger.get());
            });
    }

    void registerIndicators() {
        // Register only active indicators
        if (config.use_ema_short_filter)
            indicator_manager->registerEMA(ema_short_params);
        
        if (config.use_ema_long_filter)
            indicator_manager->registerEMA(ema_long_params);

        if (config.use_stoch_filter)
            indicator_manager->registerStochastic(stoch_params);

        if (config.use_rsi_filter)
            indicator_manager->registerRSI(rsi_params);

        if (config.use_atr_filter)
            indicator_manager->registerATR(atrlog_filter_params);

        if (base_config.sl_method == StopLossMethod::ATR || base_config.tp_method == TakeProfitMethod::ATR)
            indicator_manager->registerATR(atrlog_params);

        if (config.use_supertrend_filter)
            indicator_manager->registerSuperTrend(supertrend_filter_params);

        if (base_config.tp_method == TakeProfitMethod::SuperTrend)
            indicator_manager->registerSuperTrend(supertrend_tp_params);
    }

public:
    GenericStrategy(const StrategyBaseConfig& base_cfg, const GenericStrategyConfig& generic_cfg) 
        : Strategy(base_cfg), config(generic_cfg),
          ema_short_params(config.ema_short_period),
          ema_long_params(config.ema_long_period),
          stoch_params(config.stoch_fastk, config.stoch_slowk, config.stoch_slowd),
          rsi_params(config.rsi_period),
          atrlog_params(base_cfg.atr_period, true),
          atrlog_filter_params(config.atr_filter_period, true),
          supertrend_filter_params(config.supertrend_atr_period, config.supertrend_multiplier),
          supertrend_tp_params(base_cfg.tp_supertrend_atr_period, base_cfg.tp_supertrend_multiplier),
          generic_filters(config.filters) {


        if (!config.go_direction.has_value()) {
            logger->log_general("La direction (go_direction) n'est pas définie dans la configuration.", LogLevel::ERROR);
            throw std::invalid_argument("Direction (go_direction) must be specified in GenericStrategyConfig");
        }

        // a tester
        GenericFilter genFilter(
            ValueSource::Price(PriceType::CLOSE), 
            ComparisonOperator::GREATER_THAN, 
            ValueSource::EMA(ema_short_params.period), 
            TemporalLogic::CURRENT, 
            1
        );

        filterEvaluator = std::make_unique<FilterEvaluator>(&candle_manager, indicator_manager.get(), logger.get());

        // Initialize vectors with appropriate sizes
        stoch_kd_values.resize(config.stoch_history_periods, {0.0, 0.0});
        rsi_values.resize(config.rsi_history_periods, 0.0);
        atr_values.resize(config.atr_history_periods, 0.0);

        registerFilters();
        registerIndicators();
    }
};