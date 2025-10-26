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
    const int period;
    const bool useLog = false;
    double current_atr = 0.0;
    double previous_close = 0.0;
    
public:
    ATR(filter::ATRParams params)
        : IncrementalIndicator<double>("ATR" + std::string(params.useLog ? "LOG" : "") + "_" + std::to_string(params.period), params.period * 2)
        , period(params.period)
        , useLog(params.useLog) {

    }

    virtual std::optional<double> initialize_with_history(const std::vector<BasicCandle>& history) override;
    virtual std::optional<double> update(const BasicCandle& candle) override;
    virtual std::optional<double> get_value() const override;
};


inline std::optional<double> ATR::initialize_with_history(const std::vector<BasicCandle>& history)
{
    if (history.size() < static_cast<size_t>(period + 1))
        return std::nullopt;
    
    // Calculate True Range (TR) for the history
    std::vector<double> true_ranges;
    for (size_t i = 1; i < history.size(); ++i) {
        double high = history[i].high;
        double low = history[i].low;
        double prev_close = history[i-1].close;
        
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
    previous_close = history.back().close;
    is_initialized = true;

    if (useLog)
        return std::log(1.0 + current_atr);

    return current_atr;
}

inline std::optional<double> ATR::update(const BasicCandle& candle)
{
    if (!is_initialized)
        return std::nullopt;
    
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

inline std::optional<double> ATR::get_value() const {
    if (!is_initialized)
        return std::nullopt;
        
    if (useLog)
        return std::log(1.0 + current_atr);

    return current_atr;
}