#pragma once
#include "IncrementalIndicator.hpp"
#include <vector>
#include <deque>
#include <algorithm>
#include <utility>

/**
 * Stochastic Oscillator calculated incrementally
 */
class STOCH : public IncrementalIndicator<std::pair<double, double>> {
private:
    int fastk_period;
    int slowk_period;
    int slowd_period;
    
    std::deque<BasicCandle> candle_buffer;
    
    std::deque<double> raw_k_values;
    std::deque<double> k_values;
    std::deque<double> d_values;
    
    double current_k = 0.0;
    double current_d = 0.0;
    
public:
    STOCH(filter::StochasticParams params)
    : IncrementalIndicator<std::pair<double, double>>("STOCH_" + std::to_string(params.fastK) + "_" + std::to_string(params.slowK) + "_" + std::to_string(params.slowD), params.fastK + params.slowK + params.slowD + 1),
    fastk_period(params.fastK), slowk_period(params.slowK), slowd_period(params.slowD) {}

    std::optional<std::pair<double, double>> initialize_with_history(const std::vector<BasicCandle>& history) override;
    std::optional<std::pair<double, double>> update(const BasicCandle& candle) override;
    std::optional<std::pair<double, double>> get_value() const override;
};

inline std::optional<std::pair<double, double>> STOCH::initialize_with_history(const std::vector<BasicCandle>& history) {
    if (history.size() < static_cast<size_t>(fastk_period)) {
        return std::nullopt;
    }

    // Store candles for processing
    candle_buffer.clear();
    for (const auto& candle : history) {
        candle_buffer.push_back(candle);
    }

    // Calculate initial raw K values
    raw_k_values.clear();
    for (size_t i = 0; i <= candle_buffer.size() - fastk_period; ++i) {
        double period_high = candle_buffer[i].high;
        double period_low = candle_buffer[i].low;
        
        for (size_t j = i + 1; j < i + fastk_period; ++j) {
            period_high = std::max(period_high, candle_buffer[j].high);
            period_low = std::min(period_low, candle_buffer[j].low);
        }
        
        double close = candle_buffer[i + fastk_period - 1].close;
        
        double raw_k = 0.0;
        if (period_high > period_low) {
            raw_k = 100.0 * ((close - period_low) / (period_high - period_low));
        }
        
        raw_k_values.push_back(raw_k);
    }

    // Apply K smoothing
    k_values.clear();
    if (raw_k_values.size() >= static_cast<size_t>(slowk_period)) {
        for (size_t i = 0; i <= raw_k_values.size() - slowk_period; ++i) {
            double sum = 0.0;
            for (size_t j = i; j < i + slowk_period; ++j) {
                sum += raw_k_values[j];
            }
            double smooth_k = sum / slowk_period;
            k_values.push_back(smooth_k);
        }
    }

    // Calculate D values
    d_values.clear();
    if (k_values.size() >= static_cast<size_t>(slowd_period)) {
        for (size_t i = 0; i <= k_values.size() - slowd_period; ++i) {
            double sum = 0.0;
            for (size_t j = i; j < i + slowd_period; ++j) {
                sum += k_values[j];
            }
            double smooth_d = sum / slowd_period;
            d_values.push_back(smooth_d);
        }
    }

    if (!k_values.empty() && !d_values.empty()) {
        current_k = k_values.back();
        current_d = d_values.back();
        is_initialized = true;
    }

    return std::make_optional(std::make_pair(current_k, current_d));
}

inline std::optional<std::pair<double, double>> STOCH::update(const BasicCandle& candle) {
    // Add new candle to buffer
    candle_buffer.push_back(candle);

    if (candle_buffer.size() > static_cast<size_t>(fastk_period + slowk_period + slowd_period)) {
        candle_buffer.pop_front();
    }

    if (!is_initialized) {
        if (candle_buffer.size() < static_cast<size_t>(fastk_period + slowk_period + slowd_period))
            return std::nullopt;

        std::vector<BasicCandle> history(candle_buffer.begin(), candle_buffer.end());
        return initialize_with_history(history);
    }

    // Calculate new raw K value
    double period_high = candle_buffer[candle_buffer.size() - fastk_period].high;
    double period_low = candle_buffer[candle_buffer.size() - fastk_period].low;
    
    for (size_t i = candle_buffer.size() - fastk_period + 1; i < candle_buffer.size(); ++i) {
        period_high = std::max(period_high, candle_buffer[i].high);
        period_low = std::min(period_low, candle_buffer[i].low);
    }

    double raw_k = 0.0;
    if (period_high > period_low) {
        raw_k = 100.0 * ((candle.close - period_low) / (period_high - period_low));
    }

    raw_k_values.push_back(raw_k);
    if (raw_k_values.size() > static_cast<size_t>(fastk_period + slowk_period + slowd_period)) {
        raw_k_values.pop_front();
    }

    // Calculate new smoothed K value
    if (raw_k_values.size() >= static_cast<size_t>(slowk_period)) {
        double sum = 0.0;
        for (size_t i = raw_k_values.size() - slowk_period; i < raw_k_values.size(); ++i) {
            sum += raw_k_values[i];
        }
        double smooth_k = sum / slowk_period;
        k_values.push_back(smooth_k);
        
        if (k_values.size() > static_cast<size_t>(fastk_period + slowk_period)) {
            k_values.pop_front();
        }
    }

    // Calculate new D value
    if (k_values.size() >= static_cast<size_t>(slowd_period)) {
        double sum = 0.0;
        for (size_t i = k_values.size() - slowd_period; i < k_values.size(); ++i) {
            sum += k_values[i];
        }
        double smooth_d = sum / slowd_period;
        d_values.push_back(smooth_d);
        
        if (d_values.size() > static_cast<size_t>(fastk_period + slowk_period)) {
            d_values.pop_front();
        }
    }

    // Update current values
    if (!k_values.empty() && !d_values.empty()) {
        current_k = k_values.back();
        current_d = d_values.back();
    }

    return std::make_optional(std::make_pair(current_k, current_d));
}

inline std::optional<std::pair<double, double>> STOCH::get_value() const {
    return std::make_optional(std::make_pair(current_k, current_d));
}