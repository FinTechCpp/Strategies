#pragma once
#include "common.h"
#include "CandleManager.hpp"
#include "Managers/LoggerManager.hpp"
#include "strategy.h"
#include <cmath>
#include <algorithm>
#include <memory>
#include <vector>

class PositionManager {
public:
    // Calcul du Stop Loss avec différentes méthodes
    static double calculateStopLoss(
        const StrategyBaseConfig& config,
        double current_price,
        double current_atr,
        bool is_long,
        const CandleManager& candle_manager,
        const BasicCandle& basic_candle,
        const std::unique_ptr<ILogger>& logger
    ) {
        if (config.use_atr_for_sl && current_atr > 0.0) {
            return calculateStopLossWithATR(config, current_atr, logger);
        } 
        else if (config.use_minmax_for_sl && candle_manager.size() >= static_cast<size_t>(config.sl_minmax_periods)) {
            return calculateStopLossWithMinMax(
                config, current_price, candle_manager, basic_candle, is_long, logger);
        } 
        else {
            // Utiliser valeur fixe pour SL
            if (logger) logger->log_general("Utilisation de valeur fixe pour SL: " + 
                std::to_string(config.stop_loss_distance), LogLevel::INFO);
            return config.stop_loss_distance;
        }
    }
    
    // Calcul du Take Profit avec différentes méthodes  
    static double calculateTakeProfit(
        const StrategyBaseConfig& config,
        double current_atr,
        const std::unique_ptr<ILogger>& logger
    ) {
        if (config.use_atr_for_tp && current_atr > 0.0) {
            return calculateTakeProfitWithATR(config, current_atr, logger);
        } else {
            // Utiliser valeur fixe pour TP
            if (logger) logger->log_general("Utilisation de valeur fixe pour TP: " + 
                std::to_string(config.take_profit_distance), LogLevel::INFO);
            return config.take_profit_distance;
        }
    }
    
    // Calcul de la taille de position basée sur le risque
    static double calculatePositionSize(
        const StrategyBaseConfig& config,
        double current_price,
        double stop_loss_distance,
        const std::unique_ptr<ILogger>& logger
    ) {
        if (config.use_risk_based_sizing) {
            return calculateRiskBasedPositionSize(config, current_price, stop_loss_distance, logger);
        } else {
            // Fixed default size
            if (logger) logger->log_position_sizing(1.0, 1.0, "taille fixe", LogLevel::INFO);
            return 1.0;
        }
    }

private:
    // Calcul du SL basé sur ATR
    static double calculateStopLossWithATR(
        const StrategyBaseConfig& config,
        double current_atr,
        const std::unique_ptr<ILogger>& logger
    ) {
        if (logger) logger->log_general("Utilisation de l'ATR pour calculer SL", LogLevel::INFO);

        // Vérification ATR
        double atr_to_use = current_atr;
        if (atr_to_use <= 0.0) {
            atr_to_use = config.min_stop_loss_distance / config.stop_loss_atr_multiplier;
            if (logger) logger->log_general("ATR non valide, utilisation d'une valeur de secours: " + 
                std::to_string(atr_to_use), LogLevel::WARNING);
        }
        
        // Calcul SL basé sur ATR avec minimum
        double stop_loss_distance = std::max(
            atr_to_use * config.stop_loss_atr_multiplier,
            config.min_stop_loss_distance
        );
        
        if (logger) logger->log_general("SL calculé avec ATR: " + std::to_string(stop_loss_distance), LogLevel::INFO);
        return stop_loss_distance;
    }
    
