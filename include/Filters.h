#pragma once

#include <string>
#include <memory>
#include <vector>
#include "Managers/LoggerManager.hpp"
#include "Managers/CandleManager.hpp"

class Filters {
public:
    // Filtre pour prix > EMA
    static bool priceSupEMA(double price, double ema_value, const std::string& name, ILogger* logger);
    static bool priceInfEMA(double price, double ema_value, const std::string& name, ILogger* logger);

    // Filtre Stochastique
    static bool stochInfThreshold(const std::vector<std::pair<double, double>>& kd_values, int threshold, const std::string& name, ILogger* logger);
    static bool stochAboveThreshold(const std::vector<std::pair<double, double>>& kd_values, int threshold, const std::string& name, ILogger* logger);
    
    // Filtre RSI sous un seuil (avec mise à jour de l'historique)
    static bool rsiInfThreshold(const std::vector<double>& rsi_values, int threshold, const std::string& name, ILogger* logger);
    static bool rsiAboveThreshold(const std::vector<double>& rsi_values, int threshold, const std::string& name, ILogger* logger);
    
    // Filtre pour vérifier si n bougies Heikin-Ashi précédentes sont rouges
    static bool previousHACandlesRed(const CandleManager& candleManager, int n_previous_candles, const std::string& name, ILogger* logger);
    static bool previousHACandlesGreen(const CandleManager& candleManager, int n_previous_candles, const std::string& name, ILogger* logger);
    
    // Filtre SuperTrend pour vérifier si le prix est au-dessus de la bande SuperTrend
    static bool priceSupSupertrend(double price, std::pair<double, int> supertrend_values, const std::string& name, ILogger* logger);
    static bool priceInfSupertrend(double price, std::pair<double, int> supertrend_values, const std::string& name, ILogger* logger);

private:
    static bool priceCompareEMA(double price, double ema_value, const std::string& name, ILogger* logger, bool passIfSuperior);
    static bool stochCompareThreshold(const std::vector<std::pair<double, double>>& kd_values, int threshold, const std::string& name, ILogger* logger, bool passIfSuperior);
    static bool rsiCompareThreshold(const std::vector<double>& rsi_values, int threshold, const std::string& name, ILogger* logger, bool passIfSuperior);
    static bool priceCompareSupertrend(double price, std::pair<double, int> supertrend_values, const std::string& name, ILogger* logger, bool passIfSuperior);
    static bool previousHACandles(const CandleManager& candleManager, int n_previous_candles, const std::string& name, ILogger* logger, bool passIfGreen);
};