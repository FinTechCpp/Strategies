#pragma once
#include "IncrementalIndicator.hpp"
#include "atr.hpp"
#include <vector>
#include <cmath>
#include <memory>

/**
 * Average True Range (ATR) logarithmique - retourne log(ATR + 1)
 * Utilise la composition plutôt que l'héritage pour appliquer une transformation logarithmique
 */
class ATRLOG : public IncrementalIndicator<double> {
private:
    std::unique_ptr<ATR> atr_calculator;
    
public:
    ATRLOG(int period)
        : IncrementalIndicator<double>("ATRLOG_" + std::to_string(period), period * 2) {
        atr_calculator = std::make_unique<ATR>(period);
    }

    double initialize_with_history(const std::vector<BasicCandle>& history) override {
        double atr_value = atr_calculator->initialize_with_history(history);
        is_initialized = atr_calculator->initialized();
        return std::log(1.0 + atr_value);
    }

    double update(const BasicCandle& candle) override {
        double atr_value = atr_calculator->update(candle);
        is_initialized = atr_calculator->initialized();
        return std::log(1.0 + atr_value);
    }

    double get_value() const override {
        return std::log(1.0 + atr_calculator->get_value());
    }
};