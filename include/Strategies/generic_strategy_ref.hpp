#pragma once

#include "strategy.h"
#include "Filters.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <functional>
#include <map>

// Include the actual structures from serializerAdapters.h
// We need to include this to access GenericStrategyConfig and related structures
// Forward declarations would be insufficient since we need the full definitions
#include "../../../../backtestApp/include/components/serializerAdapters.h"

/**
 * @brief Generic strategy that can handle any configuration through JSON
 * 
 * This strategy replaces the need for specific hardcoded strategies like
 * BuyHeikinGreen and SellHeikinRed by providing a flexible framework
 * that can be configured entirely through the GenericStrategyConfig.
 */
class GenericStrategy : public Strategy {
private:
    GenericStrategyConfig config;
    
    // Dynamic indicator calculators - only instantiated based on config
    std::shared_ptr<EMA> ema_short_calculator;
    std::shared_ptr<EMA> ema_long_calculator;
    std::shared_ptr<STOCH> stochastic_calculator;
    std::shared_ptr<RSI> rsi_calculator;
    std::shared_ptr<ATRLOG> atrlog_calculator;
    std::shared_ptr<ATRLOG> atrlog_filter_calculator;
    std::shared_ptr<SUPERTREND> supertrend_filter_calculator;
    std::shared_ptr<SUPERTREND> supertrend_tp_calculator;
    
    // Indicator values storage
    std::map<std::string, std::vector<double>> indicator_values;
    std::map<std::string, std::vector<std::pair<double, double>>> indicator_pair_values;

    void before() override {
        // Update indicator values before filter evaluation
        updateIndicatorValues();
    }
    
    bool should_long() override {
        if (!config.direction) return false;
        
        // Check minimum history requirements
        if (candle_manager.size() < 3) {
            logger->log_general("Pas assez d'historique pour évaluer les conditions", LogLevel::DEBUG);
            return false;
        }

        // Check if we need Min/Max but don't have enough history
        if (base_config.sl_method == StopLossMethod::MinMax && 
            candle_manager.size() < static_cast<size_t>(base_config.sl_minmax_periods)) {
            logger->log_general("Historique insuffisant pour MinMax SL", LogLevel::DEBUG);
            return false;
        }
    
        // Basic Heikin-Ashi condition for LONG
        return candle_manager.is_latest_heikin_ashi_green();
    }

    bool should_short() override {
        if (config.direction) return false;
        
        // Check minimum history requirements
        if (candle_manager.size() < 3) {
            logger->log_general("Pas assez d'historique pour évaluer les conditions", LogLevel::DEBUG);
            return false;
        }

        // Check if we need Min/Max but don't have enough history
        if (base_config.sl_method == StopLossMethod::MinMax && 
            candle_manager.size() < static_cast<size_t>(base_config.sl_minmax_periods)) {
            logger->log_general("Historique insuffisant pour MinMax SL", LogLevel::DEBUG);
            return false;
        }
    
        // Basic Heikin-Ashi condition for SHORT
        return candle_manager.is_latest_heikin_ashi_red();
    }

    void go_long() override {
        go("LONG");
    }
    
    void go_short() override {
        go("SHORT");
    }
    
