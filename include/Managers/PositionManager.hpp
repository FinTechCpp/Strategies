#pragma once
#include "common.h"
#include "CandleManager.hpp"
#include "Managers/LoggerManager.hpp"
#include "MachineLearning/InferenceModel.hpp"
#include "strategy.h"
#include <cmath>
#include <algorithm>
#include <memory>
#include <vector>

class PositionManager {
public:
    // Stop Loss calculation 
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
            // Use fixed value for SL
            if (logger) logger->log_general("Using fixed value for SL: " + 
                std::to_string(config.stop_loss_distance), LogLevel::INFO);
            return config.stop_loss_distance;
        }
    }
    
    static double calculateTakeProfit(
        const StrategyBaseConfig& config,
        double current_price,
        double current_atr,
        double stop_loss_distance,
        const CandleManager& candle_manager,
        const std::unique_ptr<ILogger>& logger
    ) {
        if (config.use_supertrend_for_tp) {
            // SuperTrend TP: no fixed TP at open, exit based on trend reversal
            if (logger) logger->log_general("Using SuperTrend for TP - no fixed distance", LogLevel::INFO);
            return 0.0;  // No fixed TP
        } else if (config.use_rl_for_tp) {
            // RL-based TP calculation
            return calculateTakeProfitWithRL(config, current_price, current_atr, 
                                            stop_loss_distance, candle_manager, logger);
        } else if (config.use_atr_for_tp && current_atr > 0.0) {
            return calculateTakeProfitWithATR(config, current_atr, logger);
        } else if (config.use_sl_ratio_for_tp && stop_loss_distance > 0.0) {
            return calculateTakeProfitWithSLRatio(config, stop_loss_distance, logger);
        } else {
            // Use fixed value for TP
            if (logger) logger->log_general("Using fixed value for TP: " + 
                std::to_string(config.take_profit_distance), LogLevel::INFO);
            return config.take_profit_distance;
        }
    }

    // Position size calculation based on risk
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
            if (logger) logger->log_position_sizing(1.0, 1.0, "fixed size", LogLevel::INFO);
            return 1.0;
        }
    }

