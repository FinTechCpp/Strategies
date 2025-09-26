#pragma once
#include "IncrementalIndicator.hpp"
#include <vector>
#include <deque>
#include <algorithm>
#include <cmath>

/**
 * Average True Range (ATR) calculated incrementally
 */
class ATR : public IncrementalIndicator<double> {
private:
    int period;
    bool useLog = false;
    double current_atr = 0.0;
    double previous_close = 0.0;
    std::deque<double> true_range_history;
    
public:
    ATR(filter::ATRParams params)
        : IncrementalIndicator<double>("ATR" + std::string(params.useLog ? "LOG" : "") + "_" + std::to_string(params.period), params.period * 2), 
        period(params.period), useLog(params.useLog) {}
    virtual double initialize_with_history(const std::vector<BasicCandle>& history) override;
    virtual double update(const BasicCandle& candle) override;
    virtual double get_value() const override;
};


inline double ATR::initialize_with_history(const std::vector<BasicCandle>& history)
{
    if (history.size() < static_cast<size_t>(period + 1)) {
        return 0.0;
    }
    
    std::vector<double> high_history;
    std::vector<double> low_history;
    std::vector<double> close_history;
    
    for (const auto& candle : history) {
        high_history.push_back(candle.high);
        low_history.push_back(candle.low);
        close_history.push_back(candle.close);
    }
    
    // Calculate True Range (TR) for the history
    std::vector<double> true_ranges;
    for (size_t i = 1; i < high_history.size(); ++i) {
        double high = high_history[i];
        double low = low_history[i];
        double prev_close = close_history[i-1];
        
        double tr = std::max({
            high - low,
            std::abs(high - prev_close),
            std::abs(low - prev_close)
        });
        true_ranges.push_back(tr);
    }
    // Calculate initial ATR as simple average of TR
    double sum = 0.0;
    for (size_t i = true_ranges.size() - period; i < true_ranges.size(); ++i) {
        sum += true_ranges[i];
    }
    current_atr = sum / period;
    previous_close = close_history.back();
    is_initialized = true;

    if (useLog)
        return std::log(1.0 + current_atr);

    return current_atr;
}

inline double ATR::update(const BasicCandle& candle)
{
    if (!is_initialized) {
        if (previous_close == 0.0) {
            previous_close = candle.close;
            return 0.0;
        }
        
        double tr = std::max({
            candle.high - candle.low,
            std::abs(candle.high - previous_close),
            std::abs(candle.low - previous_close)
        });
        
        true_range_history.push_back(tr);
        previous_close = candle.close;
        
        if (true_range_history.size() >= static_cast<size_t>(period)) {
            double sum = 0.0;
            for (const auto& tr : true_range_history) {
                sum += tr;
            }
            current_atr = sum / period;
            is_initialized = true;

            if (useLog)
                current_atr = std::log(1.0 + current_atr);

            return current_atr;
        }
        
        return 0.0;
    }
    
    // Calculate new True Range
    double tr = std::max({
        candle.high - candle.low,
        std::abs(candle.high - previous_close),
        std::abs(candle.low - previous_close)
    });
    
    // Update ATR using Wilder's smoothing method
    current_atr = ((current_atr * (period - 1)) + tr) / period;
    previous_close = candle.close;

    if (useLog)
        return std::log(1.0 + current_atr);
    
    return current_atr;
}

inline double ATR::get_value() const {
    if (useLog)
        return std::log(1.0 + current_atr);

    return current_atr;
}