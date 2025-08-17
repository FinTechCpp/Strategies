#pragma once

#include "strategy.h"
#include "Filters.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <functional>

struct SellHeikinRedConfig {
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
    int previous_ha_candle_green_filter_n;

    int rsi_history_periods;
    int stoch_history_periods;
    
    bool use_ema_short_filter=false;
    bool use_ema_long_filter=false;
    bool use_stoch_filter=false;
    bool use_rsi_filter=false;
    bool use_supertrend_filter=false;
    bool use_previous_ha_candle_green_filter=false;

    // Overload the << operator for easy printing
    friend std::ostream& operator<<(std::ostream& os, const SellHeikinRedConfig& config) {
        os << "SellHeikinRedConfig {\n"
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
           << "  Use Previous HA Candle Red Filter: " << (config.use_previous_ha_candle_green_filter ? "Yes" : "No") << "\n"
           << "  Previous HA Candle Red Filter N: " << config.previous_ha_candle_green_filter_n << "\n"
           << "}";
        return os;
    }
};

class SellHeikinRed : public Strategy {
private:
    SellHeikinRedConfig config;
    
    // Indicator calculators
    std::shared_ptr<EMA> ema_short_calculator;
    std::shared_ptr<EMA> ema_long_calculator;
    std::shared_ptr<STOCH> stochastic_calculator;
    std::shared_ptr<ATRLOG> atrlog_calculator;
    std::shared_ptr<RSI> rsi_calculator;
    std::shared_ptr<SUPERTREND> supertrend_filter_calculator;
    std::shared_ptr<SUPERTREND> supertrend_tp_calculator;  // For TP functionality

    // Indicator values
    std::vector<std::pair<double, double>> stoch_kd_values;  // [0] = actuel, [1] = précédent, etc.
    std::vector<double> rsi_values;      // [0] = actuel, [1] = précédent, etc.
        

    void before() override {
        
        // Obtenir la dernière bougie HA pour journalisation
        BasicCandle ha_current = candle_manager.get_latest_heikin_ashi();
        bool is_green = candle_manager.is_candle_green(ha_current);
        
        logger->log_general("Bougie HA courante calculée: Open=" + std::to_string(ha_current.open) + 
                        ", Close=" + std::to_string(ha_current.close) + 
                        ", " + (is_green ? "VERTE" : "ROUGE"), LogLevel::INFO);

        // Mise à jour des valeurs historiques actuelles
        if (config.use_stoch_filter && !stoch_kd_values.empty()) {
            stoch_kd_values[0] = stochastic_calculator->get_value();
        }
        
        if (config.use_rsi_filter && !rsi_values.empty()) {
            rsi_values[0] = rsi_calculator->get_value();
        }
    }
    
    bool should_long() override {
        logger->log_general("Cette stratégie ne prend pas de positions longues", LogLevel::WARNING);
        return false;  // Cette stratégie ne prend pas de positions longues
    }
    
    bool should_short() override {  
        if (candle_manager.size() < 3) {
            logger->log_general("Pas assez d'historique (min 3 bougies)", LogLevel::WARNING);
            return false;
        }
        
        // Vérifier si on a besoin de Min/Max mais qu'on n'a pas assez d'historique
        if (base_config.sl_method == StopLossMethod::MinMax && candle_manager.size() < static_cast<size_t>(base_config.sl_minmax_periods)) {
            logger->log_general("Pas assez d'historique pour le calcul Min/Max SL", LogLevel::WARNING);
            return false;
        }
        return !candle_manager.is_latest_heikin_ashi_green();
    }
    
    void go_long() override {
        throw std::runtime_error("SellHeikinRed strategy does not support long positions");
    }
    
    void go_short() override {
        logger->log_general("Préparation d'un signal SHORT", LogLevel::INFO);
    
        // Calcul du Stop Loss
        stop_loss_distance = PositionManager::calculateStopLoss(
            base_config, 
            price(), 
            atrlog_calculator->get_value(), 
            false,  // is_long = false (SHORT)
            candle_manager, 
            candle_manager.get_latest_candle(), 
            logger
        );
        
        // Calcul du Take Profit
        take_profit_distance = PositionManager::calculateTakeProfit(
            base_config,
            price(),
            atrlog_calculator->get_value(),
            stop_loss_distance,
            candle_manager,
            logger
        );
        
        // Calcul de la taille de position
        sell_quantity = PositionManager::calculatePositionSize(
            base_config,
            price(),
            stop_loss_distance,
            logger
        );
        
        sell_price = price();
    }

