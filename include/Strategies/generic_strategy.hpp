#pragma once

#include "strategy.h"
#include "Filters.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <functional>
#include <set>
#include <iostream>

struct GenericStrategyConfig {
    std::string name; // à mettre dans StrategyBaseConfig
    std::optional<bool> go_direction = std::nullopt;  // true <=> LONG, false <=> SHORT
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
        << "  Name: " << config.name << "\n"
        << "  Go Direction: " << (config.go_direction == std::nullopt ? "Not Set" : (config.go_direction.value() ? "LONG" : "SHORT")) << "\n"
        << "  Generic Filters (" << config.filters.size() << " filters):\n";
        
        for (size_t i = 0; i < config.filters.size(); ++i) {
            const auto& filter = config.filters[i];
            os << "    Filter " << (i + 1) << ": " << filter.description << "\n"
            << "      Left Value: " << filter.leftValue.getDescription() << "\n"
            << "      Operator: ";
            
            switch (filter.op) {
                case ComparisonOperator::GREATER_THAN: os << "GREATER_THAN"; break;
                case ComparisonOperator::LESS_THAN: os << "LESS_THAN"; break;
                case ComparisonOperator::GREATER_OR_EQUAL: os << "GREATER_OR_EQUAL"; break;
                case ComparisonOperator::LESS_OR_EQUAL: os << "LESS_OR_EQUAL"; break;
                case ComparisonOperator::EQUAL: os << "EQUAL"; break;
                case ComparisonOperator::NOT_EQUAL: os << "NOT_EQUAL"; break;
                case ComparisonOperator::CROSSES_ABOVE: os << "CROSSES_ABOVE"; break;
                case ComparisonOperator::CROSSES_BELOW: os << "CROSSES_BELOW"; break;
            }
            
            os << "\n      Right Value: " << filter.rightValue.getDescription() << "\n"
            << "      Temporal Logic: ";
            
            switch (filter.temporalLogic) {
                case TemporalLogic::CURRENT: os << "CURRENT"; break;
                case TemporalLogic::ANY_OF: os << "ANY_OF"; break;
                case TemporalLogic::ALL_OF: os << "ALL_OF"; break;
            }
            
            os << "\n      Lookback Periods: " << filter.lookbackPeriods << "\n"
            << "      Enabled: " << (filter.enabled ? "Yes" : "No") << "\n";
            
            if (!filter.description.empty()) {
                os << "      Description: " << filter.description << "\n";
            }
        }
        
        if (config.filters.empty()) {
            os << "    No generic filters configured!\n";
        }
        
        os << "  ========================\n"
        << "  DEPRECATED HARDCODED FILTERS (should be removed):\n"
        << "  EMA Short Period: " << config.ema_short_period << " (Used: " << (config.use_ema_short_filter ? "Yes" : "No") << ")\n"
        << "  EMA Long Period: " << config.ema_long_period << " (Used: " << (config.use_ema_long_filter ? "Yes" : "No") << ")\n"
        << "  Stochastic (Used: " << (config.use_stoch_filter ? "Yes" : "No") << "):\n"
        << "    Fast K: " << config.stoch_fastk << "\n"
        << "    Slow K: " << config.stoch_slowk << "\n"
        << "    Slow D: " << config.stoch_slowd << "\n"
        << "    Threshold: " << config.stoch_threshold << "\n"
        << "    History Periods: " << config.stoch_history_periods << "\n"
        << "  Use Previous HA Candle Red Filter: " << (config.use_previous_ha_candle_red_filter ? "Yes" : "No") << "\n"
        << "}";
        return os;
    }
};

class GenericStrategy : public Strategy {
private:
    GenericStrategyConfig config;

    // Deprecated
    EMAParams ema_short_params;
    EMAParams ema_long_params;
    StochasticParams stoch_params;
    RSIParams rsi_params;
    ATRParams atrlog_params;
    ATRParams atrlog_filter_params;
    SuperTrendParams supertrend_filter_params;
    SuperTrendParams supertrend_tp_params;

    // TODO a mettre dans la class mere
    // std::unique_ptr<FilterEvaluator> filterEvaluator;
    std::vector<GenericFilter> filters;
    
    // Indicator values
    // std::vector<std::pair<double, double>> stoch_kd_values;
    // std::vector<double> rsi_values;
    // std::vector<double> atr_values;

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

