#pragma once

#include "strategy.h"
#include "Filters.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <functional>

struct BuyHeikinGreenConfig {
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
    
    bool use_ema_short_filter = false;
    bool use_ema_long_filter = false;
    bool use_stoch_filter = false;
    bool use_rsi_filter = false;
    bool use_previous_ha_candle_red_filter = false;
    bool use_supertrend_filter = false;

    // Overload the << operator for easy printing
    friend std::ostream& operator<<(std::ostream& os, const BuyHeikinGreenConfig& config) {
        os << "BuyHeikinGreenConfig {\n"
           << "  EMA Short Period: " << config.ema_short_period << " (Used: " << (config.use_ema_short_filter ? "Yes" : "No") << ")\n"
           << "  EMA Long Period: " << config.ema_long_period << " (Used: " << (config.use_ema_long_filter ? "Yes" : "No") << ")\n"
           << "  Stochastic (Used: " << (config.use_stoch_filter ? "Yes" : "No") << "):\n"
           << "    Fast K: " << config.stoch_fastk << "\n"
           << "    Slow K: " << config.stoch_slowk << "\n"
           << "    Slow D: " << config.stoch_slowd << "\n"
           << "    Threshold: " << config.stoch_threshold << "\n"
           << "  RSI (Used: " << (config.use_rsi_filter ? "Yes" : "No") << "):\n"
           << "    Period: " << config.rsi_period << "\n"
           << "    Threshold: " << config.rsi_threshold << "\n"
           << "  Supertrend (Used: " << (config.use_supertrend_filter ? "Yes" : "No") << "):\n"
           << "    ATR Period: " << config.supertrend_atr_period << "\n"
           << "    Multiplier: " << config.supertrend_multiplier << "\n"
           << "  Use Previous HA Candle Red Filter: " << (config.use_previous_ha_candle_red_filter ? "Yes" : "No") << "\n"
           << "  Previous HA Candle Red Filter N: " << config.previous_ha_candle_red_filter_n << "\n"     
           << "}";
        return os;
    }
};

class BuyHeikinGreen : public Strategy {
private:
    BuyHeikinGreenConfig config;
    
    // Indicator calculators
    std::unique_ptr<EMA> ema_short_calculator;
    std::unique_ptr<EMA> ema_long_calculator;
    std::unique_ptr<STOCH> stochastic_calculator;
    std::unique_ptr<RSI> rsi_calculator;
    std::unique_ptr<ATRLOG> atrlog_calculator;
    std::unique_ptr<SUPERTREND> supertrend_filter_calculator;  // For filter functionality
    std::unique_ptr<SUPERTREND> supertrend_tp_calculator;      // For TP functionality
    
    // Indicator names
    std::string ema_short_name;
    std::string ema_long_name;
    std::string stoch_k_name;
    std::string stoch_d_name;
    std::string rsi_name;
    std::string atrlog_name;
    std::string supertrend_filter_name;  // For filter SuperTrend
    std::string supertrend_tp_name;      // For TP SuperTrend
    
    // Indicator values
    double current_ema_short = 0.0;
    double current_ema_long = 0.0;
    double current_stoch_k = 0.0;
    double current_stoch_d = 0.0;
    double current_rsi = 0.0;
    double previous_rsi = 0.0;
    double previous_2_rsi = 0.0;
    double current_atrlog = 0.0;
    double k_previous = 0.0;
    double d_previous = 0.0;
    double k_previous_2 = 0.0;
    double k_previous_3 = 0.0;
    double current_supertrend_filter = 0.0;
    int current_supertrend_filter_direction = 0;
    
    // Filters
    std::vector<std::function<bool()>> active_filters;
    
