#include "Managers/PositionManager.hpp"

// Define the static member
std::unique_ptr<InferenceModel> PositionManager::rl_model = nullptr;

double PositionManager::calculateTakeProfitWithRL(
        const StrategyBaseConfig& config,
        double current_price,
        double current_atr,
        double stop_loss_distance,
        const CandleManager& candle_manager,
        const std::unique_ptr<ILogger>& logger
    ) {
        if (logger) logger->log_general("Utilisation du modèle RL pour calculer TP", LogLevel::INFO);
    
        // Load model if not already loaded
        if (!rl_model) {
            rl_model = std::make_unique<InferenceModel>();
            bool loaded = rl_model->load(config.rl_model_path);
            
            if (!loaded) {
                if (logger) logger->log_general("Échec de chargement du modèle RL : " + config.rl_model_path + ", utilisation du Ratio SL : ", LogLevel::ERROR);
                return PositionManager::calculateTakeProfitWithSLRatio(config, stop_loss_distance, logger);
            }
            
            if (logger) logger->log_general("Modèle RL chargé avec succès: " + config.rl_model_path, LogLevel::INFO);
        }
        
        // Prepare features for the model
        std::vector<float> features;
        
        // Essential market information
        features.push_back(static_cast<float>(current_atr));
        features.push_back(static_cast<float>(stop_loss_distance));
        
        // Price action data from recent candles
        auto recent_candles = candle_manager.get_last_candles(config.rl_lookback_periods);
        if (recent_candles.size() < 5) {
            if (logger) logger->log_general("Pas assez de bougies pour l'inférence RL, utilisation du ratio SL", LogLevel::WARNING);
            return PositionManager::calculateTakeProfitWithSLRatio(config, stop_loss_distance, logger);
        }
        
        // Add normalized price data
        for (const auto& candle : recent_candles) {
            // Normalize to percentage changes relative to current price
            features.push_back(static_cast<float>(candle.open / current_price - 1.0));
            features.push_back(static_cast<float>(candle.high / current_price - 1.0));
            features.push_back(static_cast<float>(candle.low / current_price - 1.0));
            features.push_back(static_cast<float>(candle.close / current_price - 1.0));
        }
        
        // Run inference
        float tp_multiplier = 0.0;
            tp_multiplier = rl_model->predict(features);
            
            // Apply safety constraints - ensure reasonable TP multiplier
            tp_multiplier = std::max(
                static_cast<float>(config.rl_tp_min_multiplier), 
                std::min(tp_multiplier, static_cast<float>(config.rl_tp_max_multiplier))
            );
            
            // Calculate final TP distance
            double take_profit_distance = stop_loss_distance * tp_multiplier;
            
            // Ensure minimum TP distance
            take_profit_distance = std::max(take_profit_distance, config.min_take_profit_distance);
            
            if (logger) logger->log_general("TP calculé avec RL: " + std::to_string(take_profit_distance) + 
                                         " (multiplicateur=" + std::to_string(tp_multiplier) + 
                                         ", SL=" + std::to_string(stop_loss_distance) + ")", LogLevel::INFO);
            return take_profit_distance;
        } 