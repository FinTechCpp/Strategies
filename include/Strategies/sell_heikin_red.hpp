#pragma once
#include "strategy.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <functional>

struct SellHeikinRedConfig {
    int ema_short_period = 150;
    int ema_long_period = 198;
    int stoch_fastk = 10;
    int stoch_slowk = 7;
    int stoch_slowd = 3;
    int stoch_threshold = 80;  // Inversé par rapport à BuyHeikinGreen
    
    bool use_ema_short_filter = true;
    bool use_ema_long_filter = true;
    bool use_stoch_filter = true;
    bool use_previous_ha_candle_green_filter = true;  // Inversé
};

class SellHeikinRed : public Strategy {
private:
    SellHeikinRedConfig config;
    
    // Indicator calculators
    std::unique_ptr<EMA> ema_short_calculator;
    std::unique_ptr<EMA> ema_long_calculator;
    std::unique_ptr<STOCH> stochastic_calculator;
    std::unique_ptr<ATR> atr_calculator;
    
    // Indicator names
    std::string ema_short_name;
    std::string ema_long_name;
    std::string stoch_k_name;
    std::string stoch_d_name;
    std::string atr_name;
    
    // Indicator values
    double current_ema_short = 0.0;
    double current_ema_long = 0.0;
    double current_stoch_k = 0.0;
    double current_stoch_d = 0.0;
    double current_atr = 0.0;
    double k_previous = 0.0;
    double d_previous = 0.0;
        
    // Filters - inversés par rapport à BuyHeikinGreen
    std::vector<std::function<bool()>> active_filters;
    
    bool ema_short_filter() {
        if (current_ema_short == 0.0) {
            logger->log_filter_result("EMA Court", false);
            logger->log_filter_detail("EMA Court", "Valeur EMA non calculée (0.0)", LogLevel::DEBUG);
            return false;
        }
        bool result = price() < current_ema_short;  // Inversé: < au lieu de >
        logger->log_filter_result("EMA Court", result);
        logger->log_indicator_comparison(ema_short_name, price(), current_ema_short, 
                                       result ? "<" : ">=", result, LogLevel::DEBUG);
        return result;
    }
    
    bool ema_long_filter() {
        if (current_ema_long == 0.0) {
            logger->log_filter_result("EMA Long", false);
            logger->log_filter_detail("EMA Long", "Valeur EMA non calculée (0.0)", LogLevel::DEBUG);
            return false;
        }
        bool result = price() < current_ema_long;  // Inversé: < au lieu de >
        logger->log_filter_result("EMA Long", result);
        logger->log_indicator_comparison(ema_long_name, price(), current_ema_long, 
                                       result ? "<" : ">=", result, LogLevel::DEBUG);
        return result;
    }
    
    bool stoch_above_threshold_filter() {  // Inversé: above au lieu de inf
        if (current_stoch_k == 0.0) {
            logger->log_filter_result("Stochastique", false);
            logger->log_filter_detail("Stochastique", "Valeur stochastique non calculée (0.0)", LogLevel::DEBUG);
            return false;
        }
        
        int threshold = config.stoch_threshold;
        bool result = current_stoch_k > threshold || (k_previous > 0.0 && k_previous > threshold);
        
        logger->log_filter_result("Stochastique", result);
        logger->log_filter_detail("Stochastique", 
                               "K=" + std::to_string(current_stoch_k) + 
                               ", K_prev=" + std::to_string(k_previous) + 
                               ", Seuil=" + std::to_string(threshold), 
                               LogLevel::DEBUG);
        
        // Update previous values
        k_previous = current_stoch_k;
        d_previous = current_stoch_d;
        
        return result;
    }
    
    bool previous_ha_candle_green_filter() {  // Inversé: green au lieu de red
        try {
            if (candle_manager.size() < 3) {
                logger->log_filter_result("Bougie HA précédente", false);
                logger->log_filter_detail("Bougie HA précédente", "Pas assez d'historique (min 3 bougies)", LogLevel::DEBUG);
                return false;
            }
            
            // Récupérer les 2 dernières bougies HA (actuelle et précédente)
            auto ha_candles = candle_manager.get_last_heikin_ashi_candles(2);
            if (ha_candles.size() < 2) {
                logger->log_filter_result("Bougie HA précédente", false);
                logger->log_filter_detail("Bougie HA précédente", "Pas assez de bougies HA", LogLevel::DEBUG);
                return false;
            }
            
            // La bougie précédente est à l'index 0
            const BasicCandle& prev_ha = ha_candles[0];
            bool is_green = candle_manager.is_candle_green(prev_ha);
            
            logger->log_filter_result("Bougie HA précédente", is_green);
            logger->log_filter_detail("Bougie HA précédente", 
                                   "Bougie précédente " + std::string(is_green ? "VERTE" : "ROUGE") + 
                                   " (open=" + std::to_string(prev_ha.open) + 
                                   ", close=" + std::to_string(prev_ha.close) + ")", 
                                   LogLevel::DEBUG);
            
            return is_green;  // Nous cherchons une bougie verte précédente
        } catch (const std::exception& e) {
            logger->log_filter_result("Bougie HA précédente", false);
            logger->log_filter_detail("Bougie HA précédente", "Erreur: " + std::string(e.what()), LogLevel::ERROR);
            return false;
        }
    }
    
