#pragma once
#include "IncrementalIndicator.hpp"
#include <vector>
#include <deque>
#include <cmath>

/**
 * Relative Strength Index (RSI) calculated incrementally
 */
class RSI : public IncrementalIndicator<double> {
private:
    int period;
    double current_rsi = 0.0;
    double prev_close = 0.0;
    
    double avg_gain = 0.0;
    double avg_loss = 0.0;
    
    std::deque<double> close_history;
    bool first_avg_calculated = false;
    
public:
    RSI(filter::RSIParams params)
    : IncrementalIndicator<double>("RSI_" + std::to_string(params.period), params.period),
    period(params.period) {}

    std::optional<double> initialize_with_history(const std::vector<BasicCandle>& history) override;
    std::optional<double> update(const BasicCandle& candle) override;
    std::optional<double> get_value() const override;
};

inline std::optional<double> RSI::initialize_with_history(const std::vector<BasicCandle>& history) {
    if (history.size() < static_cast<size_t>(period + 1)) {
        return std::nullopt;
    }

    // Initialize close history
    std::vector<double> price_history;
    price_history.reserve(history.size());

    for (const auto& candle : history) {
        price_history.push_back(candle.close);
    }
    
    // Calculate first average gain and loss
    double total_gain = 0.0;
    double total_loss = 0.0;
    
    for (size_t i = 1; i < static_cast<size_t>(period) + 1; ++i) {
        double change = price_history[i] - price_history[i-1];
        if (change > 0) {
            total_gain += change;
        } else {
            total_loss -= change;  // Convert to positive value
        }
    }
    
    avg_gain = total_gain / period;
    avg_loss = total_loss / period;
    
    // Calculate initial RSI value
    if (avg_loss == 0.0) {
        current_rsi = 100.0;
    } else {
        double rs = avg_gain / avg_loss;
        current_rsi = 100.0 - (100.0 / (1.0 + rs));
    }
    
    // Set the previous close
    prev_close = price_history[period];
    first_avg_calculated = true;
    is_initialized = true;
    
    return current_rsi;
}

inline std::optional<double> RSI::update(const BasicCandle& candle) {
    double price = candle.close;

    if (!is_initialized)
        return std::nullopt; // Pas encore initialisé
    
    // Calculate current gain/loss
    double change = price - prev_close;
    double current_gain = (change > 0) ? change : 0.0;
    double current_loss = (change < 0) ? -change : 0.0;
    
    // Update averages using Wilder's smoothing method
    avg_gain = ((avg_gain * (period - 1)) + current_gain) / period;
    avg_loss = ((avg_loss * (period - 1)) + current_loss) / period;
    
    // Calculate RSI
    if (avg_loss == 0.0) {
        current_rsi = 100.0;
    } else {
        double rs = avg_gain / avg_loss;
        current_rsi = 100.0 - (100.0 / (1.0 + rs));
    }
    
    prev_close = price;
    return current_rsi;
}

inline std::optional<double> RSI::get_value() const {
    if (!is_initialized)
        return std::nullopt;
        
    return current_rsi;
}
