#pragma once

#include "strategy.h"
#include "Filters.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <functional>

struct GenericStrategyConfig {
    std::string name; // à mettre dans StrategyBaseConfig
    std::optional<bool> go_direction = std::nullopt;  // true <=> LONG, false <=> SHORT
    std::vector<GenericFilter> filters;

    // Overload the << operator for easy printing
    friend std::ostream& operator<<(std::ostream& os, const GenericStrategyConfig& config) {
        os << "GenericStrategyConfig {\n"
           << "  Go Direction: " << (config.go_direction == std::nullopt ? "Not Set" : (config.go_direction.value() ? "LONG" : "SHORT")) << "\n";
           for (const auto& filter : config.filters)
               os << "  Filter: " << filter.description << "\n";
        return os;
    }
};

class GenericStrategy : public Strategy {
private:
    GenericStrategyConfig config;

    ATRParams atrlog_params;

    // methode rendu inutile
    void before() override {

        // if (config.use_stoch_filter && !stoch_kd_values.empty()) {
        //     stoch_kd_values[0] = indicator_manager->getStochasticValue(stoch_params);
        // }
        
        // if (config.use_rsi_filter && !rsi_values.empty()) {
        //     rsi_values[0] = indicator_manager->getRSIValue(rsi_params);
        // }
        
        // if (config.use_atr_filter && !atr_values.empty()) {
        //     atr_values[0] = indicator_manager->getATRValue(atrlog_filter_params);
        // }
    }
    