    void after() override {
        // Effectuer le décalage des valeurs historiques après chaque mise à jour
        
        // Décaler les valeurs de stochastique
        if (config.use_stoch_filter && !stoch_kd_values.empty() && stoch_kd_values[0].first > 0) {
            // Décaler toutes les valeurs d'une position
            for (int i = stoch_kd_values.size() - 1; i > 0; i--) {
                stoch_kd_values[i] = stoch_kd_values[i-1];
            }
        }
        
        // Décaler les valeurs de RSI
        if (config.use_rsi_filter && !rsi_values.empty() && rsi_values[0] > 0) {
            // Décaler toutes les valeurs d'une position
            for (int i = rsi_values.size() - 1; i > 0; i--) {
                rsi_values[i] = rsi_values[i-1];
            }
        }
    }

    void registerFilters() {
        if (config.use_ema_short_filter) {
            active_filters.push_back([this]() { 
                return Filters::priceInfEMA(price(), ema_short_calculator->get_value(), 
                                           ema_short_calculator->get_name(), logger.get());
            });
        }
        
        if (config.use_ema_long_filter) {
            active_filters.push_back([this]() { 
                return Filters::priceInfEMA(price(), ema_long_calculator->get_value(), ema_long_calculator->get_name(), logger.get());
            });
        }
        
        if (config.use_stoch_filter) {
            active_filters.push_back([this]() {
                return Filters::stochAboveThreshold(stoch_kd_values, config.stoch_threshold, stochastic_calculator->get_name(), logger.get());
            });
        }
        
        if (config.use_rsi_filter) {
            active_filters.push_back([this]() {
                return Filters::rsiAboveThreshold(rsi_values, config.rsi_threshold, rsi_calculator->get_name(), logger.get());
            });
        }
        
        if (config.use_previous_ha_candle_green_filter) {
            active_filters.push_back([this]() {
                return Filters::previousHACandlesGreen(candle_manager, config.previous_ha_candle_green_filter_n, "Bougie HA précédente", logger.get());
            });
        }

        if (config.use_supertrend_filter) {
            active_filters.push_back([this]() {
                return Filters::priceInfSupertrend(price(), supertrend_filter_calculator->get_value(), supertrend_filter_calculator->get_name(), logger.get());
            });
        }
    }
    
    void registerIndicators() {
        // Enregistrer les indicateurs actifs uniquement
        if (config.use_ema_short_filter)
            indicator_manager->registerIndicator<EMA, double>(ema_short_calculator);
        
        if (config.use_ema_long_filter)
            indicator_manager->registerIndicator<EMA, double>(ema_long_calculator);
        
        if (config.use_stoch_filter)
            indicator_manager->registerIndicator<STOCH, std::pair<double, double>>(stochastic_calculator);
        
        if (config.use_rsi_filter)
            indicator_manager->registerIndicator<RSI, double>(rsi_calculator);

        if (base_config.sl_method == StopLossMethod::ATR || base_config.tp_method == TakeProfitMethod::ATR)
            indicator_manager->registerIndicator<ATRLOG, double>(atrlog_calculator);

        if (config.use_supertrend_filter)
            indicator_manager->registerIndicator<SUPERTREND, std::pair<double, int>>(supertrend_filter_calculator);
        
        if (base_config.tp_method == TakeProfitMethod::SuperTrend)
            indicator_manager->registerIndicator<SUPERTREND, std::pair<double, int>>(supertrend_tp_calculator);
    }

public:
    SellHeikinRed(const StrategyBaseConfig& base_cfg, const SellHeikinRedConfig& shr_cfg) 
        : Strategy(base_cfg), config(shr_cfg) {
        
        // Initialize indicator calculators
        ema_short_calculator = std::make_shared<EMA>(config.ema_short_period);
        ema_long_calculator = std::make_shared<EMA>(config.ema_long_period);
        stochastic_calculator = std::make_shared<STOCH>(config.stoch_fastk, config.stoch_slowk, config.stoch_slowd);
        rsi_calculator = std::make_shared<RSI>(config.rsi_period);
        atrlog_calculator = std::make_shared<ATRLOG>(base_cfg.atr_period);
        supertrend_filter_calculator = std::make_shared<SUPERTREND>(config.supertrend_atr_period, config.supertrend_multiplier);
        supertrend_tp_calculator = std::make_shared<SUPERTREND>(base_cfg.tp_supertrend_atr_period, base_cfg.tp_supertrend_multiplier);

        // Initialisation des vecteurs avec la taille appropriée
        stoch_kd_values.resize(config.stoch_history_periods, {0.0, 0.0});
        rsi_values.resize(config.rsi_history_periods, 0.0);
        
        registerFilters();
        registerIndicators();
    }
};