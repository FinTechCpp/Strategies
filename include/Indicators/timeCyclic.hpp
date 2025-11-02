#pragma once

#include "Indicators/IncrementalIndicator.hpp"
#include "common.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief Cyclic encoding of time (hour of the day) in sin/cos
 * 
 * Transforms the hour of the day into polar coordinates to capture
 * the cyclic nature of time (0h = 24h).
 * 
 * Formula:
 *   - sin_value = sin(2π * hour / 24)
 *   - cos_value = cos(2π * hour / 24)
 * 
 * Example of use in ML:
 *   - Allows the model to understand that 23h and 1h are close
 *   - Captures hourly patterns (volatility, volume, etc.)
 */
class TIMECYCLIC : public IncrementalIndicator<std::pair<double, double>> {
private:
    filter::TimeCyclicParams m_params;
    std::pair<double, double> m_currentValue; // {sin, cos}

    /**
     * @brief Extracts the hour in decimal from a DateTime
     * @param dt DateTime of the candle
     * @return Hour of the day in decimal (0.0 - 23.999...)
     */
    double getHourOfDay(const DateTime& dt) const {
        double hour = dt.time.hour;
        double minute = dt.time.minute;
        double second = dt.time.second;
        
        return hour + (minute / 60.0) + (second / 3600.0);
    }

    /**
     * @brief Calculates the sin/cos values for a given hour
     * @param hour Hour of the day (0-24)
     * @return {sin_value, cos_value}
     */
    std::pair<double, double> calculateCyclic(double hour) const {
        double angle = 2.0 * M_PI * hour / 24.0;
        return {std::sin(angle), std::cos(angle)};
    }

public:
    explicit TIMECYCLIC(const filter::TimeCyclicParams& params = filter::TimeCyclicParams())
        : IncrementalIndicator<std::pair<double, double>>("TIMECYCLIC", 1)
        , m_params(params)
        , m_currentValue({0.0, 0.0})
    {}

    // Implementation of the pure virtual methods of IncrementalIndicator
    std::optional<std::pair<double, double>> initialize_with_history(const std::vector<BasicCandle>& history) override {
        if (history.empty()) {
            return std::nullopt;
        }

        // Initialize with the last candle
        const auto& lastCandle = history.back();
        double hour = getHourOfDay(lastCandle.date);
        m_currentValue = calculateCyclic(hour);
        is_initialized = true;

        return m_currentValue;
    }

    std::optional<std::pair<double, double>> update(const BasicCandle& candle) override {
        double hour = getHourOfDay(candle.date);
        m_currentValue = calculateCyclic(hour);
        is_initialized = true;
        return m_currentValue;
    }

    std::optional<std::pair<double, double>> get_value() const override {
        if (!is_initialized) {
            return std::nullopt;
        }
        return m_currentValue;
    }

    // Individual access methods to facilitate usage
    double getSinValue() const {
        return m_currentValue.first;
    }

    double getCosValue() const {
        return m_currentValue.second;
    }
};