    // Calcul du SL basé sur Min/Max
    static double calculateStopLossWithMinMax(
        const StrategyBaseConfig& config,
        double current_price,
        const CandleManager& candle_manager,
        const BasicCandle& basic_candle,
        bool is_long,
        const std::unique_ptr<ILogger>& logger
    ) {
        if (logger) logger->log_general("Utilisation de Min/Max pour calculer SL " + 
            std::string(is_long ? "(LONG)" : "(SHORT)"), LogLevel::INFO);
        
        int n_periods = std::min(static_cast<int>(candle_manager.size()), config.sl_minmax_periods);
        auto recent_candles = candle_manager.get_last_candles(n_periods);
        
        if (is_long) {
            // Pour LONG: recherche du minimum 
            double min_price = basic_candle.low;
            
            for (const auto& candle : recent_candles) {
                min_price = std::min(min_price, candle.low);
            }
            
            if (logger) logger->log_general("Prix minimum trouvé: " + std::to_string(min_price), LogLevel::INFO);
            
            // SL = minimum - delta (pour LONG, le SL est sous le minimum)
            double sl_price = min_price - config.sl_minmax_delta;
            double stop_loss_distance = current_price - sl_price;
            
            // Assurer une distance minimale
            if (stop_loss_distance <= 0.0 || sl_price >= current_price) {
                if (logger) logger->log_general("SL Min/Max calculé invalide, utilisation de distance fixe", LogLevel::WARNING);
                return config.stop_loss_distance;
            }
            
            if (logger) logger->log_general("SL Min/Max calculé: " + std::to_string(stop_loss_distance) + 
                " (min=" + std::to_string(min_price) + 
                ", delta=" + std::to_string(config.sl_minmax_delta) + 
                ", prix SL=" + std::to_string(sl_price) + ")", LogLevel::INFO);
                
            return stop_loss_distance;
        } 
        else {
            // Pour SHORT: recherche du maximum
            double max_price = basic_candle.high;
            
            for (const auto& candle : recent_candles) {
                max_price = std::max(max_price, candle.high);
            }
            
            if (logger) logger->log_general("Prix maximum trouvé: " + std::to_string(max_price), LogLevel::INFO);
            
            // SL = maximum + delta (pour SHORT, le SL est au-dessus du maximum)
            double sl_price = max_price + config.sl_minmax_delta;
            double stop_loss_distance = sl_price - current_price;
            
            // Assurer une distance minimale
            if (stop_loss_distance <= 0.0 || sl_price <= current_price) {
                if (logger) logger->log_general("SL Min/Max calculé invalide, utilisation de distance fixe", LogLevel::WARNING);
                return config.stop_loss_distance;
            }
            
            if (logger) logger->log_general("SL Min/Max calculé: " + std::to_string(stop_loss_distance) + 
                " (max=" + std::to_string(max_price) + 
                ", delta=" + std::to_string(config.sl_minmax_delta) + 
                ", prix SL=" + std::to_string(sl_price) + ")", LogLevel::INFO);
                
            return stop_loss_distance;
        }
    }
    
    // Calcul du TP basé sur ATR
    static double calculateTakeProfitWithATR(
        const StrategyBaseConfig& config,
        double current_atr,
        const std::unique_ptr<ILogger>& logger
    ) {
        if (logger) logger->log_general("Utilisation de l'ATR pour calculer TP", LogLevel::INFO);

        // Vérification ATR
        double atr_to_use = current_atr;
        if (atr_to_use <= 0.0) {
            atr_to_use = config.min_take_profit_distance / config.take_profit_atr_multiplier;
            if (logger) logger->log_general("ATR non valide, utilisation d'une valeur de secours: " + 
                std::to_string(atr_to_use), LogLevel::WARNING);
        }
        
        // Calcul TP basé sur ATR avec minimum
        double take_profit_distance = std::max(
            atr_to_use * config.take_profit_atr_multiplier,
            config.min_take_profit_distance
        );
        
        if (logger) logger->log_general("TP calculé avec ATR: " + std::to_string(take_profit_distance), LogLevel::INFO);
        return take_profit_distance;
    }
    
    // Calcul de la taille de position basée sur le risque
    static double calculateRiskBasedPositionSize(
        const StrategyBaseConfig& config,
        double current_price,
        double stop_loss_distance,
        const std::unique_ptr<ILogger>& logger
    ) {
        double initial_capital = config.cash;
        double risk_percentage = config.risk_percentage;
        double risk_amount = initial_capital * risk_percentage / 100.0;

        if (logger) logger->log_risk_calculation(risk_amount, risk_percentage);
        
        // Calculate position size for SL to represent exactly risk_amount
        double risk_based_position_size = risk_amount / stop_loss_distance;

        if (logger) logger->log_position_sizing(risk_based_position_size, risk_based_position_size, 
            "basé sur le risque", LogLevel::DEBUG);
        
        // Use total available capital with leverage
        double leveraged_capital = initial_capital * config.leverage_limit;
        
        // Limit max position size to a percentage of capital with leverage
        double max_position_value = leveraged_capital * config.max_position_percentage / 100.0;
        double max_position_size = max_position_value / current_price;

        if (logger) logger->log_position_sizing(max_position_size, max_position_size, 
            "limite maximale", LogLevel::DEBUG);
        
        // Take the MINIMUM between risk-based size and limit
        double raw_position_size = std::min(risk_based_position_size, max_position_size);
        double final_position_size;
        
        // If size >= 1, round to lower 0,5 
        if (raw_position_size >= 1.0) {
            final_position_size = std::floor(raw_position_size * 2.0) / 2.0;
            if (logger) logger->log_position_sizing(raw_position_size, final_position_size, "arrondi au 0.5 inférieur", LogLevel::INFO);
        } 
        
        return final_position_size;
    }
};