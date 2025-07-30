#pragma once
#include "IncrementalIndicator.hpp"
#include "atr.hpp"
#include <vector>
#include <memory>
#include <cmath>

/**
 * Supertrend indicator calculated incrementally
 * Returns the Supertrend value and direction
 */
class SUPERTREND : public IncrementalIndicator<std::pair<double, int>> {
private:
    int atr_period;
    double multiplier;
    std::unique_ptr<ATR> atr_calculator;
    
    // Current state
    double current_supertrend = 0.0;
    int current_direction = 0;  // 1 = uptrend, -1 = downtrend
    
    // Previous values for calculation
    double prev_close = 0.0;
    double prev_final_upper_band = 0.0;
    double prev_final_lower_band = 0.0;
    double prev_supertrend = 0.0;
    int prev_direction = 0;
    
    // Helper function to calculate HL2 (median price)
    double calculate_hl2(const BasicCandle& candle) const {
        return (candle.high + candle.low) / 2.0;
    }
    
public:
    SUPERTREND(int atr_period, double multiplier, const std::string& name = "")
        : IncrementalIndicator<std::pair<double, int>>(name.empty() ?
            "SUPERTREND_" + std::to_string(atr_period) + "_" + std::to_string(multiplier) : name),
          atr_period(atr_period), multiplier(multiplier) {
        atr_calculator = std::make_unique<ATR>(atr_period);
    }

    std::pair<double, int> initialize_with_history(const std::vector<BasicCandle>& history) override;
    std::pair<double, int> update(const BasicCandle& candle) override;
    std::pair<double, int> get_value() const override;
    
    // Convenience methods to get individual values
    double get_supertrend() const { return current_supertrend; }
    int get_direction() const { return current_direction; }
    bool is_uptrend() const { return current_direction == 1; }
    bool is_downtrend() const { return current_direction == -1; }
};

inline std::pair<double, int> SUPERTREND::initialize_with_history(const std::vector<BasicCandle>& history) {
    if (history.size() < static_cast<size_t>(atr_period + 1)) {
        return {0.0, 0};
    }
    
    // Initialize ATR first
    atr_calculator->initialize_with_history(history);
    
    // Calculate Supertrend for the historical data
    std::vector<double> basic_upper_bands;
    std::vector<double> basic_lower_bands;
    std::vector<double> final_upper_bands;
    std::vector<double> final_lower_bands;
    
    basic_upper_bands.resize(history.size());
    basic_lower_bands.resize(history.size());
    final_upper_bands.resize(history.size());
    final_lower_bands.resize(history.size());
    
    // Calculate basic bands and final bands
    for (size_t i = atr_period; i < history.size(); ++i) {
        double hl2 = calculate_hl2(history[i]);
        
        // Get ATR value by simulating the calculation up to this point
        ATR temp_atr(atr_period);
        std::vector<BasicCandle> temp_history(history.begin(), history.begin() + i + 1);
        double atr_value = temp_atr.initialize_with_history(temp_history);
        
        // Calculate basic bands
        basic_upper_bands[i] = hl2 + (multiplier * atr_value);
        basic_lower_bands[i] = hl2 - (multiplier * atr_value);
        
        // Calculate final bands
        if (i == atr_period) {
            // First calculation
            final_upper_bands[i] = basic_upper_bands[i];
            final_lower_bands[i] = basic_lower_bands[i];
        } else {
            // Final upper band: only goes down if previous close was above it
            final_upper_bands[i] = (basic_upper_bands[i] < final_upper_bands[i-1] || 
                                   history[i-1].close > final_upper_bands[i-1]) 
                                  ? basic_upper_bands[i] 
                                  : final_upper_bands[i-1];
            
            // Final lower band: only goes up if previous close was below it
            final_lower_bands[i] = (basic_lower_bands[i] > final_lower_bands[i-1] || 
                                   history[i-1].close < final_lower_bands[i-1]) 
                                  ? basic_lower_bands[i] 
                                  : final_lower_bands[i-1];
        }
    }
    
    // Calculate final Supertrend and direction
    size_t last_index = history.size() - 1;
    
    if (last_index < atr_period) {
        current_supertrend = 0.0;
        current_direction = 0;
        is_initialized = false;
        return {0.0, 0};
    }
    
    // Determine initial trend direction
    if (last_index == atr_period) {
        // First supertrend calculation
        if (history[last_index].close <= final_upper_bands[last_index]) {
            current_supertrend = final_upper_bands[last_index];
            current_direction = -1; // Downtrend
        } else {
            current_supertrend = final_lower_bands[last_index];
            current_direction = 1;  // Uptrend
        }
    } else {
        // Calculate trend changes
        int prev_trend = current_direction;
        
        // Simulate the trend calculation through history
        double temp_supertrend = final_upper_bands[atr_period];
        int temp_direction = (history[atr_period].close <= final_upper_bands[atr_period]) ? -1 : 1;
        
        for (size_t i = atr_period + 1; i <= last_index; ++i) {
            int prev_temp_direction = temp_direction;
            
            if (prev_temp_direction == 1) { // Previous uptrend
                if (history[i].close < final_lower_bands[i]) {
                    // Change to downtrend
                    temp_supertrend = final_upper_bands[i];
                    temp_direction = -1;
                } else {
                    // Continue uptrend
                    temp_supertrend = final_lower_bands[i];
                    temp_direction = 1;
                }
            } else { // Previous downtrend
                if (history[i].close > final_upper_bands[i]) {
                    // Change to uptrend
                    temp_supertrend = final_lower_bands[i];
                    temp_direction = 1;
                } else {
                    // Continue downtrend
                    temp_supertrend = final_upper_bands[i];
                    temp_direction = -1;
                }
            }
        }
        
        current_supertrend = temp_supertrend;
        current_direction = temp_direction;
    }
    
    // Store previous values for future updates
    prev_close = history[last_index].close;
    prev_final_upper_band = final_upper_bands[last_index];
    prev_final_lower_band = final_lower_bands[last_index];
    prev_supertrend = current_supertrend;
    prev_direction = current_direction;
    
    is_initialized = true;
    return {current_supertrend, current_direction};
}