    bool initialize_indicators() {
        // Déterminer la période maximale nécessaire en fonction des indicateurs activés
        int max_period = 0;
        
        if (config.use_ema_short_filter)
            max_period = std::max(max_period, config.ema_short_period);
        
        if (config.use_ema_long_filter)
            max_period = std::max(max_period, config.ema_long_period);
        
        if (config.use_stoch_filter)
            max_period = std::max(max_period, config.stoch_fastk + config.stoch_slowk);
        
        if (config.use_rsi_filter)
            max_period = std::max(max_period, config.rsi_period * 2);
        
        if (config.use_supertrend_filter)
            max_period = std::max(max_period, config.supertrend_atr_period * 2);
        
        if (base_config.use_supertrend_for_tp)
            max_period = std::max(max_period, base_config.tp_supertrend_atr_period * 2);
        
        if (base_config.use_atr_for_sl || base_config.use_atr_for_tp)
            max_period = std::max(max_period, base_config.atr_period * 2);

        // Log du début de l'initialisation
        logger->log_general("Tentative d'initialisation des indicateurs - Période maximale requise: " + 
            logger->fast_int_to_string(max_period) + " bougies", LogLevel::DEBUG);

        // Vérifier si nous avons assez de bougies
        size_t available_candles = candle_manager.size();
        if (available_candles < static_cast<size_t>(max_period)) {
            int remaining = max_period - static_cast<int>(available_candles);
            logger->log_general("Historique insuffisant: " + logger->fast_int_to_string(available_candles) + 
                            "/" + logger->fast_int_to_string(max_period) + " bougies (manque " + 
                            logger->fast_int_to_string(remaining) + " bougies)");
            return false;
        }

        // Récupérer toutes les bougies disponibles
        auto candles = candle_manager.get_last_candles(candle_manager.size());
        logger->log_general("Initialisation avec " + logger->fast_int_to_string(candles.size()) + " bougies");

        // Initialiser chaque indicateur seulement si nécessaire
        bool all_required_initialized = true;

        // Initialize EMAs
        if (config.use_ema_short_filter) {
            logger->log_general("Initialisation de " + ema_short_name + " (période: " + 
                logger->fast_int_to_string(config.ema_short_period) + ")");
                
            current_ema_short = ema_short_calculator->initialize_with_history(candles);
            bool success = (current_ema_short > 0.0);
            all_required_initialized = all_required_initialized && success;

            logger->log_general("Initialisation de " + ema_short_name + ": " + 
                            std::string(success ? "RÉUSSIE" : "ÉCHOUÉE"), 
                            success ? LogLevel::INFO : LogLevel::ERROR);

            if (success) logger->log_indicator_value(ema_short_name, current_ema_short);

        }
        
        if (config.use_ema_long_filter) {
            logger->log_general("Initialisation de " + ema_long_name + " (période: " + 
                logger->fast_int_to_string(config.ema_long_period) + ")");
                
            current_ema_long = ema_long_calculator->initialize_with_history(candles);
            bool success = (current_ema_long > 0.0);
            all_required_initialized = all_required_initialized && success;

            logger->log_general("Initialisation de " + ema_long_name + ": " + 
                            std::string(success ? "RÉUSSIE" : "ÉCHOUÉE"), 
                            success ? LogLevel::INFO : LogLevel::ERROR);

            if (success) logger->log_indicator_value(ema_long_name, current_ema_long);

        }
        
        // Initialize Stochastic
        if (config.use_stoch_filter) {
            logger->log_general("Initialisation de Stochastique (K: " + logger->fast_int_to_string(config.stoch_fastk) + 
            ", K-lent: " + logger->fast_int_to_string(config.stoch_slowk) + 
            ", D: " + logger->fast_int_to_string(config.stoch_slowd) + ")");

            std::pair<double, double> stoch_values = stochastic_calculator->initialize_with_history(candles);
            current_stoch_k = stoch_values.first;
            current_stoch_d = stoch_values.second;
            bool success = (current_stoch_k > 0.0 && current_stoch_d > 0.0);
            all_required_initialized = all_required_initialized && success;

            logger->log_general("Initialisation du Stochastique: " + 
                        std::string(success ? "RÉUSSIE" : "ÉCHOUÉE"), 
                        success ? LogLevel::INFO : LogLevel::ERROR);

            if (success) logger->log_indicator_value(stoch_k_name, current_stoch_k);  logger->log_indicator_value(stoch_d_name, current_stoch_d);
        }
        
        // Initialize RSI
        if (config.use_rsi_filter) {
            logger->log_general("Initialisation du RSI (période: " + 
                logger->fast_int_to_string(config.rsi_period) + ")");

            current_rsi = rsi_calculator->initialize_with_history(candles);
            bool success = (current_rsi > 0.0);
            all_required_initialized = all_required_initialized && success;

            logger->log_general("Initialisation du RSI: " + 
                            std::string(success ? "RÉUSSIE" : "ÉCHOUÉE"), 
                            success ? LogLevel::INFO : LogLevel::ERROR);

            if (success) logger->log_indicator_value(rsi_name, current_rsi);
        }
        
        // Initialize ATRLOG
        if (base_config.use_atr_for_sl || base_config.use_atr_for_tp) {
            logger->log_general("Initialisation de l'ATRLOG (période: " + 
                logger->fast_int_to_string(base_config.atr_period) + ")");
            
            current_atrlog = atrlog_calculator->initialize_with_history(candles);
            bool success = (current_atrlog > 0.0);
            all_required_initialized = all_required_initialized && success;

            logger->log_general("Initialisation de l'ATRLOG: " + 
                            std::string(success ? "RÉUSSIE" : "ÉCHOUÉE"), 
                            success ? LogLevel::INFO : LogLevel::ERROR);

            if (success) logger->log_indicator_value(atrlog_name, current_atrlog);
        }
        
        // Initialize SuperTrend Filter
        if (config.use_supertrend_filter) {
            logger->log_general("Initialisation du Supertrend Filter (période ATR: " + 
                logger->fast_int_to_string(config.supertrend_atr_period) + 
                ", multiplicateur: " + logger->fast_double_to_string(config.supertrend_multiplier) + ")");
            
            auto supertrend_values = supertrend_filter_calculator->initialize_with_history(candles);
            current_supertrend_filter = supertrend_values.first;
            current_supertrend_filter_direction = supertrend_values.second;
            bool success = (current_supertrend_filter > 0.0);
            all_required_initialized = all_required_initialized && success;

            logger->log_general("Initialisation du Supertrend Filter: " + 
                            std::string(success ? "RÉUSSIE" : "ÉCHOUÉE"), 
                            success ? LogLevel::INFO : LogLevel::ERROR);

            if (success) {
                logger->log_indicator_value(supertrend_filter_name, current_supertrend_filter);
                std::string trend_direction = (current_supertrend_filter_direction == 1) ? "UPTREND" : 
                                             (current_supertrend_filter_direction == -1) ? "DOWNTREND" : "NEUTRAL";
                logger->log_filter_detail(supertrend_filter_name, "Direction: " + trend_direction);
            }
        }
        
        // Initialize SuperTrend TP
        if (base_config.use_supertrend_for_tp) {
            logger->log_general("Initialisation du Supertrend TP (période ATR: " + 
                logger->fast_int_to_string(base_config.tp_supertrend_atr_period) + 
                ", multiplicateur: " + logger->fast_double_to_string(base_config.tp_supertrend_multiplier) + ")");
            
            auto supertrend_values = supertrend_tp_calculator->initialize_with_history(candles);
            current_supertrend = supertrend_values.first;
            current_supertrend_direction = supertrend_values.second;
            bool success = (current_supertrend > 0.0);
            all_required_initialized = all_required_initialized && success;

            logger->log_general("Initialisation du Supertrend TP: " + 
                            std::string(success ? "RÉUSSIE" : "ÉCHOUÉE"), 
                            success ? LogLevel::INFO : LogLevel::ERROR);

            if (success) {
                logger->log_indicator_value(supertrend_tp_name, current_supertrend);
                std::string trend_direction = (current_supertrend_direction == 1) ? "UPTREND" : 
                                             (current_supertrend_direction == -1) ? "DOWNTREND" : "NEUTRAL";
                logger->log_filter_detail(supertrend_tp_name, "Direction: " + trend_direction);
            }
        }
        
        // Bilan de l'initialisation
        logger->log_general("Initialisation des indicateurs: " + 
            std::string(all_required_initialized ? "TOUS INITIALISÉS AVEC SUCCÈS" : "CERTAINS ONT ÉCHOUÉ"), 
            all_required_initialized ? LogLevel::INFO : LogLevel::WARNING);

        return all_required_initialized;
    }
    
