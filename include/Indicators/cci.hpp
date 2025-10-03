#pragma once
#include "IncrementalIndicator.hpp"
#include <vector>
#include <deque>
#include <numeric>
#include <cmath>

/**
 * Commodity Channel Index (CCI) calculated incrementally
 */
class CCI : public IncrementalIndicator<double> {
private:
    int period;
    double current_cci = 0.0;
    double current_sma = 0.0;
    double current_md = 0.0;
    
    std::deque<double> tp_history;  // Typical Price history buffer
    
    static constexpr double CCI_CONSTANT = 0.015;
    
    // Calculate typical price from a candle
    inline double calculate_typical_price(const BasicCandle& candle) const {
        return (candle.high + candle.low + candle.close) / 3.0;
    }
    
public:
    CCI(int period)
        : IncrementalIndicator<double>("CCI_" + std::to_string(period), period), 
          period(period) {}
    
    double initialize_with_history(const std::vector<BasicCandle>& history) override;
    double update(const BasicCandle& candle) override;
    double get_value() const override;
};

inline double CCI::initialize_with_history(const std::vector<BasicCandle>& history) {
    if (history.size() < static_cast<size_t>(period)) {
        return 0.0;
    }
    
    // Calculate typical prices for all history
    std::vector<double> typical_prices;
    typical_prices.reserve(history.size());
    
    for (const auto& candle : history) {
        typical_prices.push_back(calculate_typical_price(candle));
    }
    
    // Store the last 'period' typical prices in buffer
    tp_history.clear();
    for (size_t i = typical_prices.size() - period; i < typical_prices.size(); ++i) {
        tp_history.push_back(typical_prices[i]);
    }
    
    // Calculate initial SMA (Simple Moving Average of TP)
    double sum = std::accumulate(tp_history.begin(), tp_history.end(), 0.0);
    current_sma = sum / period;
    
    // Calculate initial MD (Mean Deviation)
    double deviation_sum = 0.0;
    for (const auto& tp : tp_history) {
        deviation_sum += std::abs(tp - current_sma);
    }
    current_md = deviation_sum / period;
    
    // Calculate CCI
    double current_tp = tp_history.back();
    if (current_md > 0.0) {
        current_cci = (current_tp - current_sma) / (CCI_CONSTANT * current_md);
    } else {
        current_cci = 0.0;
    }
    
    is_initialized = true;
    return current_cci;
}

inline double CCI::update(const BasicCandle& candle) {
    double new_tp = calculate_typical_price(candle);
    
    // Add new typical price to buffer
    tp_history.push_back(new_tp);
    
    if (!is_initialized) {
        // Check if we have enough data to initialize
        if (tp_history.size() >= static_cast<size_t>(period)) {
            // Calculate SMA
            double sum = std::accumulate(tp_history.begin(), tp_history.end(), 0.0);
            current_sma = sum / period;
            
            // Calculate MD
            double deviation_sum = 0.0;
            for (const auto& tp : tp_history) 
                deviation_sum += std::abs(tp - current_sma);
            
            current_md = deviation_sum / period;
            
            // Calculate CCI
            if (current_md > 0.0) 
                current_cci = (new_tp - current_sma) / (CCI_CONSTANT * current_md);
            else 
                current_cci = 0.0;
            
            is_initialized = true;
            
            // Limit buffer size
            while (tp_history.size() > static_cast<size_t>(period)) 
                tp_history.pop_front();
            
            return current_cci;
        }
        
        return 0.0; // Not enough data yet
    }
    
    // Incremental calculation
    // Get the oldest value BEFORE removing it
    double old_tp = tp_history.front();
    
    // Remove oldest value if buffer is full
    if (tp_history.size() > static_cast<size_t>(period)) 
        tp_history.pop_front();
    
    // Update SMA incrementally: SMA_t = SMA_{t-1} + (TP_t - TP_{t-n}) / n
    current_sma = current_sma + (new_tp - old_tp) / period;
    
    // Calculate MD properly with current SMA
    // For true incremental calculation, we would need to track:
    // MD = Σ|TP[i] - SMA| / n
    // When SMA changes, all deviations change, so we recalculate
    // This is O(n) but for typical periods (14-20), it's still very fast
    double deviation_sum = 0.0;
    for (const auto& tp : tp_history) 
        deviation_sum += std::abs(tp - current_sma); 
    current_md = deviation_sum / period;
    
    // Calculate CCI: CCI_t = (TP_t - SMA_t) / (0.015 * MD_t)
    if (current_md > 0.0) 
        current_cci = (new_tp - current_sma) / (CCI_CONSTANT * current_md);
     else 
        current_cci = 0.0;
    
    return current_cci;
}

inline double CCI::get_value() const {
    return current_cci;
}