    bool initialize_indicators() {
        if (candle_manager.size() < static_cast<size_t>(std::max(config.ema_long_period, config.stoch_fastk + config.stoch_slowk))) {
            return false;
        }
        
        // Extract data for initialization from candle_manager
        auto candles = candle_manager.get_last_candles(candle_manager.size());
        
        // Initialize EMAs
        current_ema_short = ema_short_calculator->initialize_with_history(candles);
        current_ema_long = ema_long_calculator->initialize_with_history(candles);
        
        // Initialize Stochastic
        auto stoch_values = stochastic_calculator->initialize_with_history(candles);
        current_stoch_k = stoch_values.first;
        current_stoch_d = stoch_values.second;
        
        // Initialize ATR
        current_atr = atr_calculator->initialize_with_history(candles);
        
        return (current_ema_short > 0.0 && 
                current_ema_long > 0.0 && 
                current_stoch_k > 0.0 && 
                current_stoch_d > 0.0);
    }
    
    bool update_indicators() override {
        if (candle_manager.size() == 0) {
            logger->log_general("Aucune bougie disponible, impossible de mettre à jour les indicateurs", LogLevel::WARNING);
            return false;
        }
        
        if (!ema_short_calculator->initialized() ||
            !ema_long_calculator->initialized() ||
            !stochastic_calculator->initialized() ||
            !atr_calculator->initialized()) {

            logger->log_general("Initialisation des indicateurs requise", LogLevel::INFO);
            
            // Try to initialize indicators if they're not initialized
            if (!initialize_indicators()) {
                logger->log_general("Échec de l'initialisation des indicateurs", LogLevel::WARNING);
                return false;
            }
            
            logger->log_general("Indicateurs initialisés avec succès", LogLevel::INFO);
        }
        
        // Update EMAs
        current_ema_short = ema_short_calculator->update(candle_manager.get_latest_candle());
        current_ema_long = ema_long_calculator->update(candle_manager.get_latest_candle());

        // Update Stochastic
        auto stoch_values = stochastic_calculator->update(candle_manager.get_latest_candle());
        current_stoch_k = stoch_values.first;
        current_stoch_d = stoch_values.second;
        
        // Update ATR
        current_atr = atr_calculator->update(candle_manager.get_latest_candle());

        return true;
    }

public:
    SellHeikinRed(const StrategyBaseConfig& base_cfg, const SellHeikinRedConfig& shr_cfg) 
        : Strategy(base_cfg), config(shr_cfg) {
        
        // Initialize indicator calculators
        ema_short_calculator = std::make_unique<EMA>(config.ema_short_period);
        ema_long_calculator = std::make_unique<EMA>(config.ema_long_period);
        stochastic_calculator = std::make_unique<STOCH>(
            config.stoch_fastk,
            config.stoch_slowk,
            config.stoch_slowd
        );
        atr_calculator = std::make_unique<ATR>(base_cfg.atr_period);
        
        // Initialize indicator names
        ema_short_name = "EMA_" + std::to_string(config.ema_short_period);
        ema_long_name = "EMA_" + std::to_string(config.ema_long_period);
        stoch_k_name = "STOCH_K_" + std::to_string(config.stoch_fastk) + "_" +
                      std::to_string(config.stoch_slowk) + "_" +
                      std::to_string(config.stoch_slowd);
        stoch_d_name = "STOCH_D_" + std::to_string(config.stoch_fastk) + "_" +
                      std::to_string(config.stoch_slowk) + "_" +
                      std::to_string(config.stoch_slowd);
        atr_name = "ATR_" + std::to_string(base_cfg.atr_period);
        
        // Setup active filters
        if (config.use_ema_short_filter) {
            active_filters.push_back([this]() { return this->ema_short_filter(); });
        }
        if (config.use_ema_long_filter) {
            active_filters.push_back([this]() { return this->ema_long_filter(); });
        }
        if (config.use_stoch_filter) {
            active_filters.push_back([this]() { return this->stoch_above_threshold_filter(); });
        }
        if (config.use_previous_ha_candle_green_filter) {
            active_filters.push_back([this]() { return this->previous_ha_candle_green_filter(); });
        }
    }