    bool update_indicators() override {
        if (candle_manager.size() == 0) {
            logger->log_general("candle_manager vide, impossible de mettre à jour les indicateurs", LogLevel::WARNING);
            return false;
        }
        
        // Vérifier si nous devons initialiser les indicateurs
        bool need_ema_short = config.use_ema_short_filter && !ema_short_calculator->initialized();
        bool need_ema_long = config.use_ema_long_filter && !ema_long_calculator->initialized();
        bool need_stoch = config.use_stoch_filter && !stochastic_calculator->initialized();
        bool need_rsi = config.use_rsi_filter && !rsi_calculator->initialized();
        bool need_atrlog = (base_config.use_atr_for_sl || base_config.use_atr_for_tp) && !atrlog_calculator->initialized();
        bool need_supertrend_filter = config.use_supertrend_filter && !supertrend_filter_calculator->initialized();
        bool need_supertrend_tp = base_config.use_supertrend_for_tp && !supertrend_tp_calculator->initialized();
        
        if (need_ema_short || need_ema_long || need_stoch || need_rsi || need_atrlog || need_supertrend_filter || need_supertrend_tp) {            
            // Try to initialize indicators if they're not initialized
            return initialize_indicators();
        }
            
        // Update EMA Short si nécessaire
        if (config.use_ema_short_filter) {
            current_ema_short = ema_short_calculator->update(candle_manager.get_latest_candle());
            logger->log_indicator_value(ema_short_name, current_ema_short);
        }
        
        // Update EMA Long si nécessaire
        if (config.use_ema_long_filter) {
            current_ema_long = ema_long_calculator->update(candle_manager.get_latest_candle());
            logger->log_indicator_value(ema_long_name, current_ema_long);
        }
        
        // Update Stochastic si nécessaire
        if (config.use_stoch_filter) {
            // Update previous values BEFORE updating current values
            k_previous_3 = k_previous_2;
            k_previous_2 = k_previous;
            k_previous = current_stoch_k;
            d_previous = current_stoch_d;
            
            // Now update current values
            auto stoch_values = stochastic_calculator->update(candle_manager.get_latest_candle());
            current_stoch_k = stoch_values.first;
            current_stoch_d = stoch_values.second;
            logger->log_indicator_value(stoch_k_name, current_stoch_k);
            logger->log_indicator_value(stoch_d_name, current_stoch_d);
        }
        
        // Update RSI si nécessaire
        if (config.use_rsi_filter) {
            current_rsi = rsi_calculator->update(candle_manager.get_latest_candle());
            logger->log_indicator_value(rsi_name, current_rsi);
        }
        
        // Update ATRLOG si nécessaire pour SL ou TP
        if (base_config.use_atr_for_sl || base_config.use_atr_for_tp) {
            current_atrlog = atrlog_calculator->update(candle_manager.get_latest_candle());
            logger->log_indicator_value(atrlog_name, current_atrlog);
        }
        
        // Update SuperTrend Filter si nécessaire
        if (config.use_supertrend_filter) {
            auto supertrend_values = supertrend_filter_calculator->update(candle_manager.get_latest_candle());
            current_supertrend_filter = supertrend_values.first;
            current_supertrend_filter_direction = supertrend_values.second;
            logger->log_indicator_value(supertrend_filter_name, current_supertrend_filter);
            
            std::string trend_direction = (current_supertrend_filter_direction == 1) ? "UPTREND" : 
                                         (current_supertrend_filter_direction == -1) ? "DOWNTREND" : "NEUTRAL";
            logger->log_filter_detail(supertrend_filter_name, "Direction: " + trend_direction);
        }
        
        // Update SuperTrend TP si nécessaire
        if (base_config.use_supertrend_for_tp) {
            auto supertrend_values = supertrend_tp_calculator->update(candle_manager.get_latest_candle());
            current_supertrend = supertrend_values.first;
            current_supertrend_direction = supertrend_values.second;
            logger->log_indicator_value(supertrend_tp_name, current_supertrend);
            
            std::string trend_direction = (current_supertrend_direction == 1) ? "UPTREND" : 
                                         (current_supertrend_direction == -1) ? "DOWNTREND" : "NEUTRAL";
            logger->log_filter_detail(supertrend_tp_name, "Direction: " + trend_direction);
        }
        
        return true;
    }

