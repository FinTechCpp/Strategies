#pragma once

#include "common.h"
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <memory>

/**
 * @brief Circular buffer-based candle manager
 * 
 * Uses a circular buffer to efficiently store a fixed number of candles
 * without any reallocation or cleanup operations. Provides O(1) access
 * to the most recent candles.
 */
class CandleManager {
private:
    // Circular buffers to store candles
    std::vector<BasicCandle> candle_buffer;
    std::vector<BasicCandle> ha_buffer;
    
    // Buffer capacity (fixed size)
    size_t buffer_capacity = 10;
    
    // Current number of candles stored
    size_t candle_count = 0;
    
    // Write position in circular buffer
    size_t write_index = 0;
    
    /**
     * @brief Get the actual index in the circular buffer for a logical index
     * @param logical_index Index from 0 (oldest) to size()-1 (newest)
     * @return Actual index in the circular buffer
     */
    inline size_t get_circular_index(size_t logical_index) const {
        if (logical_index >= candle_count) {
            throw std::out_of_range("Index out of range");
        }
        
        // If buffer is not full yet, simple indexing
        if (candle_count < buffer_capacity) {
            return logical_index;
        }
        
        // Full circular buffer: calculate from write position
        size_t oldest_index = write_index;
        return (oldest_index + logical_index) % buffer_capacity;
    }
    
    /**
     * @brief Get the index of the most recent candle
     */
    inline size_t get_latest_index() const {
        if (candle_count == 0) {
            throw std::runtime_error("No candles available");
        }
        
        // Latest is always one position before write_index
        return (write_index + buffer_capacity - 1) % buffer_capacity;
    }

    // Add Heikin-Ashi candle incrementally
    void add_heikin_ashi_candle(const BasicCandle& current, size_t index) {
        // If it's the first candle, it's identical to the normal candle
        if (candle_count == 1) {
            ha_buffer[index] = current;
            return;
        }

        // Get the previous HA candle index
        size_t prev_index = (index + buffer_capacity - 1) % buffer_capacity;
        const BasicCandle& prev_ha = ha_buffer[prev_index];

        // Calculate the new HA candle
        double ha_close = (current.open + current.high + current.low + current.close) / 4.0;
        double ha_open = (prev_ha.open + prev_ha.close) / 2.0;
        double ha_high = std::max({current.high, ha_open, ha_close});
        double ha_low = std::min({current.low, ha_open, ha_close});

        // Store the new HA candle at the given index
        ha_buffer[index] = BasicCandle(current.date, ha_open, ha_high, ha_low, ha_close);
    }

public:
    /**
     * @brief Constructor with default minimal buffer size
     * @param minimal_size Minimum number of candles to store (default: 10)
     */
    CandleManager(size_t minimal_size = 10) 
        : buffer_capacity(std::max(size_t(10), minimal_size)) {
        // Pre-allocate buffers to avoid reallocations
        candle_buffer.resize(buffer_capacity);
        ha_buffer.resize(buffer_capacity);
    }

    /**
     * @brief Add a new candle to the circular buffer
     * @param candle The candle to add
     */
    void add_candle(const BasicCandle& candle) {
        // Store the new candle at the current write position
        candle_buffer[write_index] = candle;

        // Calculate and store the Heikin-Ashi candle
        add_heikin_ashi_candle(candle, write_index);

        // Increment write index (circular)
        write_index = (write_index + 1) % buffer_capacity;
        
        // Increment count (saturates at buffer_capacity)
        if (candle_count < buffer_capacity) {
            candle_count++;
        }
    }

    /**
     * @brief Get the latest candle
     * @return The most recent candle
     */
    BasicCandle get_latest_candle() const {
        if (candle_count == 0) {
            throw std::runtime_error("No candles available");
        }
        return candle_buffer[get_latest_index()];
    }

