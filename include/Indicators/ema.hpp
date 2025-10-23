#pragma once
#include "IncrementalIndicator.hpp"
#include <vector>
#include <numeric>
#include <deque>
#include <cmath>

/**
 * Exponential Moving Average (EMA) calculated incrementally
 */
class EMA : public IncrementalIndicator<double> {
private:
    const int period;
    const double multiplier;
    double current_ema = 0.0;
    
public:
    EMA(filter::EMAParams params) 
        : IncrementalIndicator<double>("EMA_" + std::to_string(params.period), params.period)
        , period(params.period)
        , multiplier(2.0 / (params.period + 1.0)) {
    }

    std::optional<double> initialize_with_history(const std::vector<BasicCandle>& history) override;
    std::optional<double> update(const BasicCandle& candle) override;
    std::optional<double> get_value() const override;
};

inline std::optional<double> EMA::initialize_with_history(const std::vector<BasicCandle>& history) {
    if (history.size() < static_cast<size_t>(period)) {
        return std::nullopt;
    }

    // Calculate initial SMA directly from BasicCandle without copying
    double sum = 0.0;
    for (size_t i = history.size() - period; i < history.size(); ++i) {
        sum += history[i].close;
    }
    current_ema = sum / period;

    is_initialized = true;
    return current_ema;
}

inline std::optional<double> EMA::update(const BasicCandle& candle) {
    
    // Si l'indicateur n'est pas encore initialisé
    if (!is_initialized)
        return std::nullopt;
    
    // Calculate new EMA value
    current_ema = (candle.close - current_ema) * multiplier + current_ema;
    return current_ema;
}

inline std::optional<double> EMA::get_value() const {
    if (!is_initialized)
        return std::nullopt;

    return current_ema;
}