    void before() override {        
        // Obtenir la dernière bougie HA pour journalisation
        BasicCandle ha_current = candle_manager.get_latest_heikin_ashi();
        bool is_green = candle_manager.is_candle_green(ha_current);
        
        logger->log_general("Bougie HA courante calculée: Open=" + logger->fast_double_to_string(ha_current.open) + 
                        ", Close=" + logger->fast_double_to_string(ha_current.close) + 
                        ", Green=" + std::string(is_green ? "Oui" : "Non"));
    }
    
    bool should_long() override {
        if (candle_manager.size() < 3) {
            logger->log_general("Pas assez d'historique (min 3 bougies)", LogLevel::WARNING);
            return false;
        }
        
        // Vérifier si on a besoin de Min/Max mais qu'on n'a pas assez d'historique
        if (base_config.use_minmax_for_sl && candle_manager.size() < static_cast<size_t>(base_config.sl_minmax_periods)) {
            logger->log_general("Pas assez d'historique pour le calcul Min/Max SL", LogLevel::WARNING);
            return false;
        }
    
        // Only check if current candle is green
        return candle_manager.is_latest_heikin_ashi_green();
    }

    void go_long() override {
        logger->log_general("Préparation d'un signal LONG", LogLevel::INFO);

        // Calcul du Stop Loss
        stop_loss_distance = PositionManager::calculateStopLoss(
            base_config, 
            price(), 
            current_atrlog, 
            true,  // is_long = true 
            candle_manager, 
            candle_manager.get_latest_candle(), 
            logger
        );
        
        // Calcul du Take Profit
        take_profit_distance = PositionManager::calculateTakeProfit(
            base_config,
            price(),
            current_atrlog,
            stop_loss_distance,
            candle_manager,
            logger
        );

        // Calcul de la taille de position
        buy_quantity = PositionManager::calculatePositionSize(
            base_config,
            price(),
            stop_loss_distance,
            logger
        );
        
        buy_price = price();
    }
    
