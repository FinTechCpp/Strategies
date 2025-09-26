#include "Managers/PositionManager.hpp"

// Define the static member
std::unique_ptr<InferenceModel> PositionManager::rl_model = nullptr;

double PositionManager::calculateTakeProfitWithRL(
        const StrategyConfig& config,
        double current_price,
        double current_atr,
        double stop_loss_distance,
        const CandleManager& candle_manager,
        ILogger* logger
    ) {
        if (logger) logger->log_general("Using ML model to calculate TP", LogLevel::INFO);

        // Load model if not already loaded
        if (!rl_model) {
            rl_model = std::make_unique<InferenceModel>();
            bool loaded = rl_model->load(config.rl_model_path);

            if (!loaded && logger) logger->log_general("Failed to load ML model: " + config.rl_model_path, LogLevel::ERROR);
            if (logger) logger->log_general("ML model loaded successfully: " + config.rl_model_path, LogLevel::INFO);
        }
        
        // Prepare generic feature vector for the model
        std::vector<float> features;
        
        // Add basic market context - the model can use these or ignore them
        features.push_back(static_cast<float>(current_price));
        features.push_back(static_cast<float>(current_atr));
        features.push_back(static_cast<float>(stop_loss_distance));
        
        // Add historical price data up to the configured lookback periods
        // The model determines how many candles it actually needs and how to use them
        if (config.rl_lookback_periods > 0) {
            auto recent_candles = candle_manager.get_last_candles(config.rl_lookback_periods);
            
            // Add available candle data (model can handle variable input length)
            for (const auto& candle : recent_candles) {
                features.push_back(static_cast<float>(candle.open));
                features.push_back(static_cast<float>(candle.high));
                features.push_back(static_cast<float>(candle.low));
                features.push_back(static_cast<float>(candle.close));
            }
            
            // If we have fewer candles than requested, pad with zeros (model's responsibility to handle)
            for (size_t i = recent_candles.size(); i < static_cast<size_t>(config.rl_lookback_periods); ++i) {
                features.push_back(0.0f); // open
                features.push_back(0.0f); // high
                features.push_back(0.0f); // low
                features.push_back(0.0f); // close
            }
        }
        
        // Log feature vector info for debugging
        if (logger) {
            logger->log_general("ML Features: Price=" + std::to_string(current_price) + 
                ", ATR=" + std::to_string(current_atr) + 
                ", SL=" + std::to_string(stop_loss_distance) + 
                ", Candles=" + std::to_string(std::min(static_cast<size_t>(config.rl_lookback_periods), candle_manager.size())) +
                ", Total features=" + std::to_string(features.size()), LogLevel::DEBUG);
        }
        
        // Run ML inference - model outputs take profit distance directly
        try {
            float predicted_tp_distance = rl_model->predict(features);
            
            // Apply safety constraints to ensure reasonable take profit distance
            double take_profit_distance = static_cast<double>(predicted_tp_distance);
            
            // Ensure the prediction is positive and within reasonable bounds
            if (take_profit_distance <= 0.0) {
                if (logger) logger->log_general("ML model predicted negative or zero TP distance (" + 
                    std::to_string(take_profit_distance), LogLevel::WARNING);
            }
            
            // Apply minimum distance constraint
            take_profit_distance = std::max(take_profit_distance, config.min_take_profit_distance);
            
            // Optional: Apply maximum distance constraint if configured
            if (config.rl_tp_max_multiplier > 0.0) {
                double max_tp_distance = stop_loss_distance * config.rl_tp_max_multiplier;
                take_profit_distance = std::min(take_profit_distance, max_tp_distance);
            }
            
            if (logger) logger->log_general("TP calculated with ML: " + std::to_string(take_profit_distance) + 
                " (raw prediction=" + std::to_string(predicted_tp_distance) + ")", LogLevel::INFO);
            return take_profit_distance;
            
        } catch (const std::exception& e) {
            if (logger) logger->log_general("Error during ML inference: " + std::string(e.what()), LogLevel::ERROR);

            return config.min_take_profit_distance; // Fallback to minimum distance
        } 
    }