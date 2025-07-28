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

    // Filtre pour prix < EMA
    static bool priceInfEMA(double price, double ema_value, const std::string& name, ILogger* logger);

    // Filtre Stochastique
    static bool stochInfThreshold(double current_k, double k_previous, 
                                double k_previous_2, double k_previous_3, 
                                int threshold, const std::string& name, 
                                ILogger* logger);
    static bool stochAboveThreshold(double current_k, double k_previous, 
                                double k_previous_2, double k_previous_3, 
                                int threshold, const std::string& name, 
                                ILogger* logger);
    
    // Filtre RSI sous un seuil (avec mise à jour de l'historique)
    static bool rsiInfThreshold(double current_rsi, double& previous_rsi, 
                             double& previous_2_rsi, int threshold, 
                             const std::string& name, ILogger* logger);

    static bool rsiAboveThreshold(double current_rsi, double& previous_rsi, 
                             double& previous_2_rsi, int threshold, 
                             const std::string& name, ILogger* logger);
    
    // Filtre pour vérifier si n bougies Heikin-Ashi précédentes sont rouges
    static bool previousHACandlesRed(const CandleManager& candleManager, 
                                    int n_previous_candles, 
                                    const std::string& name, 
                                    ILogger* logger);

    static bool previousHACandlesGreen(const CandleManager& candleManager, 
                                    int n_previous_candles, 
                                    const std::string& name, 
                                    ILogger* logger);
    
    // Filtre SuperTrend pour vérifier si le prix est au-dessus de la bande SuperTrend
    static bool priceSupSupertrend(double price, double supertrend_value, 
                                int supertrend_direction, const std::string& name, 
                                ILogger* logger);
    
    // Filtre SuperTrend pour vérifier si le prix est en dessous de la bande SuperTrend
    static bool priceInfSupertrend(double price, double supertrend_value, 
                                int supertrend_direction, const std::string& name, 
                                ILogger* logger);
};