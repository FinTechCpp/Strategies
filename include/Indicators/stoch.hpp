#pragma once
#include "IncrementalIndicator.hpp"
#include <vector>
#include <deque>
#include <algorithm>
#include <utility>

/**
 * Stochastic Oscillator calculated incrementally
 * Optimized version with true incremental updates
 */
class STOCH : public IncrementalIndicator<std::pair<double, double>> {
private:
    const int fastk_period;
    const int slowk_period;
    const int slowd_period;
    
    // Candle buffer for the calculation of %K (only fastk_period needed)
    std::deque<BasicCandle> candle_buffer;
    
    // Buffer of raw %K values for smoothing
    std::deque<double> raw_k_values;
    
    // Buffer of smoothed %K values for the calculation of %D
    std::deque<double> k_values;
    
    // Cumulative sums for incremental O(1) calculations
    double sum_raw_k = 0.0;  // Sum of the last raw_k_period values
    double sum_k = 0.0;      // Sum of the last slowd_period %K values
    
    double current_k = 0.0;
    double current_d = 0.0;
    
    // Helper method to calculate raw %K from a candle buffer
    double calculate_raw_k(const std::deque<BasicCandle>& buffer) const;
    
public:
    STOCH(filter::StochasticParams params)
        : IncrementalIndicator<std::pair<double, double>>(
              "STOCH_" + std::to_string(params.fastK) + "_" + 
              std::to_string(params.slowK) + "_" + std::to_string(params.slowD), 
              params.fastK + params.slowK + params.slowD - 2),
          fastk_period(params.fastK),
          slowk_period(params.slowK),
          slowd_period(params.slowD) {}

    std::optional<std::pair<double, double>> initialize_with_history(const std::vector<BasicCandle>& history) override;
    std::optional<std::pair<double, double>> update(const BasicCandle& candle) override;
    std::optional<std::pair<double, double>> get_value() const override;
};

// Helper method to calculate raw %K
inline double STOCH::calculate_raw_k(const std::deque<BasicCandle>& buffer) const {
    if (buffer.size() < static_cast<size_t>(fastk_period)) {
        return 0.0;
    }
    
    // Find the highest high and lowest low over the period
    double period_high = buffer[0].high;
    double period_low = buffer[0].low;
    
    for (size_t i = 1; i < fastk_period; ++i) {
        period_high = std::max(period_high, buffer[i].high);
        period_low = std::min(period_low, buffer[i].low);
    }
    
    double close = buffer[fastk_period - 1].close;
    
    // Calculate raw %K
    if (period_high > period_low) {
        return 100.0 * ((close - period_low) / (period_high - period_low));
    }
    return 0.0;
}

inline std::optional<std::pair<double, double>> STOCH::initialize_with_history(const std::vector<BasicCandle>& history) {
    const size_t min_required = fastk_period + slowk_period + slowd_period - 2;
    
    if (history.size() < min_required) {
        return std::nullopt;
    }

    // Reset all buffers
    candle_buffer.clear();
    raw_k_values.clear();
    k_values.clear();
    sum_raw_k = 0.0;
    sum_k = 0.0;

    // Phase 1: Calculate raw %K to have enough data for smoothing
    const size_t num_raw_k_needed = slowk_period + slowd_period - 1;
    
    for (size_t i = 0; i < num_raw_k_needed; ++i) {
        // Fill the buffer with fastk_period candles
        candle_buffer.clear();
        for (int j = 0; j < fastk_period; ++j) {
            candle_buffer.push_back(history[i + j]);
        }
        
        double raw_k = calculate_raw_k(candle_buffer);
        raw_k_values.push_back(raw_k);
    }

    // Phase 2: Calculate the first smoothed %K (SMA of raw_k over slowk_period)
    for (size_t i = 0; i <= num_raw_k_needed - slowk_period; ++i) {
        double sum = 0.0;
        for (int j = 0; j < slowk_period; ++j) {
            sum += raw_k_values[i + j];
        }
        double smooth_k = sum / slowk_period;
        k_values.push_back(smooth_k);
    }

    // Phase 3: Calculate the first %D (SMA of %K over slowd_period)
    if (k_values.size() >= static_cast<size_t>(slowd_period)) {
        sum_k = 0.0;
        for (int i = 0; i < slowd_period; ++i) {
            sum_k += k_values[k_values.size() - slowd_period + i];
        }
        current_d = sum_k / slowd_period;
        current_k = k_values.back();
    }

    // Prepare the candle buffer with the last fastk_period candles
    candle_buffer.clear();
    for (int i = 0; i < fastk_period; ++i) {
        candle_buffer.push_back(history[history.size() - fastk_period + i]);
    }

    // Prepare sum_raw_k with the last slowk_period values
    sum_raw_k = 0.0;
    for (int i = 0; i < slowk_period; ++i) {
        sum_raw_k += raw_k_values[raw_k_values.size() - slowk_period + i];
    }

    // Keep only the buffers necessary for future calculations
    while (raw_k_values.size() > static_cast<size_t>(slowk_period)) {
        raw_k_values.pop_front();
    }
    
    while (k_values.size() > static_cast<size_t>(slowd_period)) {
        k_values.pop_front();
    }

    is_initialized = true;
    return std::make_pair(current_k, current_d);
}

inline std::optional<std::pair<double, double>> STOCH::update(const BasicCandle& candle) {
    if (!is_initialized) {
        return std::nullopt;
    }

    // ===== Step 1: Incremental update of the candle buffer =====
    candle_buffer.push_back(candle);
    if (candle_buffer.size() > static_cast<size_t>(fastk_period)) {
        candle_buffer.pop_front();
    }

    // ===== Step 2: Calculate the new raw %K =====
    double new_raw_k = calculate_raw_k(candle_buffer);

    // ===== Step 3: Incremental update of smoothed %K (SMA) =====
    // Add the new raw_k
    raw_k_values.push_back(new_raw_k);
    sum_raw_k += new_raw_k;
    
    // Remove the old raw_k if the buffer is full
    if (raw_k_values.size() > static_cast<size_t>(slowk_period)) {
        sum_raw_k -= raw_k_values.front();
        raw_k_values.pop_front();
    }
    
    // Calculate the new smoothed %K (incremental simple moving average O(1))
    double new_k = sum_raw_k / slowk_period;
    current_k = new_k;

    // ===== Step 4: Incremental update of %D (SMA of %K) =====
    // Add the new %K
    k_values.push_back(new_k);
    sum_k += new_k;
    
    // Remove the old %K if the buffer is full
    if (k_values.size() > static_cast<size_t>(slowd_period)) {
        sum_k -= k_values.front();
        k_values.pop_front();
    }
    
    // Calculate the new %D (incremental simple moving average O(1))
    current_d = sum_k / slowd_period;

    return std::make_pair(current_k, current_d);
}

inline std::optional<std::pair<double, double>> STOCH::get_value() const {
    if (!is_initialized) {
        return std::nullopt;
    }
    return std::make_pair(current_k, current_d);
}