    void go_short() override {
        throw std::runtime_error("BuyHeikinGreen strategy does not support short selling");
    }
    
    std::vector<std::function<bool()>> filters() override {
        return active_filters;
    }

public:
    BuyHeikinGreen(const StrategyBaseConfig& base_cfg, const BuyHeikinGreenConfig& bhg_cfg) 
        : Strategy(base_cfg), config(bhg_cfg) {
        
        // Initialize indicator calculators
        ema_short_calculator = std::make_unique<EMA>(config.ema_short_period);
        ema_long_calculator = std::make_unique<EMA>(config.ema_long_period);
        stochastic_calculator = std::make_unique<STOCH>(
            config.stoch_fastk,
            config.stoch_slowk,
            config.stoch_slowd
        );
        rsi_calculator = std::make_unique<RSI>(config.rsi_period);
        atrlog_calculator = std::make_unique<ATRLOG>(base_cfg.atr_period);
        
        // Create separate SuperTrend calculators for filter and TP functionality
        if (config.use_supertrend_filter) {
            supertrend_filter_calculator = std::make_unique<SUPERTREND>(
                config.supertrend_atr_period,
                config.supertrend_multiplier
            );
            supertrend_filter_name = "SUPERTREND_FILTER_" + logger->fast_int_to_string(config.supertrend_atr_period) + "_" +
                                   logger->fast_double_to_string(config.supertrend_multiplier);
        }
        
        if (base_cfg.use_supertrend_for_tp) {
            supertrend_tp_calculator = std::make_unique<SUPERTREND>(
                base_cfg.tp_supertrend_atr_period,
                base_cfg.tp_supertrend_multiplier
            );
            supertrend_tp_name = "SUPERTREND_TP_" + logger->fast_int_to_string(base_cfg.tp_supertrend_atr_period) + "_" +
                               logger->fast_double_to_string(base_cfg.tp_supertrend_multiplier);
        }
        
        // Initialize indicator names
        ema_short_name = "EMA_" + logger->fast_int_to_string(config.ema_short_period);
        ema_long_name = "EMA_" + logger->fast_int_to_string(config.ema_long_period);
        stoch_k_name = "STOCH_K_" + logger->fast_int_to_string(config.stoch_fastk) + "_" +
                      logger->fast_int_to_string(config.stoch_slowk) + "_" +
                      logger->fast_int_to_string(config.stoch_slowd);
        stoch_d_name = "STOCH_D_" + logger->fast_int_to_string(config.stoch_fastk) + "_" +
                      logger->fast_int_to_string(config.stoch_slowk) + "_" +
                      logger->fast_int_to_string(config.stoch_slowd);
        rsi_name = "RSI_" + logger->fast_int_to_string(config.rsi_period);
        atrlog_name = "ATRLOG_" + logger->fast_int_to_string(base_cfg.atr_period);

        // Setup active filters
        if (config.use_ema_short_filter) {
            active_filters.push_back([this]() {
                return Filters::priceSupEMA(price(), current_ema_short, ema_short_name, logger.get());
            });
        }
        if (config.use_ema_long_filter) {
            active_filters.push_back([this]() {
                return Filters::priceSupEMA(price(), current_ema_long, ema_long_name, logger.get());
            });
        }
        if (config.use_stoch_filter) {
            active_filters.push_back([this]() {
                return Filters::stochInfThreshold(current_stoch_k, k_previous, k_previous_2, k_previous_3, 
                                                config.stoch_threshold, "Stochastique", logger.get());
            });
        }

        if (config.use_rsi_filter) {
            active_filters.push_back([this]() {
                return Filters::rsiInfThreshold(current_rsi, previous_rsi, previous_2_rsi, 
                                            config.rsi_threshold, rsi_name, logger.get());
            });
        }

        if (config.use_previous_ha_candle_red_filter) {
            active_filters.push_back([this]() {
                return Filters::previousHACandlesRed(candle_manager, config.previous_ha_candle_red_filter_n, 
                                                "Bougie HA précédente", logger.get());
            });
        }

        if (config.use_supertrend_filter) {
            active_filters.push_back([this]() {
                return Filters::priceSupSupertrend(price(), current_supertrend_filter, 
                                                current_supertrend_filter_direction, 
                                                "Supertrend Filter", logger.get());
            });
        }
    }
};