    // No should_long() or should_short() override - all logic handled by filters
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
            candle_manager.get(), 
            candle_manager->get_latest_candle(), 
            logger
        );

        // Calculate Take Profit
        take_profit_distance = PositionManager::calculateTakeProfit(
            base_config,
            price(),
            indicator_manager->getATRValue(atrlog_params),
            stop_loss_distance,
            candle_manager.get(),
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

    // Inutile, les filtres sont dans le std::vector<GenericFilter>
    void registerFilters() {
        active_filters.clear();
        
        // il faut ajouter un filtre sur le nombre minimum de bougies a avoir dans le candle manager avant de commencer a trader
        active_filters.push_back([this]() {
            if (candle_manager->size() < 3) {
                logger->log_general("Pas assez d'historique (min 3 bougies)", LogLevel::WARNING);
                return false;
            }

            if (base_config.sl_method == StopLossMethod::MinMax && candle_manager->size() < static_cast<size_t>(base_config.sl_minmax_periods)) {
                logger->log_general("Pas assez d'historique pour le calcul Min/Max SL", LogLevel::WARNING);
                return false;
            }

            return true;
        });


        // Nouveau filtre pour vérifier si la bougie HA précédente est rouge ou verte qui remplace la condition dans should_long et should_short
        // if (config.go_direction.value()) // true => LONG
        //     active_filters.push_back([this]() {
        //         return Filters::previousHACandlesGreen(candle_manager, 1, 0, "Bougie HA précédente", logger.get());
        //     });
        // else // false => SHORT
        //     active_filters.push_back([this]() {
        //         return Filters::previousHACandlesRed(candle_manager, 1, 0, "Bougie HA précédente", logger.get());
        //     });


        // active_filters.push_back([this]() {
        //     return filterEvaluator->evaluateAll(filters);
        // });

        // if (config.use_ema_short_filter)
        //     active_filters.push_back([this]() {
        //         return Filters::priceSupEMA(price(), indicator_manager->getEMAValue(ema_short_params), "EMA Short", logger.get());
        //     });
        // if (config.use_ema_long_filter)
        //     active_filters.push_back([this]() {
        //         return Filters::priceSupEMA(price(), indicator_manager->getEMAValue(ema_long_params), "EMA Long", logger.get());
        //     });
        // if (config.use_stoch_filter)
        //     active_filters.push_back([this]() {
        //         return Filters::stochInfThreshold(stoch_kd_values, config.stoch_threshold, "Stoch", logger.get());
        //     });
        // if (config.use_rsi_filter)
        //     active_filters.push_back([this]() {
        //         return Filters::rsiInfThreshold(rsi_values, config.rsi_threshold, "RSI", logger.get());
        //     });
        // if (config.use_atr_filter)
        //     active_filters.push_back([this]() {
        //         return Filters::atrAboveThreshold(atr_values, config.atr_threshold, "ATR", logger.get());
        //     });
        // if (config.use_previous_ha_candle_red_filter)
        //     active_filters.push_back([this]() {
        //         return Filters::previousHACandlesRed(candle_manager, config.previous_ha_candle_red_filter_n, 1, "Bougie HA précédente", logger.get());
        //     });
        // if (config.use_supertrend_filter)
        //     active_filters.push_back([this]() {
        //         return Filters::priceSupSupertrend(price(), indicator_manager->getSuperTrendValue(supertrend_filter_params), "Supertrend", logger.get());
        //     });
    }

    // TODO : Il faut lire les filtres et extraire les indicateur qu'il faudra calculer et informer le indicator_manager
    // void registerIndicators() {
    //     // Register only active indicators
    //     if (config.use_ema_short_filter)
    //         indicator_manager->registerEMA(ema_short_params);
        
    //     if (config.use_ema_long_filter)
    //         indicator_manager->registerEMA(ema_long_params);

    //     if (config.use_stoch_filter)
    //         indicator_manager->registerStochastic(stoch_params);

    //     if (config.use_rsi_filter)
    //         indicator_manager->registerRSI(rsi_params);

    //     if (config.use_atr_filter)
    //         indicator_manager->registerATR(atrlog_filter_params);

    //     if (base_config.sl_method == StopLossMethod::ATR || base_config.tp_method == TakeProfitMethod::ATR)
    //         indicator_manager->registerATR(atrlog_params);

    //     if (config.use_supertrend_filter)
    //         indicator_manager->registerSuperTrend(supertrend_filter_params);

    //     if (base_config.tp_method == TakeProfitMethod::SuperTrend)
    //         indicator_manager->registerSuperTrend(supertrend_tp_params);
    // }

public:
    GenericStrategy(const StrategyBaseConfig& base_cfg, const GenericStrategyConfig& generic_cfg) 
        : Strategy(base_cfg), config(generic_cfg),
          atrlog_params(base_cfg.atr_period, true) {


        if (!config.go_direction.has_value()) {
            logger->log_general("La direction (go_direction) n'est pas définie dans la configuration.", LogLevel::ERROR);
            throw std::invalid_argument("Direction (go_direction) must be specified in GenericStrategyConfig");
        }

        // a tester
        GenericFilter filterEMA(
            ValueSource::Price(PriceType::CLOSE), 
            ComparisonOperator::GREATER_THAN, 
            ValueSource::EMA(20), 
            TemporalLogic::CURRENT, 
            1
        );

        indicator_manager->registerEMA(filterEMA.rightValue.emaParams);

        // GenericFilter filterStoch(
        //     ValueSource::StochasticK(stoch_params.fastK, stoch_params.slowK, stoch_params.slowD), 
        //     ComparisonOperator::LESS_THAN, 
        //     ValueSource::Constant(static_cast<double>(config.stoch_threshold)), 
        //     TemporalLogic::ANY_OF, 
        //     config.stoch_history_periods
        // );

        // GenericFilter filterHAGreen(
        //     ValueSource::CandleProperty(CandlePropertyType::HEIKIN_ASHI_IS_GREEN),
        //     ComparisonOperator::EQUAL,
        //     ValueSource::Constant(1.0), // 1.0 pour vrai
        //     TemporalLogic::CURRENT
        // );

        // GenericFilter filterPrevHARed(
        //     ValueSource::CandleProperty(CandlePropertyType::HEIKIN_ASHI_IS_RED, 1),
        //     ComparisonOperator::EQUAL,
        //     ValueSource::Constant(1.0), // 1.0 pour vrai
        //     TemporalLogic::ALL_OF,
        //     config.previous_ha_candle_red_filter_n
        // );

        // filters.push_back(filterHAGreen);
        // filters.push_back(filterPrevHARed);
        // filters.push_back(filterEMA);
        // filters.push_back(filterStoch);

        // registerFilters();
        // registerIndicators();
    }
};