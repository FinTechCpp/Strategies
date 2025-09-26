#pragma once

#include "strategy.h"
#include "Filters.h"
#include <algorithm>
#include <cmath>
#include <set>
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
            indicator_manager->getATRValue(ATRParams(base_config.atr_period, true)), 
            config.go_direction.value(),  // is_long = true 
            *candle_manager, 
            logger.get()
        );

        // Calculate Take Profit
        take_profit_distance = PositionManager::calculateTakeProfit(
            base_config,
            price(),
            indicator_manager->getATRValue(ATRParams(base_config.atr_period, true)),
            stop_loss_distance,
            *candle_manager,
            logger.get()
        );

        // Calculate position size
        double quantity = PositionManager::calculatePositionSize(
            base_config,
            price(),
            stop_loss_distance,
            logger.get()
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

    void registerFiltersIndicators() {

        // Fonction helper pour enregistrer un indicateur une seule fois
        auto registerIfNeeded = [this](const ValueSource& source) {
            if (source.category != ValueCategory::INDICATOR) {
                return;
            }
            
            switch (source.indicatorType) {
                case IndicatorType::EMA:
                    indicator_manager->registerEMA(source.emaParams);
                    break;
                case IndicatorType::RSI:
                    indicator_manager->registerRSI(source.rsiParams);
                    break;
                case IndicatorType::ATR:
                    indicator_manager->registerATR(source.atrParams);
                    break;
                case IndicatorType::STOCHASTIC_K:
                case IndicatorType::STOCHASTIC_D:
                    indicator_manager->registerStochastic(source.stochParams);
                    break;
                case IndicatorType::SUPERTREND_VALUE:
                case IndicatorType::SUPERTREND_DIRECTION:
                    indicator_manager->registerSuperTrend(source.supertrendParams);
                    break;
                default:
                    break;
            }
        };

        // Parcourir tous les filtres et enregistrer directement les indicateurs nécessaires
        for (const auto& filter : filters) {
            registerIfNeeded(filter.leftValue);
            registerIfNeeded(filter.rightValue);
        }
    }

public:
    GenericStrategy(const StrategyBaseConfig& base_cfg, const GenericStrategyConfig& generic_cfg) 
        : Strategy(base_cfg), config(generic_cfg),
          atrlog_params(base_cfg.atr_period, true) {


        if (!config.go_direction.has_value()) {
            logger->log_general("La direction (go_direction) n'est pas définie dans la configuration.", LogLevel::ERROR);
            throw std::invalid_argument("Direction (go_direction) must be specified in GenericStrategyConfig");
        }

        // a tester
        // GenericFilter filterEMA(
        //     ValueSource::Price(PriceType::CLOSE), 
        //     ComparisonOperator::GREATER_THAN, 
        //     ValueSource::EMA(20), 
        //     TemporalLogic::CURRENT, 
        //     1
        // );

        // indicator_manager->registerEMA(filterEMA.rightValue.emaParams);

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


        filters = config.filters;

        registerFiltersIndicators();
    }
};