    void before() override {
        // Update all technical indicators
        update_indicators();
        
        // Obtenir la dernière bougie HA pour journalisation
        try {
            BasicCandle ha_current = candle_manager.get_latest_heikin_ashi();
            bool is_green = candle_manager.is_candle_green(ha_current);
            
            logger->log_general("Bougie HA courante calculée: Open=" + std::to_string(ha_current.open) + 
                            ", Close=" + std::to_string(ha_current.close) + 
                            ", " + (is_green ? "VERTE" : "ROUGE"), LogLevel::INFO);
        } catch (const std::exception& e) {
            logger->log_general("Erreur lors de la récupération des bougies HA: " + std::string(e.what()), LogLevel::ERROR);
        }
    }
    
    bool should_long() override {
        return false;  // Cette stratégie ne prend pas de positions longues
    }
    
    bool should_short() override {  // Implémente should_short au lieu de should_long
        if (candle_manager.size() < 3) {
            logger->log_general("Pas assez d'historique (min 3 bougies)", LogLevel::WARNING);
            return false;
        }
        
        // Vérifier si on a besoin de Min/Max mais qu'on n'a pas assez d'historique
        if (base_config.use_minmax_for_sl && candle_manager.size() < static_cast<size_t>(base_config.sl_minmax_periods)) {
            logger->log_general("Pas assez d'historique pour le calcul Min/Max SL", LogLevel::WARNING);
            return false;
        }
        
        try {
            // Vérifier si la bougie actuelle est rouge (pas verte)
            BasicCandle ha_current = candle_manager.get_latest_heikin_ashi();
            bool is_green = candle_manager.is_candle_green(ha_current);
            
            logger->log_general("Bougie HA actuelle " + std::string(is_green ? "VERTE" : "ROUGE"), LogLevel::INFO);
            
            return !is_green;  // Signal de vente si bougie ROUGE
        } catch (const std::exception& e) {
            logger->log_general("Erreur lors de la vérification de la bougie HA: " + std::string(e.what()), LogLevel::ERROR);
            return false;
        }
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
            current_atr, 
            false,  // is_long = false (SHORT)
            candle_manager, 
            candle_manager.get_latest_candle(), 
            logger
        );
        
        // Calcul du Take Profit
        take_profit_distance = PositionManager::calculateTakeProfit(
            base_config,
            current_atr,
            stop_loss_distance,
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
        logger->log_sl_tp(stop_loss_distance, take_profit_distance);
    }
    
    // void go_short() override {
    //     logger->log_general("Préparation d'un signal SHORT", LogLevel::INFO);

    //     // Calcul du Stop Loss
    //     if (base_config.use_atr_for_sl && current_atr > 0.0) {
    //         logger->log_general("Utilisation de l'ATR pour calculer SL", LogLevel::INFO);

    //         // Vérification ATR
    //         if (current_atr <= 0.0) {
    //             current_atr = base_config.min_stop_loss_distance / base_config.stop_loss_atr_multiplier;
    //             logger->log_general("ATR non valide, utilisation d'une valeur de secours: " + 
    //                 std::to_string(current_atr), LogLevel::WARNING);
    //         }

    //         // Transformation logarithmique pour SL
    //         double log_atr = std::log(1.0 + current_atr);
            
    //         // Calcul SL basé sur ATR avec minimum
    //         stop_loss_distance = std::max(
    //             log_atr * base_config.stop_loss_atr_multiplier,
    //             base_config.min_stop_loss_distance
    //         );
            
    //         logger->log_general("SL calculé avec ATR: " + std::to_string(stop_loss_distance), LogLevel::INFO);
    //     } 
    //     else if (base_config.use_minmax_for_sl && candle_manager.size() >= base_config.sl_minmax_periods) {
    //         logger->log_general("Utilisation de Min/Max pour calculer SL (SHORT)", LogLevel::INFO);
            
    //         // Recherche du maximum sur les n dernières périodes
    //         double max_price = current_candle.high;
    //         int n_periods = std::min(static_cast<int>(candle_manager.size()), base_config.sl_minmax_periods);
            
