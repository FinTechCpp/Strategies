#pragma once

#include <string>
#include <memory>
#include <vector>
#include "Managers/LoggerManager.hpp"
#include "Managers/CandleManager.hpp"

class Filters {
public:
    // Filters for EMA
    static bool priceSupEMA(double price, double ema_value, const std::string& name, ILogger* logger);
    static bool priceInfEMA(double price, double ema_value, const std::string& name, ILogger* logger);

    // Stochastic filter
    static bool stochInfThreshold(const std::vector<std::pair<double, double>>& kd_values, int threshold, const std::string& name, ILogger* logger);
    static bool stochAboveThreshold(const std::vector<std::pair<double, double>>& kd_values, int threshold, const std::string& name, ILogger* logger);

    // RSI filter below a threshold (with history)
    static bool rsiInfThreshold(const std::vector<double>& rsi_values, int threshold, const std::string& name, ILogger* logger);
    static bool rsiAboveThreshold(const std::vector<double>& rsi_values, int threshold, const std::string& name, ILogger* logger);

    // Filter to check if n previous Heikin-Ashi candles are red
    static bool previousHACandlesRed(const CandleManager& candleManager, size_t n_previous_candles, size_t offset, const std::string& name, ILogger* logger);
    static bool previousHACandlesGreen(const CandleManager& candleManager, size_t n_previous_candles, size_t offset, const std::string& name, ILogger* logger);

    // SuperTrend filter to check if the price is above the SuperTrend band
    static bool priceSupSupertrend(double price, std::pair<double, int> supertrend_values, const std::string& name, ILogger* logger);
    static bool priceInfSupertrend(double price, std::pair<double, int> supertrend_values, const std::string& name, ILogger* logger);

    // ATR filter to check if the ATR is above a threshold (with history)
    static bool atrAboveThreshold(const std::vector<double>& atr_values, double threshold, const std::string& name, ILogger* logger);

private:
    static bool priceCompareEMA(double price, double ema_value, const std::string& name, ILogger* logger, bool passIfSuperior);
    static bool stochCompareThreshold(const std::vector<std::pair<double, double>>& kd_values, int threshold, const std::string& name, ILogger* logger, bool passIfSuperior);
    static bool rsiCompareThreshold(const std::vector<double>& rsi_values, int threshold, const std::string& name, ILogger* logger, bool passIfSuperior);
    static bool priceCompareSupertrend(double price, std::pair<double, int> supertrend_values, const std::string& name, ILogger* logger, bool passIfSuperior);
    static bool previousHACandles(const CandleManager& candleManager, size_t n_previous_candles, size_t offset, const std::string& name, ILogger* logger, bool passIfGreen);
    static bool atrCompareThreshold(const std::vector<double>& atr_values, double threshold, const std::string& name, ILogger* logger, bool passIfSuperior);
};