inline std::pair<double, int> SUPERTREND::update(const BasicCandle& candle) {
    if (!is_initialized) {
        return {0.0, 0};
    }
    
    // Update ATR
    double atr_value = atr_calculator->update(candle);
    
    // Calculate basic bands
    double hl2 = calculate_hl2(candle);
    double basic_upper_band = hl2 + (multiplier * atr_value);
    double basic_lower_band = hl2 - (multiplier * atr_value);
    
    // Calculate final bands
    double final_upper_band = (basic_upper_band < prev_final_upper_band || 
                              prev_close > prev_final_upper_band) 
                             ? basic_upper_band 
                             : prev_final_upper_band;
    
    double final_lower_band = (basic_lower_band > prev_final_lower_band || 
                              prev_close < prev_final_lower_band) 
                             ? basic_lower_band 
                             : prev_final_lower_band;
    
    // Calculate Supertrend and direction
    if (prev_direction == 1) { // Previous uptrend
        if (candle.close < final_lower_band) {
            // Change to downtrend
            current_supertrend = final_upper_band;
            current_direction = -1;
        } else {
            // Continue uptrend
            current_supertrend = final_lower_band;
            current_direction = 1;
        }
    } else { // Previous downtrend
        if (candle.close > final_upper_band) {
            // Change to uptrend
            current_supertrend = final_lower_band;
            current_direction = 1;
        } else {
            // Continue downtrend
            current_supertrend = final_upper_band;
            current_direction = -1;
        }
    }
    
    // Update previous values for next iteration
    prev_close = candle.close;
    prev_final_upper_band = final_upper_band;
    prev_final_lower_band = final_lower_band;
    prev_supertrend = current_supertrend;
    prev_direction = current_direction;
    
    return {current_supertrend, current_direction};
}

inline std::pair<double, int> SUPERTREND::get_value() const {
    return {current_supertrend, current_direction};
}