    /**
     * @brief Unified position entry method that handles both LONG and SHORT
     */
    // TODO : remplacer le return void par une structure avec dedans le sl distance, tp distance, quantity, price
    void go(const std::string& direction) {
        bool is_long = (direction == "LONG");
        logger->log_general("Préparation d'un signal " + direction, LogLevel::INFO);

        // Calculate Stop Loss
        stop_loss_distance = PositionManager::calculateStopLoss(
            base_config, 
            price(), 
            atrlog_calculator ? atrlog_calculator->get_value() : 0.0, 
            is_long,
            candle_manager, 
            candle_manager.get_latest_candle(), 
            logger
        );
        
        // Calculate Take Profit
        take_profit_distance = PositionManager::calculateTakeProfit(
            base_config,
            price(),
            atrlog_calculator ? atrlog_calculator->get_value() : 0.0,
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
        
        // Set appropriate values based on direction
        if (is_long) {
            buy_quantity = quantity;
            buy_price = price();
        } else {
            sell_quantity = quantity;
            sell_price = price();
        }
    }

    void after() override {
        // Update historical values after each candle
        updateHistoricalValues();
    }

    /**
     * @brief Register dynamic filters based on configuration
     */
    void registerFilters() {
        logger->log_general("Enregistrement des filtres génériques", LogLevel::INFO);
        
        for (const auto& filter_config : config.filters) {
            if (!filter_config.enabled) continue;
            
            // Create lambda for this filter
            active_filters.push_back([this, filter_config]() {
                return evaluateGenericFilter(filter_config);
            });
            
            logger->log_general("Filtre enregistré: " + filter_config.name, LogLevel::DEBUG);
        }
        
        logger->log_general("Total filtres enregistrés: " + std::to_string(active_filters.size()), LogLevel::INFO);
    }

    /**
     * @brief Register dynamic indicators based on configuration
     */
    void registerFiltersIndicators() {
        logger->log_general("Enregistrement des indicateurs génériques", LogLevel::INFO);
        
        for (const auto& indicator_config : config.indicators) {
            if (!indicator_config.enabled) continue;
            
            registerSingleIndicator(indicator_config);
        }
        
        // Always register ATR for SL/TP calculations
        if (!atrlog_calculator) {
            int atr_period = base_config.atr_period > 0 ? base_config.atr_period : 14;
            atrlog_calculator = std::make_shared<ATRLOG>(atr_period);
            indicator_manager->registerIndicator<ATRLOG, double>(atrlog_calculator);
            logger->log_general("ATR par défaut enregistré (période: " + std::to_string(atr_period) + ")", LogLevel::DEBUG);
        }
        
        logger->log_general("Enregistrement des indicateurs terminé", LogLevel::INFO);
    }

private:
    /**
     * @brief Register a single indicator based on its configuration
     */
    void registerSingleIndicator(const IndicatorConfig& indicator_config) {
        try {
            if (indicator_config.type == "EMA") {
                int period = static_cast<int>(indicator_config.parameters.at("period"));
                auto ema = std::make_shared<EMA>(period);
                
                if (indicator_config.name.find("short") != std::string::npos) {
                    ema_short_calculator = ema;
                } else if (indicator_config.name.find("long") != std::string::npos) {
                    ema_long_calculator = ema;
                }
                
                indicator_manager->registerIndicator<EMA, double>(ema);
                
            } else if (indicator_config.type == "RSI") {
                int period = static_cast<int>(indicator_config.parameters.at("period"));
                rsi_calculator = std::make_shared<RSI>(period);
                indicator_manager->registerIndicator<RSI, double>(rsi_calculator);
                
            } else if (indicator_config.type == "STOCH") {
                int fastk = static_cast<int>(indicator_config.parameters.at("fastk"));
                int slowk = static_cast<int>(indicator_config.parameters.at("slowk"));
                int slowd = static_cast<int>(indicator_config.parameters.at("slowd"));
                stochastic_calculator = std::make_shared<STOCH>(fastk, slowk, slowd);
                indicator_manager->registerIndicator<STOCH, std::pair<double, double>>(stochastic_calculator);
                
            } else if (indicator_config.type == "ATR" || indicator_config.type == "ATRLOG") {
                int period = static_cast<int>(indicator_config.parameters.at("period"));
                auto atr = std::make_shared<ATRLOG>(period);
                
                if (indicator_config.name.find("filter") != std::string::npos) {
                    atrlog_filter_calculator = atr;
                } else {
                    atrlog_calculator = atr;
                }
                
                indicator_manager->registerIndicator<ATRLOG, double>(atr);
                
            } else if (indicator_config.type == "SUPERTREND") {
                int atr_period = static_cast<int>(indicator_config.parameters.at("atr_period"));
                double multiplier = indicator_config.parameters.at("multiplier");
                auto st = std::make_shared<SUPERTREND>(atr_period, multiplier);
                
                if (indicator_config.name.find("filter") != std::string::npos) {
                    supertrend_filter_calculator = st;
                } else if (indicator_config.name.find("tp") != std::string::npos) {
                    supertrend_tp_calculator = st;
                }
                
                indicator_manager->registerIndicator<SUPERTREND, std::pair<double, int>>(st);
            }
            
            logger->log_general("Indicateur enregistré: " + indicator_config.name + " (" + indicator_config.type + ")", LogLevel::DEBUG);
            
        } catch (const std::exception& e) {
            logger->log_general("Erreur lors de l'enregistrement de l'indicateur " + indicator_config.name + ": " + e.what(), LogLevel::ERROR);
        }
    }
    
    /**
     * @brief Evaluate a generic filter based on its configuration
     */
    bool evaluateGenericFilter(const FilterConfig& filter_config) {
        try {
            double val1 = getValueFromSource(filter_config.value1);
            double val2 = getValueFromSource(filter_config.value2);
            
            bool result = false;
            
            switch (filter_config.comparison) {
                case ComparisonType::THRESHOLD_ABOVE:
                    result = val1 > val2;
                    break;
                case ComparisonType::THRESHOLD_BELOW:
                    result = val1 < val2;
                    break;
                case ComparisonType::CROSSOVER_ABOVE:
                    result = checkCrossover(filter_config, true);
                    break;
                case ComparisonType::CROSSOVER_BELOW:
                    result = checkCrossover(filter_config, false);
                    break;
            }
            
            logger->log_filter_result(filter_config.name, result, 
                "Val1: " + logger->fast_double_to_string(val1) + 
                ", Val2: " + logger->fast_double_to_string(val2));
            
            return result;
            
        } catch (const std::exception& e) {
            logger->log_general("Erreur dans le filtre " + filter_config.name + ": " + e.what(), LogLevel::ERROR);
            return false;
        }
    }
    
    /**
     * @brief Get value from a value source (price, indicator, constant)
     */
    double getValueFromSource(const ValueSource& source) {
        switch (source.type) {
            case ValueType::CONSTANT:
                return source.constantValue;
                
            case ValueType::PRICE:
                return price(); // Current price
                
            case ValueType::INDICATOR: {
                // Get value from the appropriate indicator
                if (source.identifier == "ema_short" && ema_short_calculator) {
                    return getHistoricalValue(source.identifier, source.historicalOffset, ema_short_calculator->get_value());
                } else if (source.identifier == "ema_long" && ema_long_calculator) {
                    return getHistoricalValue(source.identifier, source.historicalOffset, ema_long_calculator->get_value());
                } else if (source.identifier == "rsi" && rsi_calculator) {
                    return getHistoricalValue(source.identifier, source.historicalOffset, rsi_calculator->get_value());
                } else if (source.identifier == "stoch_k" && stochastic_calculator) {
                    auto stoch_val = stochastic_calculator->get_value();
                    return getHistoricalValue(source.identifier, source.historicalOffset, stoch_val.first);
                } else if (source.identifier == "stoch_d" && stochastic_calculator) {
                    auto stoch_val = stochastic_calculator->get_value();
                    return getHistoricalValue(source.identifier, source.historicalOffset, stoch_val.second);
                } else if (source.identifier == "atr" && atrlog_filter_calculator) {
                    return getHistoricalValue(source.identifier, source.historicalOffset, atrlog_filter_calculator->get_value());
                } else if (source.identifier == "supertrend" && supertrend_filter_calculator) {
                    auto st_val = supertrend_filter_calculator->get_value();
                    return getHistoricalValue(source.identifier, source.historicalOffset, st_val.first);
                }
                
                logger->log_general("Indicateur non trouvé: " + source.identifier, LogLevel::WARNING);
                return price(); // Fallback to price
            }
            
            default:
                throw std::runtime_error("Type de source de valeur non supporté");
        }
    }
    
    /**
     * @brief Get historical value with offset
     */
    double getHistoricalValue(const std::string& indicator_name, int offset, double current_value) {
        if (offset == 0) return current_value;
        
        auto it = indicator_values.find(indicator_name);
        if (it != indicator_values.end() && offset < static_cast<int>(it->second.size())) {
            return it->second[offset - 1]; // offset-1 because [0] is current, [1] is previous
        }
        
        return current_value; // Fallback to current if no historical data
    }
    
    /**
     * @brief Check crossover conditions
     */
    bool checkCrossover(const FilterConfig& filter_config, bool above) {
        // Simplified crossover detection - needs historical values
        double current_val1 = getValueFromSource(filter_config.value1);
        double current_val2 = getValueFromSource(filter_config.value2);
        
        // Get previous values
        ValueSource prev_val1 = filter_config.value1;
        ValueSource prev_val2 = filter_config.value2;
        prev_val1.historicalOffset = 1;
        prev_val2.historicalOffset = 1;
        
        double previous_val1 = getValueFromSource(prev_val1);
        double previous_val2 = getValueFromSource(prev_val2);
        
        if (above)  
            return (previous_val1 <= previous_val2) && (current_val1 > current_val2);
        else 
            return (previous_val1 >= previous_val2) && (current_val1 < current_val2);
        
    }
    
    /**
     * @brief Update indicator values for historical access
     */
    void updateIndicatorValues() {
        // Update single value indicators
        if (ema_short_calculator) updateIndicatorHistory("ema_short", ema_short_calculator->get_value());
        if (ema_long_calculator) updateIndicatorHistory("ema_long", ema_long_calculator->get_value());
        if (rsi_calculator) updateIndicatorHistory("rsi", rsi_calculator->get_value());
        if (atrlog_filter_calculator) updateIndicatorHistory("atr", atrlog_filter_calculator->get_value());
        
        // Update pair value indicators
        if (stochastic_calculator) {
            auto stoch_val = stochastic_calculator->get_value();
            updateIndicatorHistory("stoch_k", stoch_val.first);
            updateIndicatorHistory("stoch_d", stoch_val.second);
        }
        
        if (supertrend_filter_calculator) {
            auto st_val = supertrend_filter_calculator->get_value();
            updateIndicatorHistory("supertrend", st_val.first);
        }
    }
    
    /**
     * @brief Update historical values after each update
     */
    void updateHistoricalValues() {
        // This method is called in after() to shift historical values
        // The actual shifting is done in updateIndicatorHistory during before()
    }
    
    /**
     * @brief Update indicator history buffer
     */
    void updateIndicatorHistory(const std::string& name, double value) {
        auto& history = indicator_values[name];
        
        // Insert at beginning and limit size
        history.insert(history.begin(), value);
        if (history.size() > 10) history.resize(10);
        
    }

public:
    GenericStrategy(const StrategyBaseConfig& base_cfg, const GenericStrategyConfig& generic_cfg) 
        : Strategy(base_cfg), config(generic_cfg) {
        
        logger->log_general("Initialisation de GenericStrategy: " + config.name, LogLevel::INFO);
        
        // Configure direction support
        
        logger->log_general("Direction configurée: " + std::string(config.direction ? "LONG" : "SHORT"), LogLevel::INFO);
        
        // Register indicators first (they're needed for filters)
        registerFiltersIndicators();
        
        // Then register filters
        registerFilters();
        
        logger->log_general("GenericStrategy initialisée avec " + 
                           std::to_string(config.indicators.size()) + " indicateurs et " +
                           std::to_string(config.filters.size()) + " filtres", LogLevel::INFO);
    }
};