private:
    // Stop Loss calculation based on ATR
    static double calculateStopLossWithATR(
        const StrategyBaseConfig& config,
        double current_atr,
        const std::unique_ptr<ILogger>& logger
    ) {
        if (logger) logger->log_general("Using ATR to calculate SL", LogLevel::INFO);

        // Check ATR
        double atr_to_use = current_atr;
        if (atr_to_use <= 0.0) {
            atr_to_use = config.min_stop_loss_distance / config.stop_loss_atr_multiplier;
            if (logger) logger->log_general("Invalid ATR, using fallback value: " + 
                std::to_string(atr_to_use), LogLevel::WARNING);
        }

        // Stop Loss calculation based on ATR with minimum
        double stop_loss_distance = std::max(
            atr_to_use * config.stop_loss_atr_multiplier,
            config.min_stop_loss_distance
        );

        if (logger) logger->log_general("SL calculated with ATR: " + std::to_string(stop_loss_distance), LogLevel::INFO);
        return stop_loss_distance;
    }

    // Stop Loss calculation based on Min/Max
    static double calculateStopLossWithMinMax(
        const StrategyBaseConfig& config,
        double current_price,
        const CandleManager& candle_manager,
        const BasicCandle& basic_candle,
        bool is_long,
        const std::unique_ptr<ILogger>& logger
    ) {
        if (logger) logger->log_general("Using Min/Max to calculate SL " + 
            std::string(is_long ? "(LONG)" : "(SHORT)"), LogLevel::INFO);
        
        int n_periods = std::min(static_cast<int>(candle_manager.size()), config.sl_minmax_periods);
        auto recent_candles = candle_manager.get_last_candles(n_periods);
        
        if (is_long) {
            // For LONG: find the minimum
            double min_price = basic_candle.low;
            
            for (const auto& candle : recent_candles) 
                min_price = std::min(min_price, candle.low);
            
            if (logger) logger->log_general("Minimum price found: " + std::to_string(min_price), LogLevel::INFO);

            // SL = minimum - delta (for LONG, the SL is below the minimum)
            double sl_price = min_price - config.sl_minmax_delta;
            double stop_loss_distance = current_price - sl_price;

            stop_loss_distance = std::max(stop_loss_distance, config.min_stop_loss_distance);
            
            // Ensure minimum distance
            if (stop_loss_distance <= 0.0 || sl_price >= current_price) {
                if (logger) logger->log_general("SL Min/Max calculated invalid, using fixed distance", LogLevel::WARNING);
                return config.stop_loss_distance;
            }

            if (logger) logger->log_general("SL Min/Max calculated: " + std::to_string(stop_loss_distance) + 
                " (min=" + std::to_string(min_price) + 
                ", delta=" + std::to_string(config.sl_minmax_delta) + 
                ", SL price=" + std::to_string(sl_price) + 
                ", Min SL=" + std::to_string(config.min_stop_loss_distance) + ")", LogLevel::INFO);
                
            return stop_loss_distance;
        } 
        else {
            // For SHORT: find the maximum
            double max_price = basic_candle.high;
            
            for (const auto& candle : recent_candles) 
                max_price = std::max(max_price, candle.high);
            
            if (logger) logger->log_general("Maximum price found: " + std::to_string(max_price), LogLevel::INFO);

            // SL = maximum + delta (for SHORT, the SL is above the maximum)
            double sl_price = max_price + config.sl_minmax_delta;
            double stop_loss_distance = sl_price - current_price;

            stop_loss_distance = std::max(stop_loss_distance, config.min_stop_loss_distance);
            
            // Ensure minimum distance
            if (stop_loss_distance <= 0.0 || sl_price <= current_price) {
                if (logger) logger->log_general("SL Min/Max calculated invalid, using fixed distance", LogLevel::WARNING);
                return config.stop_loss_distance;
            }

            if (logger) logger->log_general("SL Min/Max calculated: " + std::to_string(stop_loss_distance) + 
                " (max=" + std::to_string(max_price) + 
                ", delta=" + std::to_string(config.sl_minmax_delta) + 
                ", SL price=" + std::to_string(sl_price) + 
                ", Min SL=" + std::to_string(config.min_stop_loss_distance) + ")", LogLevel::INFO);
                
            return stop_loss_distance;
        }
    }

    
    // TP calculation based on ATR
    static double calculateTakeProfitWithATR(
        const StrategyBaseConfig& config,
        double current_atr,
        const std::unique_ptr<ILogger>& logger
    ) {
        if (logger) logger->log_general("Using ATR to calculate TP", LogLevel::INFO);

        // ATR check
        double atr_to_use = current_atr;
        if (atr_to_use <= 0.0) {
            atr_to_use = config.min_take_profit_distance / config.take_profit_atr_multiplier;
            if (logger) logger->log_general("Invalid ATR, using fallback value: " + 
                std::to_string(atr_to_use), LogLevel::WARNING);
        }
        
        // Calcul TP basé sur ATR avec minimum
        double take_profit_distance = std::max(
            atr_to_use * config.take_profit_atr_multiplier,
            config.min_take_profit_distance
        );
        
        if (logger) logger->log_general("TP calculated with ATR: " + std::to_string(take_profit_distance), LogLevel::INFO);
        return take_profit_distance;
    }

    // TP calculation based on SL ratio
    static double calculateTakeProfitWithSLRatio(
        const StrategyBaseConfig& config,
        double stop_loss_distance,
        const std::unique_ptr<ILogger>& logger
    ) {
        if (logger) logger->log_general("Using SL ratio to calculate TP", LogLevel::INFO);
        
        // TP calculation based on SL ratio with minimum
        double take_profit_distance = std::max(
            stop_loss_distance * config.tp_sl_ratio,
            config.min_take_profit_distance
        );
        
        if (logger) logger->log_general("TP calculated with SL ratio: " + std::to_string(take_profit_distance) + 
        " (SL=" + std::to_string(stop_loss_distance) + 
        ", ratio=" + std::to_string(config.tp_sl_ratio) + ")", LogLevel::INFO);
        return take_profit_distance;
    }
    
    // Static cache for the ML model to avoid reloading
    static std::unique_ptr<InferenceModel> rl_model;
    
    // Generic ML-based take profit calculation
    // The model receives market features and outputs a take profit distance directly
    // Feature engineering and model interpretation is delegated to the trained model
    static double calculateTakeProfitWithRL(
        const StrategyBaseConfig& config,
        double current_price,
        double current_atr,
        double stop_loss_distance,
        const CandleManager& candle_manager,
        const std::unique_ptr<ILogger>& logger
    );

    // Position size calculation based on risk
    static double calculateRiskBasedPositionSize(
        const StrategyBaseConfig& config,
        double current_price,
        double stop_loss_distance,
        const std::unique_ptr<ILogger>& logger
    ) {
        double initial_capital = config.cash;
        double risk_percentage = config.risk_percentage;
        double risk_amount = initial_capital * risk_percentage / 100.0;

        logger->log_risk_calculation(risk_amount, risk_percentage);
        
        // Calculate position size for SL to represent exactly risk_amount
        double risk_based_position_size = risk_amount / stop_loss_distance;

        logger->log_position_sizing(risk_based_position_size, risk_based_position_size, 
            "based on risk", LogLevel::DEBUG);
        
        // Use total available capital with leverage
        double leveraged_capital = initial_capital * config.leverage_limit;
        
        // Limit max position size to a percentage of capital with leverage
        double max_position_value = leveraged_capital;
        double max_position_size = max_position_value / current_price;

        logger->log_position_sizing(max_position_size, max_position_size, 
            "maximum limit", LogLevel::DEBUG);
        
        // Take the MINIMUM between risk-based size and limit
        double raw_position_size = std::min(risk_based_position_size, max_position_size);

        if (risk_based_position_size > max_position_size) {
            logger->log_general("Risk-based position size (" + 
                std::to_string(risk_based_position_size) + 
                ") exceeds maximum limit (" + 
                std::to_string(max_position_size) + 
                "), adjusting to limit", LogLevel::WARNING);
        }
        
        // Round to lower 0.5
        double final_position_size = std::floor(raw_position_size * 2.0) / 2.0;
        logger->log_position_sizing(raw_position_size, final_position_size, "rounded down to 0.5", LogLevel::INFO);
        
        return final_position_size;
    }
};