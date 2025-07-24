#pragma once
#include "common.h"
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <memory>

class CandleManager {
private:
    // Simple buffer to store candles
    std::vector<BasicCandle> candle_buffer;
    
    // Cache for Heikin-Ashi candles
    std::vector<BasicCandle> ha_buffer;
    
    // Buffer management parameters
    size_t max_buffer_size = 200;      // Normal max size
    size_t hysteresis_threshold = 250; // Trigger threshold for cleaning
    size_t clean_target_size = 180;    // Target size after cleaning

    // Add Heikin-Ashi candle incrementally
    void add_heikin_ashi_candle(const BasicCandle& current) {
        // If it's the first candle, it's identical to the normal candle
        if (ha_buffer.empty()) {
            ha_buffer.push_back(current);
            return;
        }

        // Get the last HA candle
        const BasicCandle& prev_ha = ha_buffer.back();

        // Calculate the new HA candle
        double ha_close = (current.open + current.high + current.low + current.close) / 4.0;
        double ha_open = (prev_ha.open + prev_ha.close) / 2.0;
        double ha_high = std::max({current.high, ha_open, ha_close});
        double ha_low = std::min({current.low, ha_open, ha_close});

        // Create and add the new HA candle
        BasicCandle new_ha(current.date, ha_open, ha_high, ha_low, ha_close);
        ha_buffer.push_back(new_ha);
    }

    // Convert BasicCandle to Candle (for compatibility if needed)
    Candle to_candle(const BasicCandle& basic) const {
        Candle candle(basic.date, basic.open, basic.high, basic.low, basic.close);
        return candle;
    }

    // Check and clean buffers if necessary (hysteresis logic)
    void check_and_clean_buffers() {
        if (candle_buffer.size() > hysteresis_threshold) {
            // Calculate how many candles to remove
            size_t to_remove = candle_buffer.size() - clean_target_size;

            // Remove from the beginning of the buffer
            candle_buffer.erase(candle_buffer.begin(), candle_buffer.begin() + to_remove);

            // Align the HA buffer
            if (ha_buffer.size() > to_remove) {
                ha_buffer.erase(ha_buffer.begin(), ha_buffer.begin() + to_remove);
            } else {
                // If we removed more candles than there are in ha_buffer,
                // we need to recalculate the entire HA buffer
                recalculate_all_heikin_ashi();
            }
        }
    }

    // Recalculate the entire HA buffer (called only if necessary)
    void recalculate_all_heikin_ashi() {
        if (candle_buffer.empty()) {
            ha_buffer.clear();
            return;
        }
        
        ha_buffer.clear();
        ha_buffer.reserve(candle_buffer.size());

        // Calculate the next HA candles incrementally
        for (size_t i = 0; i < candle_buffer.size(); ++i) {
            add_heikin_ashi_candle(candle_buffer[i]);
        }
    }

public:
    // Constructor with buffer management parameters
    CandleManager(size_t max_size = 200, size_t threshold = 250, size_t target = 180) 
        : max_buffer_size(max_size), hysteresis_threshold(threshold), clean_target_size(target) {
            candle_buffer.reserve(max_buffer_size);
            ha_buffer.reserve(max_buffer_size);
        }

    // Add a new candle (BasicCandle)
    void add_candle(const BasicCandle& candle) {
        // New candle
        candle_buffer.push_back(candle);

        // Incremental calculation for the new candle
        add_heikin_ashi_candle(candle);

        // Check and clean buffers if necessary
        check_and_clean_buffers();
    }

    // Get the latest candle
    BasicCandle get_latest_candle() const {
        if (candle_buffer.empty()) {
            throw std::runtime_error("No candles available");
        }
        return candle_buffer.back();
    }

    // Get the last N candles
    std::vector<BasicCandle> get_last_candles(size_t n) const {
        if (candle_buffer.empty()) {
            return {};
        }
        
        size_t count = std::min(n, candle_buffer.size());
        
        return std::vector<BasicCandle>(
            candle_buffer.end() - count, 
            candle_buffer.end()
        );
    }

    // Get the latest Heikin-Ashi candle
    BasicCandle get_latest_heikin_ashi() const {
        if (ha_buffer.empty()) {
            throw std::runtime_error("No Heikin-Ashi candles available");
        }
        
        return ha_buffer.back();
    }

    // Get the last N Heikin-Ashi candles
    std::vector<BasicCandle> get_last_heikin_ashi_candles(size_t n) const {
        if (ha_buffer.empty()) 
            return {};
        
        size_t count = std::min(n, ha_buffer.size());
        
        return std::vector<BasicCandle>(
            ha_buffer.end() - count, 
            ha_buffer.end()
        );
    }

    // Check if a candle is green
    bool is_candle_green(const BasicCandle& candle) const {
        return candle.close > candle.open;
    }

    // Check if the latest candle is green
    bool is_latest_candle_green() const {
        if (candle_buffer.empty()) {
            return false;
        }
        return is_candle_green(candle_buffer.back());
    }

    // Check if the latest Heikin-Ashi candle is green
    bool is_latest_heikin_ashi_green() const {
        if (ha_buffer.empty()) {
            return false;
        }
        
        return is_candle_green(ha_buffer.back());
    }
    
    // Remove all candles and Heikin-Ashi candles
    void clear() {
        candle_buffer.clear();
        ha_buffer.clear();
    }

    // Get the total number of candles
    size_t size() const {
        return candle_buffer.size();
    }

    // Compatibility methods for Strategy

    // Access a candle by index
    BasicCandle at(size_t index) const {
        return candle_buffer.at(index);
    }

    // Access a candle by index
    BasicCandle operator[](size_t index) const {
        return candle_buffer[index];
    }

    // Configure buffer management parameters
    void set_buffer_params(size_t max_size, size_t threshold, size_t target) {
        max_buffer_size = max_size;
        hysteresis_threshold = threshold;
        clean_target_size = target;

        // Check if the new parameters require immediate cleaning
        check_and_clean_buffers();
    }
};