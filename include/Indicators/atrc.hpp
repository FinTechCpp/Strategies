#pragma once
#include "IncrementalIndicator.hpp"
#include <vector>
#include <deque>
#include <algorithm>
#include <cmath>

/**
 * Average True Range (ATR) calculated incrementally
 */
class ATRC : public IncrementalIndicator<double> {
private:
    int period;
    double current_atr = 0.0;
    std::deque<double> true_range_history;
    
public:
    ATRC(int period)
        : IncrementalIndicator<double>("ATRC_" + std::to_string(period), period * 2), period(period) {}
    std::optional<double> initialize_with_history(const std::vector<BasicCandle>& history) override;
    std::optional<double> update(const BasicCandle& candle) override;
    std::optional<double> get_value() const override;
};

inline std::optional<double> ATRC::initialize_with_history(const std::vector<BasicCandle>& history)
{
    if (history.size() < static_cast<size_t>(period + 1)) {
        return std::nullopt;
    }
    
    std::vector<double> high_history;
    std::vector<double> low_history;
    
    for (const auto& candle : history) {
        high_history.push_back(candle.high);
        low_history.push_back(candle.low);
    }
    
    // Calculate True Range (TR) for the history
    std::vector<double> true_ranges;
    for (size_t i = 1; i < high_history.size(); ++i) {
        true_ranges.push_back(high_history[i] - low_history[i]);
    }
    
    // Calculate initial ATR as simple average of TR
    double sum = 0.0;
    for (size_t i = true_ranges.size() - period; i < true_ranges.size(); ++i) {
        sum += true_ranges[i];
    }
    
    current_atr = sum / period;
    is_initialized = true;
    
    return current_atr;
} 

inline std::optional<double> ATRC::update(const BasicCandle& candle)
{
    if (!is_initialized) {
        double tr = candle.high - candle.low;
        
        true_range_history.push_back(tr);
        
        if (true_range_history.size() < static_cast<size_t>(period))
            return std::nullopt;

        double sum = 0.0;
        for (const auto& tr : true_range_history) {
            sum += tr;
        }
        
        current_atr = sum / period;
        is_initialized = true;

        return current_atr;
    }
    
    // Calculate new True Range
    double tr = candle.high - candle.low;
    
    // Update ATR using Wilder's smoothing method
    current_atr = ((current_atr * (period - 1)) + tr) / period;
    
    return current_atr;
}

inline std::optional<double> ATRC::get_value() const {
    return current_atr;
}