    // Inutile
    void after() override {
        // Perform historical value shifting after each update

        // // Shift stochastic values
        // if (config.use_stoch_filter && !stoch_kd_values.empty() && stoch_kd_values[0].first > 0) {
        //     // Shift all values by one position
        //     for (int i = stoch_kd_values.size() - 1; i > 0; i--) {
        //         stoch_kd_values[i] = stoch_kd_values[i-1];
        //     }
        // }

        // // Shift RSI values
        // if (config.use_rsi_filter && !rsi_values.empty() && rsi_values[0] > 0) {
        //     // Shift all values by one position
        //     for (int i = rsi_values.size() - 1; i > 0; i--) {
        //         rsi_values[i] = rsi_values[i-1];
        //     }
        // }

        // // Shift ATR values
        // if (config.use_atr_filter && !atr_values.empty() && atr_values[0] > 0) {
        //     // Shift all values by one position
        //     for (int i = atr_values.size() - 1; i > 0; i--) {
        //         atr_values[i] = atr_values[i-1];
        //     }
        // }
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


        active_filters.push_back([this]() {
            return filterEvaluator->evaluateAll(filters);
        });

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
    void registerIndicators() {
        // Extraire automatiquement les indicateurs des filtres génériques
        std::set<IndicatorType> required_indicators;
        std::set<int> ema_periods;
        std::set<int> rsi_periods;
        std::set<int> atr_periods;
        
        for (const auto& filter : filters) {
            // Vérifier leftValue
            if (filter.leftValue.category == ValueCategory::INDICATOR) {
                required_indicators.insert(filter.leftValue.indicatorType);
                
                // Extraire les périodes spécifiques
                if (filter.leftValue.indicatorType == IndicatorType::EMA) {
                    ema_periods.insert(filter.leftValue.emaParams.period);
                } else if (filter.leftValue.indicatorType == IndicatorType::RSI) {
                    rsi_periods.insert(filter.leftValue.rsiParams.period);
                } else if (filter.leftValue.indicatorType == IndicatorType::ATR) {
                    atr_periods.insert(filter.leftValue.atrParams.period);
                }
            }
            // Vérifier rightValue  
            if (filter.rightValue.category == ValueCategory::INDICATOR) {
                required_indicators.insert(filter.rightValue.indicatorType);
                
                // Extraire les périodes spécifiques
                if (filter.rightValue.indicatorType == IndicatorType::EMA) {
                    ema_periods.insert(filter.rightValue.emaParams.period);
                } else if (filter.rightValue.indicatorType == IndicatorType::RSI) {
                    rsi_periods.insert(filter.rightValue.rsiParams.period);
                } else if (filter.rightValue.indicatorType == IndicatorType::ATR) {
                    atr_periods.insert(filter.rightValue.atrParams.period);
                }
            }
        }
        
        // Enregistrer les indicateurs requis avec leurs vraies périodes
        for (IndicatorType indicator : required_indicators) {
            switch (indicator) {
                case IndicatorType::EMA:
                    // Enregistrer tous les EMAs avec leurs périodes spécifiques
                    for (int period : ema_periods) {
                        EMAParams emaParam(period);
                        indicator_manager->registerEMA(emaParam);
                        std::cout << "Registered EMA with period " << period << std::endl;
                    }
                    break;
                case IndicatorType::STOCHASTIC_K:
                case IndicatorType::STOCHASTIC_D:
                    indicator_manager->registerStochastic(stoch_params);
                    std::cout << "Registered Stochastic indicators" << std::endl;
                    break;
                case IndicatorType::RSI:
                    // Enregistrer tous les RSIs avec leurs périodes spécifiques  
                    for (int period : rsi_periods) {
                        RSIParams rsiParam(period);
                        indicator_manager->registerRSI(rsiParam);
                        std::cout << "Registered RSI with period " << period << std::endl;
                    }
                    break;
                case IndicatorType::ATR:
                    // Enregistrer tous les ATRs avec leurs périodes spécifiques
                    for (int period : atr_periods) {
                        ATRParams atrParam(period, true); // useLog = true par défaut
                        indicator_manager->registerATR(atrParam);
                        std::cout << "Registered ATR with period " << period << std::endl;
                    }
                    break;
                case IndicatorType::SUPERTREND_VALUE:
                case IndicatorType::SUPERTREND_DIRECTION:
                    indicator_manager->registerSuperTrend(supertrend_filter_params);
                    std::cout << "Registered SuperTrend indicator" << std::endl;
                    break;
                default:
                    break;
            }
        }

        // Toujours enregistrer ATR pour SL/TP si nécessaire
        if (base_config.sl_method == StopLossMethod::ATR || base_config.tp_method == TakeProfitMethod::ATR)
            indicator_manager->registerATR(atrlog_params);

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
          supertrend_tp_params(base_cfg.tp_supertrend_atr_period, base_cfg.tp_supertrend_multiplier) {

        if (!config.go_direction.has_value()) {
            logger->log_general("La direction (go_direction) n'est pas définie dans la configuration.", LogLevel::ERROR);
            throw std::invalid_argument("Direction (go_direction) must be specified in GenericStrategyConfig");
        }

        // PROBLEME : Vous utilisez des filtres hardcodés au lieu des filtres de la configuration !
        // Supprimons le code hardcodé et utilisons config.filters
        
        // Utiliser les filtres de la configuration au lieu de les créer
        filters = config.filters;
        
        // Debug : afficher la configuration reçue
        std::cout << "Configuration reçue dans GenericStrategy:\n" << config << std::endl;
        
        // Debug : afficher les filtres locaux
        std::cout << "Filtres locaux dans GenericStrategy (" << filters.size() << " filtres):\n";
        for (size_t i = 0; i < filters.size(); ++i) {
            std::cout << "  " << (i + 1) << ": " << filters[i].description << "\n";
        }

        filterEvaluator = std::make_unique<FilterEvaluator>(candle_manager.get(), indicator_manager.get(), logger.get());

        registerFilters();
        registerIndicators();
    }
};