    /**
     * @brief Get the last N candles
     * @param n Number of candles to retrieve
     * @return Vector of the N most recent candles (oldest first)
     */
    std::vector<BasicCandle> get_last_candles(size_t n) const {
        if (candle_count == 0) {
            return {};
        }
        
        size_t count = std::min(n, candle_count);
        std::vector<BasicCandle> result;
        result.reserve(count);
        
        // Start from oldest candle in the requested range
        size_t start_logical = candle_count - count;
        for (size_t i = 0; i < count; ++i) {
            size_t index = get_circular_index(start_logical + i);
            result.push_back(candle_buffer[index]);
        }
        
        return result;
    }

    /**
     * @brief Get the latest Heikin-Ashi candle
     * @return The most recent Heikin-Ashi candle
     */
    BasicCandle get_latest_heikin_ashi() const {
        if (candle_count == 0) {
            throw std::runtime_error("No Heikin-Ashi candles available");
        }
        return ha_buffer[get_latest_index()];
    }

    /**
     * @brief Get the last N Heikin-Ashi candles
     * @param n Number of candles to retrieve
     * @return Vector of the N most recent HA candles (oldest first)
     */
    std::vector<BasicCandle> get_last_heikin_ashi_candles(size_t n) const {
        if (candle_count == 0) 
            return {};
        
        size_t count = std::min(n, candle_count);
        std::vector<BasicCandle> result;
        result.reserve(count);
        
        // Start from oldest candle in the requested range
        size_t start_logical = candle_count - count;
        for (size_t i = 0; i < count; ++i) {
            size_t index = get_circular_index(start_logical + i);
            result.push_back(ha_buffer[index]);
        }
        
        return result;
    }

    // Check if a candle is green
    bool is_candle_green(const BasicCandle& candle) const {
        return candle.close > candle.open;
    }

    // Check if the latest candle is green
    bool is_latest_candle_green() const {
        if (candle_count == 0) {
            return false;
        }
        return is_candle_green(candle_buffer[get_latest_index()]);
    }

    // Check if the latest Heikin-Ashi candle is green
    bool is_latest_heikin_ashi_green() const {
        if (candle_count == 0) {
            return false;
        }
        return is_candle_green(ha_buffer[get_latest_index()]);
    }

    // Check if the latest Heikin-Ashi candle is red
    bool is_latest_heikin_ashi_red() const {
        if (candle_count == 0) {
            return false;
        }
        return !is_candle_green(ha_buffer[get_latest_index()]);
    }
    
    // Remove all candles and Heikin-Ashi candles
    void clear() {
        candle_count = 0;
        write_index = 0;
    }

    // Get the total number of candles
    size_t size() const {
        return candle_count;
    }

    // Compatibility methods for Strategy

    // Access a candle by index (0 = oldest, size()-1 = newest)
    BasicCandle at(size_t logical_index) const {
        return candle_buffer.at(get_circular_index(logical_index));
    }

    // Access a candle by index (0 = oldest, size()-1 = newest)
    BasicCandle operator[](size_t logical_index) const {
        return candle_buffer[get_circular_index(logical_index)];
    }

    /**
     * @brief Set the minimal buffer size required for calculations
     * 
     * This method resizes the circular buffer to accommodate at least
     * the specified number of candles. Existing data is preserved when
     * expanding the buffer.
     * 
     * @param minimal_size The minimum number of candles needed for calculations
     */
    void setMinimalBufferSize(size_t minimal_size) {
        size_t new_capacity = std::max(size_t(10), minimal_size);
        
        // If already sufficient, nothing to do
        if (new_capacity <= buffer_capacity) {
            return;
        }
        
        // Create new larger buffers
        std::vector<BasicCandle> new_candle_buffer(new_capacity);
        std::vector<BasicCandle> new_ha_buffer(new_capacity);
        
        // Copy existing data in correct order
        if (candle_count > 0) {
            for (size_t i = 0; i < candle_count; ++i) {
                size_t old_index = get_circular_index(i);
                new_candle_buffer[i] = candle_buffer[old_index];
                new_ha_buffer[i] = ha_buffer[old_index];
            }
        }
        
        // Replace buffers
        candle_buffer = std::move(new_candle_buffer);
        ha_buffer = std::move(new_ha_buffer);
        buffer_capacity = new_capacity;
        write_index = candle_count % buffer_capacity;
    }
};