    //         // Récupérer les n dernières bougies
    //         auto recent_candles = candle_manager.get_last_candles(n_periods);
            
    //         // Trouver le maximum
    //         for (const auto& candle : recent_candles) {
    //             max_price = std::max(max_price, candle.high);
    //         }
            
    //         logger->log_general("Prix maximum trouvé: " + std::to_string(max_price), LogLevel::INFO);
            
    //         // SL = maximum + delta (pour SHORT, le SL est au-dessus du maximum)
    //         double sl_price = max_price + base_config.sl_minmax_delta;
    //         stop_loss_distance = sl_price - price();
            
    //         // Assurer une distance minimale
    //         if (stop_loss_distance <= 0.0 || sl_price <= price()) {
    //             logger->log_general("SL Min/Max calculé invalide, utilisation de distance fixe", LogLevel::WARNING);
    //             stop_loss_distance = base_config.stop_loss_distance;
    //         }
            
    //         logger->log_general("SL Min/Max calculé: " + std::to_string(stop_loss_distance) + 
    //                           " (max=" + std::to_string(max_price) + 
    //                           ", delta=" + std::to_string(base_config.sl_minmax_delta) + 
    //                           ", prix SL=" + std::to_string(sl_price) + ")", 
    //                           LogLevel::INFO);
    //     } 
    //     else {
    //         // Utiliser valeur fixe pour SL
    //         stop_loss_distance = base_config.stop_loss_distance;
    //         logger->log_general("Utilisation de valeur fixe pour SL: " + 
    //             std::to_string(stop_loss_distance), LogLevel::INFO);
    //     }
        
    //     // Calcul du Take Profit
    //     if (base_config.use_atr_for_tp && current_atr > 0.0) {
    //         logger->log_general("Utilisation de l'ATR pour calculer TP", LogLevel::INFO);

    //         // Vérification ATR (uniquement si pas déjà fait)
    //         if (current_atr <= 0.0) {
    //             current_atr = base_config.min_take_profit_distance / base_config.take_profit_atr_multiplier;
    //             logger->log_general("ATR non valide, utilisation d'une valeur de secours: " + 
    //                 std::to_string(current_atr), LogLevel::WARNING);
    //         }

    //         // Transformation logarithmique pour TP
    //         double log_atr = std::log(1.0 + current_atr);
            
    //         // Calcul TP basé sur ATR avec minimum
    //         take_profit_distance = std::max(
    //             log_atr * base_config.take_profit_atr_multiplier,
    //             base_config.min_take_profit_distance
    //         );
            
    //         logger->log_general("TP calculé avec ATR: " + std::to_string(take_profit_distance), LogLevel::INFO);
    //     } else {
    //         // Utiliser valeur fixe pour TP
    //         take_profit_distance = base_config.take_profit_distance;
    //         logger->log_general("Utilisation de valeur fixe pour TP: " + 
    //             std::to_string(take_profit_distance), LogLevel::INFO);
    //     }
        
    //     // Calculer la taille de position basée sur le risque si activé
    //     if (base_config.use_risk_based_sizing) {
    //         double initial_capital = base_config.cash;
    //         double risk_amount = initial_capital * base_config.risk_percentage / 100.0;
            
    //         // Calculer la taille de position pour que le SL représente exactement risk_amount
    //         double risk_based_position_size = risk_amount / stop_loss_distance;
            
    //         // Utiliser le capital total disponible avec effet de levier
    //         double leveraged_capital = initial_capital * base_config.leverage_limit;
            
    //         // Limiter la taille max de position à un pourcentage du capital avec levier
    //         double max_position_value = leveraged_capital * base_config.max_position_percentage / 100.0;
    //         double max_position_size = max_position_value / price();
            
    //         // Prendre le MINIMUM entre la taille basée sur le risque et la limite
    //         double raw_position_size = std::min(risk_based_position_size, max_position_size);
            
    //         // Si taille >= 1, arrondir à l'entier le plus proche
    //         if (raw_position_size >= 1.0) {
    //             sell_quantity = std::floor(raw_position_size);
    //         } else {
    //             // Limiter au minimum de 0.5
    //             sell_quantity = std::max(0.5, std::min(raw_position_size, 0.99));
    //         }
    //     } else {
    //         // Taille fixe par défaut (entier)
    //         sell_quantity = 1.0;
    //     }
        
    //     sell_price = price();
    // }
    
    std::vector<std::function<bool()>> filters() override {
        return active_filters;
    }
};